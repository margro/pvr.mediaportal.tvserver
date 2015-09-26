#pragma once
/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1335  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef __TIMERS_H
#define __TIMERS_H

#include "kodi/libXBMC_pvr.h"
#include <stdlib.h>
#include <string>
#include "DateTime.h"

namespace TvDatabase
{

// From MediaPortal: TvDatabase.ScheduleRecordingType
enum ScheduleRecordingType
{                                  // English MediaPortal GUI description:
  Once = 0,                        // Record once
  Daily = 1,                       // Record every day at this time
  Weekly = 2,                      // Record every week at this time
  EveryTimeOnThisChannel = 3,      // Record every time on this channel
  EveryTimeOnEveryChannel = 4,     // Record every time on every channel
  Weekends = 5,                    // Record Weekends
  WorkingDays = 6,                 // Record Weekdays
  WeeklyEveryTimeOnThisChannel = 7 // Weekly on this channel
};

// From MediaPortal: TvDatabase.KeepMethodType
enum KeepMethodType
{
  UntilSpaceNeeded = 0,
  UntilWatched = 1,
  TillDate = 2,
  Always = 3
};

};

const int cSecsInDay  = 86400;          ///> Amount of seconds in one day
const int cKodiTimerTypeOffset = (PVR_TIMER_TYPE_NONE + 1);     ///> Offset used to map the ScheduleRecordingType onto the iTimerType values

class cTimer
{
  public:
    cTimer();
    cTimer(const PVR_TIMER &timerinfo);
    virtual ~cTimer();

    void GetPVRtimerinfo(PVR_TIMER &tag);
    unsigned int Index(void) const { return m_index; }
    unsigned int Channel(void) const { return m_channel; }
    int Priority(void) { return Mepo2XBMCPriority(m_priority); }
    const char* Title(void) const { return m_title.c_str(); }
    const char* Dir(void) const { return m_directory.c_str(); }
    time_t StartTime(void) const;
    time_t EndTime(void) const;
    bool ParseLine(const char *s);
    int PreRecordInterval(void) const { return m_prerecordinterval; }
    int PostRecordInterval(void) const { return m_postrecordinterval; }
    int RepeatFlags() { return SchedRecType2RepeatFlags(m_schedtype); };
    bool Repeat() const { return (m_schedtype == TvDatabase::Once ? false : true); };
    bool Done() const { return m_done; };
    bool IsManual() const { return m_ismanual; };
    bool IsActive() const { return (m_canceled==MPTV::cUndefinedDate); };
    bool IsRecording() const { return m_isrecording; };
    TvDatabase::ScheduleRecordingType RepeatFlags2SchedRecType(int repeatflags);
    std::string AddScheduleCommand();
    std::string UpdateScheduleCommand();
    void SetScheduleRecordingType(TvDatabase::ScheduleRecordingType schedType);
    void SetKeepMethod(TvDatabase::KeepMethodType keepmethod);
    void SetPreRecordInterval(int minutes);
    void SetPostRecordInterval(int minutes);

  private:
    int SchedRecType2RepeatFlags(TvDatabase::ScheduleRecordingType schedtype);

    /**
     * @brief Convert a XBMC Lifetime value to MediaPortals keepMethod+keepDate settings
     * @param lifetime the XBMC lifetime value (in days) (following the VDR syntax)
     * Should be called after setting m_starttime !!
     */
    void SetKeepMethod(int lifetime);
    int GetLifetime(void);
    int XBMC2MepoPriority(int xbmcprio);
    int Mepo2XBMCPriority(int mepoprio);

    // MediaPortal database fields:
    unsigned int m_index;              ///> MediaPortal id_Schedule
    int          m_channel;            ///> MediaPortal idChannel
    TvDatabase::ScheduleRecordingType m_schedtype; ///> MediaPortal scheduleType
    std::string m_title;               ///> MediaPortal programName
    MPTV::CDateTime m_startTime;       ///> MediaPortal startTime
    MPTV::CDateTime m_endTime;         ///> MediaPortal endTime
    //                                      skipped: maxAirings field
    int         m_priority;            ///> MediaPortal priority (not the XBMC one!!!)
    std::string m_directory;           ///> MediaPortal directory
    //                                      skipped:  quality field
    TvDatabase::KeepMethodType m_keepmethod;       ///> MediaPortal keepMethod
    MPTV::CDateTime m_keepDate;        ///> MediaPortal keepDate
    int         m_prerecordinterval;   ///> MediaPortal preRecordInterval
    int         m_postrecordinterval;  ///> MediaPortal postRecordInterval
    MPTV::CDateTime m_canceled;        ///> MediaPortal canceled (date + time)
    //                                      skipped: recommendedCard
    bool        m_series;              ///> MediaPortal series
    //                                      skipped: idParentSchedule: not yet supported in XBMC

    // XBMC asks for these fields:
    bool        m_active;
    bool        m_done;
    bool        m_ismanual;
    bool        m_isrecording;

    unsigned int m_progid;             ///> MediaPortal Program ID
};

#endif //__TIMERS_H
