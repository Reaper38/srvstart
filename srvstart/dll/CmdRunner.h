// ============================================================================
//
// FILE        : CmdRunner.h
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : interface definition for exported class CmdRunner
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
// ============================================================================

// prevent multiple inclusion

#if !defined(__CMD_RUNNER_H__)
#define __CMD_RUNNER_H__

// ============================================================================
//
// IMPORT / EXPORT
//
// ============================================================================

//
// if SRVSTART_DLL_EXPORT is #defined, then we are building the DLL
//  and exporting the classes
//
// otherwise, we are building an executable which will link with the DLL at run-time
//
// -------------------------------------------------------------
// APART FROM THE DLL ITSELF,
//  ANY SOURCE FILE WHICH #includes THIS ONE SHOULD ENSURE THAT
//  SRVSTART_DLL_EXPORT is not #defined
// -------------------------------------------------------------
//

#ifdef SRVSTART_DLL_EXPORT
#define SRVSTART_DLL_API __declspec(dllexport)
#pragma message("exporting CmdRunner")

#else

#ifdef	SRVSTART_DLL_LOCAL
#pragma message("CmdRunner is local")
#define	SRVSTART_DLL_API

#else

#define SRVSTART_DLL_API __declspec(dllimport)
#pragma message("importing CmdRunner")

#endif
#endif

// ============================================================================
//
// PROJECT HEADER FILES
//
// ============================================================================
// system headers
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

// namespace header
#include "SrvStart.h"

// forward declarations
struct CmdRunnerData;

// ============================================================================
//
// NAMESPACE
//
// ============================================================================

// all the DLL classes are defined within the SrvStart namespace
namespace SrvStart {

// ============================================================================
//
// CmdRunner class
//
// ============================================================================
class SRVSTART_DLL_API CmdRunner
{
public:
	// public types
	typedef enum START_MODES { COMMAND_MODE, SERVICE_MODE, ANY_MODE,
								INSTALL_MODE, INSTALL_DESKTOP_MODE, REMOVE_MODE };
	typedef enum EXECUTION_PRIORITIES {HIGH_PRIORITY, IDLE_PRIORITY, NORMAL_PRIORITY, REAL_PRIORITY };
	typedef enum SHUTDOWN_METHODS { SHUTDOWN_BY_KILL, SHUTDOWN_BY_COMMAND, SHUTDOWN_BY_WINMESSAGE };

	// start
	void start() throw (SrvStartException);

	// startup and shutdown
	void setStartupCommand(const char *sc) throw (SrvStartException);
	void setShutdownCommand(const char *sc) throw (SrvStartException);
	void setWaitCommand(const char *wc) throw (SrvStartException);
	void addStartupCommandArgument(const char *arg) throw (SrvStartException);
	void setShutdownMethod(const SHUTDOWN_METHODS sm) throw (SrvStartException);

	char *getStartupCommand() const;
	char *getShutdownCommand() const;
	char *getWaitCommand() const;
	SHUTDOWN_METHODS getShutdownMethod() const;

	// properties
	void setDebugLevel(int dl);
	void setWaitInterval(int wi);
	void setExecutionPriority(EXECUTION_PRIORITIES ep);
	void setStartupDelay(int sd);
	void setStartupDirectory(const char *dir) throw (SrvStartException);

	char *getSrvName() const;
	int   getWaitInterval() const;
	int   getStartupDelay() const;
	char *getStartupDirectory() const;
	EXECUTION_PRIORITIES getExecutionPriority() const;

	// environment
	void addEnv(const char *nm,const char *val) throw (SrvStartException);

	// start profile
	void setStartMinimised(bool sm);
	void setStartInNewWindow(bool nw);
	bool getStartMinimised() const;
	bool getStartInNewWindow() const;

	// auto-restart
	void setAutoRestart(bool ar);
	void setAutoRestartInterval(int in);
	bool getAutoRestart() const;
	int  getAutoRestartInterval() const;

	// drive mappings
	void mapLocalDrive(const char driveLetter,const char *drivePath) throw (SrvStartException);
	void mapNetworkDrive(const char driveLetter,const char *networkPath) throw (SrvStartException);

	// constructor and destructor
	CmdRunner(START_MODES mode = COMMAND_MODE,char *nm = NULL) throw (SrvStartException);
	virtual ~CmdRunner();

public:	// stop callbacks - provided for use by ScmConnector ONLY

	// service stop callback variable
	bool stopCallbackVar;

	// service stop callback event
	HANDLE stopCallbackEvent;

	// service stop callback function
	static void stopCallbackFunction(void *thisObject);

private:	// member functions: internals
	// start the command
	void startCommand() throw (SrvStartException);

	// wait for command to start
	void waitForStartup() throw (SrvStartException);

	// watch command while it's running
	typedef enum WATCH_OUTCOMES { WATCH_COMMAND_COMPLETED, WATCH_COMMAND_WAS_STOPPED };
	WATCH_OUTCOMES watchCommand() throw (SrvStartException);

	// kill the command
	void killCommand() throw (SrvStartException);

private:	// data members - hidden data
	struct CmdRunnerData *cmdRunnerData;

};

} // namespace SrvStart

#endif // !defined(__CMD_RUNNER_H__)
