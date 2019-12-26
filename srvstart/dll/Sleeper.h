//=============================================================================
//
// FILE        : Sleeper.h
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : class definition for Sleeper class
//
//               Refer to services.htm for description and instructions for use.
//               You can obtain this file on the Web at
//
//                         http://www.nick.rozanski.com/services.htm
//
// MODIFICATION HISTORY
// --------------------
//
//  Refer to master header file SrvStart.h for full modification history.
//
// DISTRIBUTION
// ------------
// Copyright (C) 1998-2000 Nick Rozanski (Nick@Rozanski.com)
// Distributed under the terms of the GNU General Public License
//  as published by the Free Software Foundation
//  (675 Mass Ave, Cambridge, MA 02139, USA)
//
// SrvStart is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
//=============================================================================

// prevent multiple inclusion

#if !defined(__SLEEPER_H__)
#define __SLEEPER_H__


// ============================================================================
//
// NAMESPACE
//
// ============================================================================

// all the DLL classes are defined within the SrvStart namespace
namespace SrvStart {

// ============================================================================
//
// Sleeper class
//
// ============================================================================

class Sleeper
{
public:
	// sleep for the given number of seconds
	static void Sleep(int seconds,char *msg)
	{
		// create local wait event
		HANDLE hSleepEvent;
		hSleepEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

		// wait for given time
		LOGGER_LOG_DEBUG2("waiting %dms for %s ...",1000*seconds,msg)
		(void)WaitForSingleObject(hSleepEvent,1000*seconds);
		LOGGER_LOG_DEBUG2("... wait %dms for %s complete",1000*seconds,msg)

		// close handle
		CloseHandle(hSleepEvent);

	}

private:
	Sleeper(); // no constructor
};

} // namespace SrvStart

#endif // !defined(__SLEEPER_H__)
