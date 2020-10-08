/*
 *      Copyright (C) 2005-2013 Team Kodi
 *      https://kodi.tv
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *************************************************************************
 *  This file is a modified version from Team MediaPortal's
 *  TsReader DirectShow filter
 *  MediaPortal is a GPL'ed HTPC-Application
 *  Copyright (C) 2005-2012 Team MediaPortal
 *  http://www.team-mediaportal.com
 *
 * Changes compared to Team MediaPortal's version:
 * - Code cleanup for PVR addon usage
 * - Code refactoring for cross platform usage
 *************************************************************************
 *  This file originates from TSFileSource, a GPL directshow push
 *  source filter that provides an MPEG transport stream output.
 *  Copyright (C) 2005-2006 nate, bear
 *  http://forums.dvbowners.com/
 */

#include "DeMultiplexer.h"
#include "utils.h" //UNUSED()
#include <kodi/General.h>  //for kodi::Log
#include "TSReader.h"

#include <thread>

#define MAX_BUF_SIZE 8000
#define BUFFER_LENGTH 0x1000
#define READ_SIZE (1316*30)
#define INITIAL_READ_SIZE (READ_SIZE * 1024)

namespace MPTV
{
    CDeMultiplexer::CDeMultiplexer(CTsReader& filter)
        :m_filter(filter),
        m_LastDataFromRtsp(0),
        m_bEndOfFile(false),
        m_reader(NULL),
        m_iPatVersion(-1),
        m_ReqPatVersion(-1),
        m_WaitNewPatTmo(0),
        m_receivedPackets(0),
        m_bStarting(false),
        m_bAudioAtEof(false),
        m_bVideoAtEof(false),
        m_bGotNewChannel(false)
    {
        m_patParser.SetCallBack(this);
    }

    CDeMultiplexer::~CDeMultiplexer()
    {

    }

    /// Starts the demuxer
    /// This method will read the file until we found the pat/sdt
    /// with all the audio/video pids
    void CDeMultiplexer::Start()
    {
        //reset some values
        m_bStarting = true;
        m_receivedPackets = 0;
        m_bEndOfFile = false;
        m_iPatVersion = -1;
        m_ReqPatVersion = -1;
        unsigned long long m_Time = GetTickCount64();

        while ((GetTickCount64() - m_Time) < 5000 && m_bGotNewChannel == false)
        {
            size_t BytesRead = ReadFromFile();
            if (0 == BytesRead)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        m_bStarting = false;
    }

    void CDeMultiplexer::SetFileReader(FileReader* reader)
    {
        m_reader = reader;
    }

    /// This method reads the next READ_SIZE bytes from the file
    /// and processes the raw data
    /// When a TS packet has been discovered, OnTsPacket(byte* tsPacket) gets called
    //  which in its turn deals with the packet
    size_t CDeMultiplexer::ReadFromFile()
    {
        if (m_filter.IsSeeking())
            return 0;       // Ambass : to check

        std::lock_guard<std::mutex> lock(m_sectionRead);
        if (NULL == m_reader)
            return 0;

        byte buffer[READ_SIZE];
        size_t dwReadBytes = 0;

        // if we are playing a RTSP stream
        if (m_reader->IsBuffer())
        {
            // and, the current buffer holds data
            size_t nBytesToRead = m_reader->HasData();

            if (nBytesToRead > sizeof(buffer))
            {
                nBytesToRead = sizeof(buffer);
            }
            else
            {
                m_bAudioAtEof = true;
                m_bVideoAtEof = true;
            }

            if (nBytesToRead)
            {
              // then read raw data from the buffer
              if (SUCCEEDED(m_reader->Read(buffer, nBytesToRead, &dwReadBytes)))
              {
                if (dwReadBytes > 0)
                {
                  // yes, then process the raw data
                  //result=true;
                  OnRawData(buffer, dwReadBytes);
                  m_LastDataFromRtsp = GetTickCount64();
                }
              }
              else
              {
                kodi::Log(ADDON_LOG_DEBUG, "%s: Read failed...", __FUNCTION__);
              }
            }
            else
            {
                if (!m_filter.IsTimeShifting())
                {
                    kodi::Log(ADDON_LOG_DEBUG, "%s: endoffile... %llu", __FUNCTION__, GetTickCount64() - m_LastDataFromRtsp);
                    //set EOF flag and return
                    if (GetTickCount64() - m_LastDataFromRtsp > 2000 && m_filter.State() != State_Paused) // A bit crappy, but no better idea...
                    {
                        kodi::Log(ADDON_LOG_DEBUG, "%s: endoffile!", __FUNCTION__);
                        m_bEndOfFile = true;
                        return 0;
                    }
                }
            }
            return dwReadBytes;
        }
        else
        {
            // playing a local file.
            // read raw data from the file
            if (SUCCEEDED(m_reader->Read(buffer, sizeof(buffer), &dwReadBytes)))
            {
                if ((m_filter.IsTimeShifting()) && (dwReadBytes < sizeof(buffer)))
                {
                    m_bAudioAtEof = true;
                    m_bVideoAtEof = true;
                }

                if (dwReadBytes > 0)
                {
                    // succeeded, process data
                    OnRawData(buffer, (int)dwReadBytes);
                }
                else
                {
                    if (!m_filter.IsTimeShifting())
                    {
                        // set EOF flag and return
                        kodi::Log(ADDON_LOG_DEBUG, "%s: endoffile!", __FUNCTION__);
                        m_bEndOfFile = true;
                        return 0;
                    }
                }

                // and return
                return dwReadBytes;
            }
            else
            {
                kodi::Log(ADDON_LOG_DEBUG, "%s: Read failed...", __FUNCTION__);
            }
        }
        return 0;
    }

    /// This method gets called via ReadFile() when a new TS packet has been received
    /// if will :
    ///  - decode any new pat/pmt/sdt
    ///  - decode any audio/video packets and put the PES packets in the appropiate buffers
    void CDeMultiplexer::OnTsPacket(byte* tsPacket)
    {
        CTsHeader header(tsPacket);

        m_patParser.OnTsPacket(tsPacket);

        if (m_iPatVersion == -1)
        {
            // First Pat not found
            return;
        }

        // Wait for new PAT if required.
        if ((m_iPatVersion & 0x0F) != (m_ReqPatVersion & 0x0F))
        {
            if (m_ReqPatVersion == -1)
            {                                     // Now, unless channel change,
                m_ReqPatVersion = m_iPatVersion;    // Initialize Pat Request.
                m_WaitNewPatTmo = GetTickCount64();   // Now, unless channel change request,timeout will be always true.
            }
            if (GetTickCount64() < m_WaitNewPatTmo)
            {
                // Timeout not reached.
                return;
            }
        }
    }

    void CDeMultiplexer::RequestNewPat(void)
    {
      if (m_reader)
      {
        m_ReqPatVersion++;
        m_ReqPatVersion &= 0x0F;
        kodi::Log(ADDON_LOG_DEBUG, "Request new PAT = %d", m_ReqPatVersion);
        m_WaitNewPatTmo = GetTickCount64() + 10000;

        size_t dwBytesProcessed = 0;
        unsigned long long m_Time = GetTickCount64();

        m_bGotNewChannel = false;

        while ((GetTickCount64() - m_Time) < 5000 && m_bGotNewChannel == false)
        {
            size_t BytesRead = ReadFromFile();
            if (0 == BytesRead)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            dwBytesProcessed += BytesRead;
        }

        kodi::Log(ADDON_LOG_DEBUG, "Found a new channel after processing %li bytes. File position: %I64d\n", dwBytesProcessed, m_reader->GetFilePointer());
      }
    }

    /// This method gets called-back from the pat parser when a new PAT/PMT/SDT has been received
    /// In this method we check if any audio/video/subtitle pid or format has changed
    /// If not, we simply return
    /// If something has changed we ask the MP to rebuild the graph
    void CDeMultiplexer::OnNewChannel(CChannelInfo& UNUSED(info))
    {
        m_bGotNewChannel = true;
    }
}
