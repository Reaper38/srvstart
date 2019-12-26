// ============================================================================
//
// FILE        : CmdRunner.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : Implementation of exported class CmdRunner
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

// this is the "main" source file
#define	SRVSTART_DLL

// we are exporting the class
#define	SRVSTART_DLL_EXPORT

// ============================================================================
//
// PROJECT HEADER FILES
//
// ============================================================================

// system headers
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <direct.h>

// support headers
#include <logger.h>

// class headers
#include "Sleeper.h"
#include "StringSubstituter.h"
#include "ScmConnector.h"
#include "CmdRunner.h"

// ============================================================================
//
// NAMESPACE DECLARATIONS
//
// ============================================================================

using namespace SrvStart;

// ============================================================================
//
// CONSTANT DEFINITIONS
//
// ============================================================================

const char *DEFAULT_NAME			= "";
const char *DEFAULT_COMMAND			= "";

// ============================================================================
//
// LOCAL FUNCTION PROTOTYPES
//
// ============================================================================

void createProcess(char *command,bool wait,HANDLE &hProcess,DWORD *processId=0,void *env=0,
					char *cwd=0,DWORD creationFlags=NORMAL_PRIORITY_CLASS,
					STARTUPINFO *startupInfo=0,int waitInterval=1)
					throw(SrvStartException);
void waitForProcessToComplete(HANDLE &hProcess,int waitInterval=1) throw(SrvStartException);
typedef enum STARTED_PROCESS_STATUS { 
				PROCESS_STATUS_STILL_RUNNING,
				PROCESS_STATUS_EXIT_SUCCESS,
				PROCESS_STATUS_EXIT_FAILURE } ;
static STARTED_PROCESS_STATUS getProcessStatus(HANDLE hProcess) throw(SrvStartException);
BOOL CALLBACK sendCloseMessage(HWND hwnd,LPARAM lParam);

// ============================================================================
//
// LOCAL CLASSES
//
// ============================================================================

//
// CmdRunnerData holds the internal data used by the class
//

struct CmdRunnerData
{
	// identification
	char *srvName;

	// startup / shutdown
	CmdRunner::START_MODES startMode;
	char *startupCommand;
	char *startupDirectory;
	char *waitCommand;
	char *shutdownCommand;
	CmdRunner::SHUTDOWN_METHODS shutdownMethod;

	// characteristics
	int waitInterval;
	CmdRunner::EXECUTION_PRIORITIES executionPriority;
	int startupDelay;
	bool startMinimised;
	bool startInNewWindow;

	// auto-restart
	bool autoRestart;
	int  autoRestartInterval;

	// process
	HANDLE hCommandProcess;
	DWORD  dwProcessId;

	// ScmConnector
	ScmConnector *scmConnector;

	// 	StringSubstituter
	StringSubstituter stringSubstituter;

	// constructor / destructor
	CmdRunnerData()
	{
		LOGGER_LOG_DEBUG("CmdRunnerData::CmdRunnerData()")

		startMode         = CmdRunner::COMMAND_MODE;

		stringSubstituter.stringInit(srvName);
		stringSubstituter.stringInit(startupCommand);
		stringSubstituter.stringInit(startupDirectory);
		stringSubstituter.stringInit(waitCommand);
		stringSubstituter.stringInit(shutdownCommand);
		shutdownMethod = CmdRunner::SHUTDOWN_BY_KILL;

		waitInterval      = 1;
		executionPriority = CmdRunner::NORMAL_PRIORITY;
		startupDelay      = 0;
		startMinimised    = false;
		startInNewWindow  = false;

		autoRestart         = false;
		autoRestartInterval = 0;

		hCommandProcess = 0;

		scmConnector = 0;

	} ;
	
	virtual ~CmdRunnerData()
	{
		stringSubstituter.stringDelete(srvName);
		stringSubstituter.stringDelete(startupCommand);
		stringSubstituter.stringDelete(startupDirectory);
		stringSubstituter.stringDelete(waitCommand);
		stringSubstituter.stringDelete(shutdownCommand);
	} ;

} ;

// ============================================================================
//
// CODE MACROS
//
// ============================================================================

#define	SS_RETURNV(func)	LOGGER_LOG_DEBUG1("returning from '%s'",func) return;
#define	SS_RETURN(func,val)	LOGGER_LOG_DEBUG1("returning from '%s'",func) return val;
#define	CHECK_GOOD_STRING(mt,s)	\
	if(s==0) { THROW_SRVSTART_EXCEPTION(SRVSTART_EXCEPTION_INVALID_PARAMETER, "CmdRunner",mt) } \
	if((*s)=='\0') { THROW_SRVSTART_EXCEPTION(SRVSTART_EXCEPTION_INVALID_PARAMETER, "CmdRunner",mt) } \
	LOGGER_LOG_DEBUG2("%s(): good string '%s'",mt,s)

// ============================================================================
//
// PUBLIC MEMBER FUNCTIONS
//
// ============================================================================


// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::CmdRunner
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : constructor
//
// ARGUMENTS       : mode IN start mode (COMMAND_MODE, SERVICE_MODE, or ANY_MODE)
//                   nm   IN command/service name
//
// THROWS          : SrvStartException
//
// ============================================================================
CmdRunner::CmdRunner
(
	START_MODES  mode,
	char        *nm
)
throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("CmdRunner::CmdRunner()")

	// initialise
	stopCallbackVar   = false;
	stopCallbackEvent = 0;

	// allocate CmdRunner data
	cmdRunnerData = new CmdRunnerData;

	// start mode
	cmdRunnerData->startMode = mode;

	// service name
	cmdRunnerData->stringSubstituter.stringCopy(cmdRunnerData->srvName, (nm==0?DEFAULT_NAME:nm));
	LOGGER_LOG_DEBUG1("service name is '%s'",cmdRunnerData->srvName)

	// private member variables
	cmdRunnerData->stringSubstituter.stringCopy(cmdRunnerData->startupCommand,DEFAULT_COMMAND);
	cmdRunnerData->stringSubstituter.stringCopy(cmdRunnerData->shutdownCommand,"");
	cmdRunnerData->stringSubstituter.stringCopy(cmdRunnerData->waitCommand,"");

	switch(mode)
	{
		case COMMAND_MODE:
			LOGGER_LOG_DEBUG("CmdRunner::CmdRunner(): mode is COMMAND_MODE")
			break;
		case SERVICE_MODE:
			LOGGER_LOG_DEBUG("CmdRunner::CmdRunner(): mode is SERVICE_MODE")
			break;
		case ANY_MODE:
			LOGGER_LOG_DEBUG("CmdRunner::CmdRunner(): mode is ANY_MODE")
			break;
		default:
			LOGGER_LOG_ERROR1("CmdRunner::CmdRunner(): invalid start mode %d",mode)
			THROW_SRVSTART_EXCEPTION
				(SRVSTART_EXCEPTION_INVALID_PARAMETER,"CmdRunner","CmdRunner")
			break;
	}

	// try and connect to Service Control manager if requested
	if((mode==SERVICE_MODE)||(mode==ANY_MODE))
	{
		// create and initialise ScmConnector object
		LOGGER_LOG_DEBUG("about to create ScmConnector")
		bool wasAnyMode = (mode==ANY_MODE);
		cmdRunnerData->scmConnector = 
			new ScmConnector(cmdRunnerData->srvName,wasAnyMode);

		// what is the status of the ScmConnector?
		ScmConnector::SCM_STATUSES scmStatus = cmdRunnerData->scmConnector->getScmStatus();
		switch(scmStatus)
		{
			case ScmConnector::STATUS_MUST_START_AS_CONSOLE:
				if(wasAnyMode)
				{
					LOGGER_LOG_DEBUG("CmdRunner::CmdRunner(): mode was ANY_MODE, now COMMAND_MODE")
					cmdRunnerData->startMode = COMMAND_MODE;
				}
				break;

			case ScmConnector:: STATUS_STARTING:
			case ScmConnector:: STATUS_RUNNING:
			case ScmConnector:: STATUS_STOPPING:
			case ScmConnector:: STATUS_STOPPED:
				if(wasAnyMode)
				{
					LOGGER_LOG_DEBUG("CmdRunner::CmdRunner(): mode was ANY_MODE, now SERVICE_MODE")
					cmdRunnerData->startMode = SERVICE_MODE;
				}
				break;

			default:
				// unexpected status
				LOGGER_LOG_ERROR1("CmdRunner::CmdRunner(): unexpected SCM status %d",scmStatus)
				THROW_SRVSTART_EXCEPTION
					(SRVSTART_EXCEPTION_GENERAL_ERROR,"CmdRunner","CmdRunner")
				break;
		}
	}

	// for service mode, install the callbacks
	// note that only ONE of these callbacks is needed;
	//  the other two are included for illsutrative purposes only!

	if(cmdRunnerData->startMode==SERVICE_MODE)
	{
		// install service stop callback variable
		stopCallbackVar = false;
		cmdRunnerData->scmConnector->installStopCallback(&stopCallbackVar);
		LOGGER_LOG_DEBUG("installed callback variable")

		// service stop callback event
		stopCallbackEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
		if (stopCallbackEvent == NULL)
		{
			// failed to create wait event
			LOGGER_LOG_ERROR1("CmdRunner(): failed to create stop callback event, error=%d",GetLastError())
			THROW_SRVSTART_EXCEPTION
				(SRVSTART_EXCEPTION_GENERAL_ERROR,"CmdRunner","CmdRunner")
		}
		else
		{
			cmdRunnerData->scmConnector->installStopCallback(&stopCallbackEvent);
			LOGGER_LOG_DEBUG("installed callback event")
		}

		// install service stop callback function
		cmdRunnerData->scmConnector->installStopCallback(
			static_cast<ScmConnector::STOP_HANDLER_FUNCTION*>(stopCallbackFunction),
			static_cast<void*>(this));
		LOGGER_LOG_DEBUG("installed callback function")

	}

	// we are ready to have our properties set now
	SS_RETURNV("CmdRunner::CmdRunner")
}

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::start
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : this is the main CmdRunner function.  It runs the command,
//                   and returns when it has completed.  Note that all command
//                   properties must have been set before this function is
//                   called.
//
// THROWS          : SrvStartException
//
// ============================================================================
void CmdRunner::start() throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("start()")

	// make sure that the command has been set
	CHECK_GOOD_STRING("start",cmdRunnerData->startupCommand)

	// we are now ready to perform the required substitutions
	cmdRunnerData->stringSubstituter.stringSubstitute(cmdRunnerData->startupCommand);

	// also on startup directory etc if supplied
#define	_SUBSTITUTE(d) \
	if(d!=0) { if ((*d)!='\0') { cmdRunnerData->stringSubstituter.stringSubstitute(d); } }

	_SUBSTITUTE(cmdRunnerData->startupDirectory)
	_SUBSTITUTE(cmdRunnerData->waitCommand)
	_SUBSTITUTE(cmdRunnerData->shutdownCommand)

	// there are three cases to deal with

	// case 1: this is an ordinary command running in the same window

	if((cmdRunnerData->startMode == COMMAND_MODE)&&
		(!cmdRunnerData->startInNewWindow))
	{
		LOGGER_LOG_DEBUG("start(): ordinary command running in the same window")
		// first of all, change to the right directory
		if(_chdir(cmdRunnerData->startupDirectory))
		{
			// failed to change directory
			LOGGER_LOG_ERROR1("start(): failed to change to directory %s)",cmdRunnerData->startupDirectory)
			THROW_SRVSTART_EXCEPTION
				(SRVSTART_EXCEPTION_INVALID_PARAMETER,"CmdRunner","start")
		}
		// start the command using POSIX system()
		// there doesn't seem to be a Win32 API call for this!
		LOGGER_LOG_DEBUG1("running command '%s' using system()",cmdRunnerData->startupCommand)
		int rc = system(cmdRunnerData->startupCommand);
		// this is a blocking call, so just return now
		if(rc == 0)
		{
			// success
			SS_RETURNV("CmdRunner::start()")
		}
		else
		{
			LOGGER_LOG_ERROR2("start(): failed to start command %s using system() (rc = %d)",
									cmdRunnerData->startupCommand,rc)
			THROW_SRVSTART_EXCEPTION
				(SRVSTART_EXCEPTION_COMMAND_FAILED,"CmdRunner","start")
		}
	}

	// case 2: this is an ordinary command running in a separate window

	if(cmdRunnerData->startMode == COMMAND_MODE)
	{
		LOGGER_LOG_DEBUG("start(): ordinary command running in a separate window")
		// run it
		LOGGER_LOG_DEBUG("running command")
		startCommand();

		// watch the process (wait for it to complete)
		LOGGER_LOG_DEBUG("waiting for command to complete")
		watchCommand();

		// everything went ok - return
		SS_RETURNV("CmdRunner::start")

	}

	// case 3: this is a service
	LOGGER_LOG_DEBUG("start(): service")

	// if the service is set to auto-restart, we may have to loop
	bool stillLooping = true;

#define	CATCH_AND_NOTIFY \
	catch(...) { \
		cmdRunnerData->scmConnector->notifyScmStatus(ScmConnector::STATUS_STOPPING,true); \
		cmdRunnerData->scmConnector->notifyScmStatus(ScmConnector::STATUS_STOPPED,true); \
		throw; }

	while(stillLooping)
	{

		// run the command
		try { startCommand(); }
		CATCH_AND_NOTIFY

		// wait for the process to start up
		LOGGER_LOG_DEBUG("process is starting")
		try { waitForStartup(); }
		CATCH_AND_NOTIFY

		// it is running - notify the SCM
		LOGGER_LOG_DEBUG("process is running")
		try { cmdRunnerData->scmConnector->notifyScmStatus(ScmConnector::STATUS_RUNNING); }
		CATCH_AND_NOTIFY

		// watch the process (wait for it to finish or be stopped)
		LOGGER_LOG_DEBUG("waiting for process to finish")
		try
		{
			switch(watchCommand())
			{
				case WATCH_COMMAND_COMPLETED:
					// command completed on its own
					LOGGER_LOG_DEBUG("command completed on its own")
					if(cmdRunnerData->autoRestart)
					{
						// auto-restart has been set - is the service still running?
						LOGGER_LOG_DEBUG("auto-restart has been set")
						if(cmdRunnerData->scmConnector->getScmStatus()==ScmConnector::STATUS_RUNNING)
						{
							// yes, the service is still running - restart the program
							LOGGER_LOG_DEBUG("auto-restart has been set: will restart service program")
							if(cmdRunnerData->autoRestartInterval>0)
							{
								// sleep before restart
								Sleeper::Sleep(cmdRunnerData->autoRestartInterval,"restart service");
							}
							stillLooping = true;
						}
						else
						{
							// the service is not running (probably shutting down) - do not restart the program
							LOGGER_LOG_DEBUG("auto-restart has been set: not restarting service program since shutting down")
							stillLooping = false;
						}
					}
					else
					{
						// auto-restart has not been set - exit
						LOGGER_LOG_DEBUG("auto-restart has not been set - exiting")
						stillLooping = false;
					}
					break;

				case WATCH_COMMAND_WAS_STOPPED:
					// command was stopped by SCM (eg NET STOP)
					LOGGER_LOG_DEBUG("command was stopped by SCM - exiting")
					stillLooping = false;
					break;
			}

			if(!stillLooping)
			{
				// command has completed - notify SCM
				LOGGER_LOG_DEBUG("command has completed - service is shutting down")
				cmdRunnerData->scmConnector->notifyScmStatus(ScmConnector::STATUS_STOPPING,true);
				cmdRunnerData->scmConnector->notifyScmStatus(ScmConnector::STATUS_STOPPED);
			}

		}
		CATCH_AND_NOTIFY

	}

	// return
	SS_RETURNV("CmdRunner::start")

}

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::get|setStartupCommand
//                   CmdRunner::get|setShutdownCommand
//                   CmdRunner::get|setWaitCommand
//                   CmdRunner::addStartupCommandArgument
//                   CmdRunner::get|setShutdownMethod
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : set startup, shutdown and wait commands
//
// ARGUMENTS       : as below
//
// THROWS          : SrvStartException
//
// ============================================================================
void CmdRunner::setStartupCommand(const char *sc) throw (SrvStartException)
{
	CHECK_GOOD_STRING("setStartupCommand",sc)
	cmdRunnerData->stringSubstituter.stringCopy(cmdRunnerData->startupCommand,sc);
}
void CmdRunner::setShutdownCommand(const char *sc) throw (SrvStartException)
{
	CHECK_GOOD_STRING("setShutdownCommand",sc)
	cmdRunnerData->stringSubstituter.stringCopy(cmdRunnerData->shutdownCommand,sc);
	cmdRunnerData->shutdownMethod = SHUTDOWN_BY_COMMAND;
}
void CmdRunner::setWaitCommand(const char *wc) throw (SrvStartException)
{
	CHECK_GOOD_STRING("setWaitCommand",wc)
	cmdRunnerData->stringSubstituter.stringCopy(cmdRunnerData->waitCommand,wc);
}
void CmdRunner::addStartupCommandArgument(const char *arg) throw (SrvStartException)
{
	CHECK_GOOD_STRING("addStartupCommandArgument",arg)
	cmdRunnerData->stringSubstituter.stringAppend(cmdRunnerData->startupCommand,arg,true);
}
void CmdRunner::setShutdownMethod(const SHUTDOWN_METHODS sm) { cmdRunnerData->shutdownMethod = sm; }

char *CmdRunner::getStartupCommand() const { return cmdRunnerData->startupCommand; }
char *CmdRunner::getShutdownCommand() const { return cmdRunnerData->shutdownCommand; }
char *CmdRunner::getWaitCommand() const { return cmdRunnerData->waitCommand; }
CmdRunner::SHUTDOWN_METHODS CmdRunner::getShutdownMethod() const { return cmdRunnerData->shutdownMethod; }

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::mapLocalDrive
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : map a local drive letter (ie SUBST)
//
//                   note that this is a GLOBAL MAPPING which affects ALL PROCESSES
//                    (and requires appropriate permissions)
//
// ARGUMENTS       : driveLetter  IN  drive letter to map
//                   drivePath    IN  local path to map it to
//
// THROWS          : SrvStartException
//
// ============================================================================
void CmdRunner::mapLocalDrive
(
	const char  driveLetter,
	const char *drivePath
) throw (SrvStartException)
{
	CHECK_GOOD_STRING("mapLocalDrive",drivePath)

#define SUBST_COMMAND	"subst.exe"

	char  driveLetterString[] = { ' ', driveLetter, ':', '\0' };
	char *substCommand;

	// get full path to SUBST_COMMAND
	char *fullSubstPath = new char[MAX_PATH];
	_searchenv(SUBST_COMMAND,"PATH",fullSubstPath);

	//
	// first of all, delete substituted drive letter (just in case)
	//

	// build up command string
	cmdRunnerData->stringSubstituter.stringInit(substCommand);
	cmdRunnerData->stringSubstituter.stringAppend(substCommand,fullSubstPath);
	cmdRunnerData->stringSubstituter.stringAppend(substCommand,driveLetterString);
	cmdRunnerData->stringSubstituter.stringAppend(substCommand," /d");

	// run SUBST /d - ignore the result of the command (error if subst does not exist)
	LOGGER_LOG_DEBUG1("About to delete substitution using '%s'",substCommand)
	(void)system(substCommand);
	LOGGER_LOG_DEBUG1("Completed: '%s'",substCommand)

	// release string storage
	cmdRunnerData->stringSubstituter.stringDelete(substCommand);

	//
	// now, perform substitution
	//

	// build up command string
	cmdRunnerData->stringSubstituter.stringInit(substCommand);
	cmdRunnerData->stringSubstituter.stringAppend(substCommand,fullSubstPath);
	cmdRunnerData->stringSubstituter.stringAppend(substCommand,driveLetterString);
	cmdRunnerData->stringSubstituter.stringAppend(substCommand," ");
	cmdRunnerData->stringSubstituter.stringAppend(substCommand,drivePath);
	cmdRunnerData->stringSubstituter.stringSubstitute(substCommand);

	// run SUBST x: path
	LOGGER_LOG_DEBUG1("About to create substitution using '%s'",substCommand)
	int rc = system(substCommand);

	// release string storage
	cmdRunnerData->stringSubstituter.stringDelete(substCommand);
	delete[] fullSubstPath;

	// successful subst?
	if(rc == 0)
	{
		// success
		LOGGER_LOG_DEBUG1("Completed: '%s'",substCommand)
	}
	else
	{
		LOGGER_LOG_ERROR3("mapLocalDrive(): failed to subst %c = '%s' (rc = %d)",
								driveLetter,drivePath,rc)
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_COMMAND_FAILED,"CmdRunner","mapLocalDrive")
	}

}

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::mapNetworkDrive
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : map a network drive letter (ie NET USE)
//
// ARGUMENTS       : driveLetter  IN  drive letter to map
//                   networkPath  IN  network path to map it to
//
// THROWS          : SrvStartException
//
// ============================================================================
void CmdRunner::mapNetworkDrive
(
	const char  driveLetter,
	const char *networkPath
) throw (SrvStartException)
{
	CHECK_GOOD_STRING("mapNetworkDrive",networkPath)
	char *netPath;
	cmdRunnerData->stringSubstituter.stringInit(netPath);
	cmdRunnerData->stringSubstituter.stringCopy(netPath,networkPath);
	cmdRunnerData->stringSubstituter.stringSubstitute(netPath);

	NETRESOURCE netResource;
	netResource.dwType = RESOURCETYPE_DISK;
	char driveLetterString[3] = { driveLetter, ':', '\0' };
	netResource.lpLocalName = driveLetterString;
	netResource.lpRemoteName = const_cast<char*>(netPath);
	netResource.lpProvider = 0;

	// disconnect any existing network connection (discard result)
	LOGGER_LOG_DEBUG1("mapNetworkDrive: about to disconnect '%s' (if any)",driveLetterString)
	(void)WNetCancelConnection2(driveLetterString,0,FALSE);
	LOGGER_LOG_DEBUG1("mapNetworkDrive: disconnected '%s' (if any)",driveLetterString)

	// make call to WNetAddConnection2 to map network drive
	LOGGER_LOG_DEBUG2("mapNetworkDrive: mapping '%s' to '%s'",netResource.lpLocalName,netResource.lpRemoteName)
	DWORD netError = WNetAddConnection2(&netResource,NULL,NULL,0);

	// did an error occur?
	if(netError == NO_ERROR)
	{
		// success
		SS_RETURNV("mapNetworkDrive")
	}

	// log the error
	switch(netError)
	{
	case ERROR_ACCESS_DENIED:
		LOGGER_LOG_ERROR("mapNetworkDrive: Access to the network resource was denied")
		break;

	case ERROR_ALREADY_ASSIGNED:
		LOGGER_LOG_ERROR1(
			"mapNetworkDrive: The local device '%s' is already connected to a network resource",
			netResource.lpLocalName)
		break;

	case ERROR_BAD_DEV_TYPE:
		LOGGER_LOG_ERROR2(
			"mapNetworkDrive: The type of local device '%s' and the type of network resource '%s' do not match",
			netResource.lpLocalName,netResource.lpRemoteName)
		break;

	case ERROR_BAD_DEVICE:
		LOGGER_LOG_ERROR1("mapNetworkDrive: The value specified by '%s' is invalid",netResource.lpLocalName)
		break;

	case ERROR_BAD_NET_NAME:
		LOGGER_LOG_ERROR1(
			"mapNetworkDrive: The value specified by '%s' is not acceptable to any network resource provider. The resource name is invalid, or the named resource cannot be located",
			netResource.lpRemoteName)
		break;

	case ERROR_BAD_PROFILE:
		LOGGER_LOG_ERROR("mapNetworkDrive: The user profile is in an incorrect format")
		break;

	case ERROR_BAD_PROVIDER:
		LOGGER_LOG_ERROR("mapNetworkDrive: The value specified by lpProvider does not match any provider")
		break;

	case ERROR_BUSY:
		LOGGER_LOG_ERROR(
			"mapNetworkDrive: The router or provider is busy, possibly initializing. The caller should retry")
		break;

	case ERROR_CANCELLED:
		LOGGER_LOG_ERROR(
			"mapNetworkDrive: The attempt to make the connection was cancelled by the user through a dialog box from one of the network resource providers, or by a called resource")
		break;

	case ERROR_CANNOT_OPEN_PROFILE:
		LOGGER_LOG_ERROR(
			"mapNetworkDrive: The system is unable to open the user profile to process persistent connections")
		break;

	case ERROR_DEVICE_ALREADY_REMEMBERED:
		LOGGER_LOG_ERROR1(
			"mapNetworkDrive: An entry for the device specified in '%s' is already in the user profile",
			netResource.lpLocalName)
		break;

	case ERROR_EXTENDED_ERROR:
		LOGGER_LOG_ERROR(
			"mapNetworkDrive: A network-specific error occured. Call the WNetGetLastError function to get a description of the error")
		break;

	case ERROR_INVALID_PASSWORD:
		LOGGER_LOG_ERROR("mapNetworkDrive: The specified password is invalid")
		break;

	case ERROR_NO_NET_OR_BAD_PATH:
		LOGGER_LOG_ERROR(
			"mapNetworkDrive: A network component has not started, or the specified name could not be handled")
		break;

	case ERROR_NO_NETWORK:
		LOGGER_LOG_ERROR("mapNetworkDrive: The network is unavailable")
		break;

	default:
		LOGGER_LOG_ERROR1("mapNetworkDrive: unrecognised error %d",netError)
		break;

	}

	// throw exception
	THROW_SRVSTART_EXCEPTION
		(SRVSTART_EXCEPTION_INVALID_PARAMETER,"CmdRunner","mapNetworkDrive")

}

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::get<property>
//                   CmdRunner::set<property>
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : set / get properties
//
// ARGUMENTS       : property value (set)
//
// RETURNS         : property value (get)
//
// ============================================================================
void CmdRunner::setDebugLevel(int dl) { LOGGER_SET_DEBUG_LEVEL(dl); }
void CmdRunner::setWaitInterval(int wi) { cmdRunnerData->waitInterval = wi; }
void CmdRunner::setExecutionPriority(EXECUTION_PRIORITIES ep) { cmdRunnerData->executionPriority = ep; }
void CmdRunner::setStartupDelay(int sd) { cmdRunnerData->startupDelay = sd; }
void CmdRunner::setStartupDirectory(const char *dir) throw (SrvStartException)
{
	CHECK_GOOD_STRING("setStartupDirectory",dir)
	cmdRunnerData->stringSubstituter.stringCopy(cmdRunnerData->startupDirectory,dir);
}

char *CmdRunner::getSrvName() const { return cmdRunnerData->srvName; }
int   CmdRunner::getWaitInterval() const { return cmdRunnerData->waitInterval; }
int   CmdRunner::getStartupDelay() const { return cmdRunnerData->startupDelay; }
char *CmdRunner::getStartupDirectory() const { return cmdRunnerData->startupDirectory; }
CmdRunner::EXECUTION_PRIORITIES
      CmdRunner::getExecutionPriority() const { return cmdRunnerData->executionPriority; }


// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::setAutoRestart
//                   CmdRunner::setAutoRestartInterval
//                   CmdRunner::getAutoRestart
//                   CmdRunner::getAutoRestartInterval
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : set / get auto-restart properties
//
// ARGUMENTS       : property value (set)
//
// RETURNS         : property value (get)
//
// ============================================================================
void CmdRunner::setAutoRestart(bool ar) { cmdRunnerData->autoRestart = ar; }
void CmdRunner::setAutoRestartInterval(int in) { cmdRunnerData->autoRestartInterval = in; }

bool CmdRunner::getAutoRestart() const { return cmdRunnerData->autoRestart; }
int  CmdRunner::getAutoRestartInterval() const { return cmdRunnerData->autoRestartInterval; }

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::addEnv
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : set an environment variable
//
// ARGUMENTS       : nm  IN environment variable name
//                   val IN environment variable value
//
// THROWS          : SrvStartException
//
// ============================================================================
void CmdRunner::addEnv
(
	const char *nm,
	const char *val
) throw (SrvStartException)
{
	LOGGER_LOG_DEBUG2("CmdRunner::addEnv('%s','%s')",nm,val)

	// check input parameters
	CHECK_GOOD_STRING("addEnv",nm)
	CHECK_GOOD_STRING("addEnv",val)

	// copy value
	char *tmp_val = 0;
	cmdRunnerData->stringSubstituter.stringCopy(tmp_val,val);

	// perform substitution on environment value
	cmdRunnerData->stringSubstituter.stringSubstitute(tmp_val);
	LOGGER_LOG_DEBUG1("value after substitution is '%s')",tmp_val)

	// log an informational message
	LOGGER_LOG_INFO2("SET %s=%s",nm,tmp_val)
	// set the environment variable
	SetEnvironmentVariable(nm,tmp_val);
}

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::setStartMinimised
//                   CmdRunner::setStartInNewWindow
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : set start profile
//
// ARGUMENTS       : flag
//
// ============================================================================
void CmdRunner::setStartMinimised(bool sm) { cmdRunnerData->startMinimised = sm; }
void CmdRunner::setStartInNewWindow(bool nw) { cmdRunnerData->startInNewWindow = nw; }

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::~CmdRunner
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : destructor
//
// ARGUMENTS       : mode IN start mode
//
// ============================================================================
CmdRunner::~CmdRunner()
{
	LOGGER_LOG_DEBUG("CmdRunner::~CmdRunner()")

	// delete CmdRunner data
	delete cmdRunnerData;
	CloseHandle(stopCallbackEvent);
}

// ============================================================================
//
// STATIC MEMBER FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::stopCallbackFunction
//
// ACCESS SPECIFIER: public static
//
// DESCRIPTION     : this function is called by the ScmConnector if the SCM
//                   sends a STOP request (eg if the user uses Control
//                   Panel|Services to stop the service).
//
//                   It is NOT needed by the CmdRunner class, but it is included
//                   for illustrative purposes.
//
// ARGUMENTS       : genericPointer  IN generic pointer
//
// ============================================================================
void CmdRunner::stopCallbackFunction
(
	void *genericPointer
)
{
	// pointer should point to this object
	if(genericPointer==0)
	{
		// the callback has been called with a null pointer
		// this should never happen
		LOGGER_LOG_ERROR("stopCallbackFunction() has been invoked with NULL pointer")
	}
	else
	{
		// cast the generic pointer
		CmdRunner *thisObject = static_cast<CmdRunner*>(genericPointer);
		LOGGER_LOG_DEBUG1("stopCallbackFunction(%s) has been correctly invoked",
				thisObject->getSrvName())
	}
}

// ============================================================================
//
// PRIVATE MEMBER FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::startCommand
//
// ACCESS SPECIFIER: private
//
// DESCRIPTION     : start the command
//
// THROWS          : SrvStartException
//
// ============================================================================
void CmdRunner::startCommand() throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("CmdRunner::startCommand()")

	STARTUPINFO startupInfo;
	DWORD       creationFlags = 0;

	// initialise StartupInfo structure
	memset(&startupInfo,0,sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);

	// define execution priority of new process
	switch(cmdRunnerData->executionPriority)
	{
		case HIGH_PRIORITY:
			LOGGER_LOG_DEBUG("command will start at HIGH priority")
			creationFlags = creationFlags | HIGH_PRIORITY_CLASS;
			break;
		case IDLE_PRIORITY:
			LOGGER_LOG_DEBUG("command will start at IDLE priority")
			creationFlags = creationFlags | IDLE_PRIORITY_CLASS;
			break;
		case REAL_PRIORITY:
			LOGGER_LOG_DEBUG("command will start at REALTIME priority")
			creationFlags = creationFlags | REALTIME_PRIORITY_CLASS;
			break;
		default:
			LOGGER_LOG_DEBUG("command will start at NORMAL priority")
			creationFlags = creationFlags | NORMAL_PRIORITY_CLASS;
			break;
	}

	// command mode?
	if(cmdRunnerData->startMode == COMMAND_MODE)
	{
		LOGGER_LOG_DEBUG("command will start in new console window")
		creationFlags = creationFlags | CREATE_NEW_CONSOLE;
		// new window name
		LOGGER_LOG_DEBUG1("new window title is '%s'",cmdRunnerData->srvName)
		startupInfo.lpTitle = cmdRunnerData->srvName;
		// start new window minimised?
		if(cmdRunnerData->startMinimised)
		{
			LOGGER_LOG_DEBUG("command will start in minimised window")
			startupInfo.dwFlags = STARTF_USESHOWWINDOW;
			startupInfo.wShowWindow = SW_MINIMIZE;
		}
	}

	// start the process
	createProcess(cmdRunnerData->startupCommand,false,cmdRunnerData->hCommandProcess,
						&(cmdRunnerData->dwProcessId),0,cmdRunnerData->startupDirectory,
						creationFlags,&startupInfo,cmdRunnerData->waitInterval);

	// return
	SS_RETURNV("CmdRunner::startCommand()")

}

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::waitForStartup
//
// ACCESS SPECIFIER: private
//
// DESCRIPTION     : wait for command to finish starting up
//
// THROWS          : SrvStartException
//
// ============================================================================
void CmdRunner::waitForStartup() throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("CmdRunner::waitForStartup()")

	// what are we waiting for?
	if(cmdRunnerData->waitCommand[0] != '\0')
	{
		// start wait command and wait for it to complete
		LOGGER_LOG_INFO3(
"%s is waiting for command '%s' to complete before reporting a 'running' status to the SCM for service '%s'",
			getApplication(),cmdRunnerData->waitCommand,cmdRunnerData->srvName)

		// run wait command and wait for it to complete
		HANDLE hWaitProcess;
		createProcess(cmdRunnerData->waitCommand,true,hWaitProcess);
		LOGGER_LOG_INFO2("wait command '%s' has now completed for service '%s'",
					cmdRunnerData->waitCommand,cmdRunnerData->srvName)
		SS_RETURNV("CmdRunner::waitForStartup")
	}
	else
	{
		if(cmdRunnerData->startupDelay>0)
		{
			// wait for a specified time period
			LOGGER_LOG_INFO3(
	"%s is waiting %d seconds before reporting a 'running' status to the SCM for service '%s'",
					getApplication(),cmdRunnerData->startupDelay,cmdRunnerData->srvName)
			// sleep for specified time
			Sleeper::Sleep(cmdRunnerData->startupDelay,"startup delay");
			// we are ready to roll
			LOGGER_LOG_INFO2("wait period %d has now completed for service '%s'",
							cmdRunnerData->startupDelay,cmdRunnerData->srvName)
			SS_RETURNV("CmdRunner::waitForStartup")
		}
		else
		{
			SS_RETURNV("CmdRunner::waitForStartup")
		}
	}
}

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::watchCommand
//
// ACCESS SPECIFIER: private
//
// DESCRIPTION     : watch command until it completes (it finishes on its own
//                   or a STOP request is received)
//
// RETURNS         : one of:
//                      WATCH_COMMAND_COMPLETED
//                      WATCH_COMMAND_WAS_STOPPED
//                      WATCH_ERROR
//
// THROWS          : SrvStartException
//
// ============================================================================
CmdRunner::WATCH_OUTCOMES CmdRunner::watchCommand() throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("watchCommand()")

	// wait for command to complete
	while(true)
	{
		// sleep
		Sleeper::Sleep(cmdRunnerData->waitInterval,"process to complete");

		// get the current status of the command
		switch(getProcessStatus(cmdRunnerData->hCommandProcess))
		{
			case PROCESS_STATUS_STILL_RUNNING:
				// process is still running - just continue
				LOGGER_LOG_DEBUG("watchCommand: process still running - will wait again")
				break;

			case PROCESS_STATUS_EXIT_SUCCESS:
				// process has exited successfully - return
				LOGGER_LOG_DEBUG("watchCommand: process has finished ok")
				SS_RETURN("watchCommand",WATCH_COMMAND_COMPLETED);
				break;

			case PROCESS_STATUS_EXIT_FAILURE:
				// process has failed - return error
				LOGGER_LOG_ERROR("watchCommand: process has finished with error")
				SS_RETURN("watchCommand",WATCH_COMMAND_COMPLETED);
				break;
		}

		// process is still running - has a stop callback been invoked?
		if(stopCallbackVar)
		{
			LOGGER_LOG_DEBUG("watchCommand: STOP callback variable has been set")
			// this next bit of code is just for illustration - it is not actually needed
			if(WaitForSingleObject(stopCallbackEvent,0)==WAIT_OBJECT_0)
			{
				// this is as expected
				LOGGER_LOG_DEBUG("watchCommand: STOP callback event has been signalled - OK")
			}
			else
			{
				// this is unexpected (but maybe we have got in just too quickly
				LOGGER_LOG_DEBUG("watchCommand: STOP callback event has NOT been signalled - unexpected")
			}

			// notify STOPPING status to SCM
			cmdRunnerData->scmConnector->notifyScmStatus(ScmConnector::STATUS_STOPPING);

			// kill the command
			killCommand();

			// command killed ok
			LOGGER_LOG_DEBUG("command killed ok")
			SS_RETURN("watchCommand",WATCH_COMMAND_WAS_STOPPED);

		}

	}

}

// ============================================================================
//
// MEMBER FUNCTION : CmdRunner::killCommand
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : stop the running command (service mode only)
//
// THROWS          : SrvStartException
//
// ============================================================================
void CmdRunner::killCommand() throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("CmdRunner::killCommand()")

	// notify the SCM that the service is stopping
	cmdRunnerData->scmConnector->notifyScmStatus(ScmConnector::STATUS_STOPPING);

	// is the shutdown method 'command'?
	if(cmdRunnerData->shutdownMethod==SHUTDOWN_BY_COMMAND)
	{
		// is there a shutdown command?
		if(cmdRunnerData->shutdownCommand[0] != '\0')
		{
			LOGGER_LOG_DEBUG1("using '%s' to shut down process",cmdRunnerData->shutdownCommand)

			// run the shutdown command for this process and wait for it to complete
			HANDLE hStopProcess;
			createProcess(cmdRunnerData->shutdownCommand,true,hStopProcess);
		}
		else
		{
			LOGGER_LOG_INFO("Shutdown method of 'command' was specified, but no command was specified.  Will use 'kill' instead.")
			cmdRunnerData->shutdownMethod=SHUTDOWN_BY_KILL;
		}
	}

	// is the shutdown method 'winmessage'?
	if(cmdRunnerData->shutdownMethod==SHUTDOWN_BY_WINMESSAGE)
	{
		LOGGER_LOG_DEBUG("sending Windows message to shut down process")

		// find all Windows opened by the process
		LOGGER_LOG_DEBUG("about to call EnumWindows()")
		EnumWindows((WNDENUMPROC)sendCloseMessage,(LPARAM)cmdRunnerData->dwProcessId);
		LOGGER_LOG_DEBUG("call to EnumWindows() completed")

	}

	// is the shutdown method 'kill'?
	if(cmdRunnerData->shutdownMethod==SHUTDOWN_BY_KILL)
	{
		LOGGER_LOG_DEBUG("using TerminateProcess() to shut down process")
		// use brute force to terminate the process we have started
		// NB this "may leave DLLs in an unstable state" according to Microsoft ...
		// (I haven't seen it myself yet)
		if(!TerminateProcess(cmdRunnerData->hCommandProcess,0))
		{
			// failed to terminate process
			// it may have already terminated, so just log a message
			LOGGER_LOG_INFO1("failed to terminate process, error=%d (it may have already stopped)",
					GetLastError())
		}
	}

	// whatever the shutdown method, wait for the process to shut down
	int shutdownCount = 0;
	while(getProcessStatus(cmdRunnerData->hCommandProcess)==PROCESS_STATUS_STILL_RUNNING)
	{
		// sleep one second
		Sleeper::Sleep(1,"process to complete");
		if(((++shutdownCount)|60)==0)
		{
			// log a warning message
			LOGGER_LOG_INFO2("WARNING: service '%s' has been shutting down for %d minutes",
								cmdRunnerData->srvName,shutdownCount/60)
		}
	}

	// return
	SS_RETURNV("CmdRunner::killCommand")
}

// ============================================================================
//
// LOCAL UTILITY FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// LOCAL FUNCTION  : createProcess
//
// DESCRIPTION     : create a new Windows NT process
//
// ARGUMENTS       : command       IN  command to run
//                   wait          IN  if true, then wait for process to complete
//                   hProcess      OUT handle to created process
//                   processId     OUT process id of created process (may be NULL)
//                   env           IN  pointer to environment block (may be NULL)
//                   cwd           IN  starting directory (may be NULL)
//                   creationFlags IN  creation flags (see help for Win32 CreateProcess)
//                   startupInfo   IN  startup info (see help for Win32 CreateProcess)
//                   waitInterval  IN  how often to poll the running process if
//                                     wait is true
//
// THROWS          : SrvStartException
//
// ============================================================================
void createProcess
(
	char        *command,
	bool         wait,
	HANDLE      &hProcess,
	DWORD       *processId,
	void        *env,
	char        *cwd,
	DWORD        creationFlags,
	STARTUPINFO *startupInfo,
	int          waitInterval
) throw (SrvStartException)
{
	LOGGER_LOG_DEBUG1("createProcess '%s'",command)

	SECURITY_ATTRIBUTES processAttributes;
	SECURITY_ATTRIBUTES threadAttributes;

	// use default security attributes for started process and its main thread
	processAttributes.nLength              = sizeof(processAttributes);
	processAttributes.lpSecurityDescriptor = NULL;
	processAttributes.bInheritHandle       = FALSE;
	threadAttributes.nLength               = sizeof(threadAttributes);
	threadAttributes.lpSecurityDescriptor  = NULL;
	threadAttributes.bInheritHandle        = FALSE;

	// initialise StartupInfo structure if not supplied
	STARTUPINFO sin;
	if(startupInfo == NULL)
	{
		memset(&sin,0,sizeof(sin));
		sin.cb = sizeof(sin);
		startupInfo = &sin;
	}

	// start the process
	LOGGER_LOG_DEBUG1("about to start process with command '%s'",command)
	PROCESS_INFORMATION startedProcessInfo;
	if(CreateProcess(
			NULL,
			command,				// command to run
			&processAttributes,		// process security attributes
			&threadAttributes,		// main thread security attributes
			FALSE,					// do not inherit handles
			creationFlags,			// creation flags
			env,					// environment
			cwd,					// current directory
			startupInfo,			// startup info
			&startedProcessInfo)	// returned process info
			)
	{
		hProcess = startedProcessInfo.hProcess;
		if(processId!=0) { (*processId) = startedProcessInfo.dwProcessId; }
		LOGGER_LOG_DEBUG1("process started, id = %d",startedProcessInfo.dwProcessId)
	}
	else
	{
		hProcess = NULL;
		LOGGER_LOG_ERROR2("createProcess(): failed to start process '%s', error=%d",command,GetLastError())
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_CREATE_PROCESS_FAILED,"","createProcess")
	}

	// do we need to wait for the process to finish?
	if(wait)
	{
		// wait for process to complete
		waitForProcessToComplete(hProcess,waitInterval);
	}

	SS_RETURNV("createProcess")
}

// ============================================================================
//
// LOCAL FUNCTION  : waitForProcessToComplete
//
// DESCRIPTION     : wait for a given process to complete
//
// ARGUMENTS       : hProcess IN process to wait for
//
// THROWS          : SrvStartException
//
// ============================================================================
void waitForProcessToComplete
(
	HANDLE &hProcess,
	int     waitInterval
) throw(SrvStartException)
{
	LOGGER_LOG_DEBUG("waitForProcessToComplete()")

	// wait for command to complete
	while(true)
	{
		// sleep
		Sleeper::Sleep(waitInterval,"process to complete");

		// get the current status of the command
		switch(getProcessStatus(hProcess))
		{
			case PROCESS_STATUS_STILL_RUNNING:
				// process is still running - just continue
				LOGGER_LOG_DEBUG("waitForProcessToComplete(): process still running - will wait again")
				break;

			case PROCESS_STATUS_EXIT_SUCCESS:
				// process has exited successfully - return
				LOGGER_LOG_DEBUG("waitForProcessToComplete(): process has finished ok")
				CloseHandle(hProcess);
				SS_RETURNV("waitForProcessToComplete")
				break;

			case PROCESS_STATUS_EXIT_FAILURE:
				// process has failed - return error
				LOGGER_LOG_ERROR("waitForProcessToComplete(): process has finished with error")
				CloseHandle(hProcess);
				THROW_SRVSTART_EXCEPTION
					(SRVSTART_EXCEPTION_CREATE_PROCESS_FAILED,"","waitForProcessToComplete")
				break;
		}
	}
}

// ============================================================================
//
// LOCAL FUNCTION  : getProcessStatus
//
// DESCRIPTION     : get the status of a given process
//
// ARGUMENTS       : hProcess  IN handle to process
//
// RETURNS         : status of that process
//
// THROWS          : SrvStartException
//
// ============================================================================
STARTED_PROCESS_STATUS getProcessStatus
(
	HANDLE hProcess
) throw (SrvStartException)
{

	DWORD exitCode;

	if(!GetExitCodeProcess(hProcess,&exitCode))
	{
		LOGGER_LOG_ERROR1("getProcessStatus(): failed to get process status, error=%d",GetLastError())
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_GENERAL_ERROR,"StartedProcessStatus","getProcessStatus")
	}

	// check exit code of started process
	if(exitCode==STILL_ACTIVE)
	{
		// the wait process is still running
		LOGGER_LOG_DEBUG1("process is still running (status %d)",exitCode)
		SS_RETURN("getProcessStatus",PROCESS_STATUS_STILL_RUNNING)
	}
	else if(exitCode==0)
	{
		// the process has exited successfully
		LOGGER_LOG_DEBUG1("process completed, exit code = %d",exitCode)
		SS_RETURN("getProcessStatus",PROCESS_STATUS_EXIT_SUCCESS)
	}
	else
	{
		// the process has exited unsuccessfully
		LOGGER_LOG_ERROR1("getProcessStatus(): command failed with exit code %d",exitCode)
		SS_RETURN("getProcessStatus",PROCESS_STATUS_EXIT_FAILURE)
	}
}

// ============================================================================
//
// LOCAL FUNCTION  : sendCloseMessage
//
// DESCRIPTION     : this function is called by EnumWindows
//                   if the supplied window handle matches the process id
//                     then it sends that window a close message
//
// ARGUMENTS       : hwnd    IN handle to window enumerated by EnumWindows
//                   lParam  IN id of started process
//
// RETURNS         : status of that process
//
// ============================================================================
BOOL CALLBACK sendCloseMessage
(
	HWND   hwnd,
	LPARAM lParam
)
{
	// get process id for the given window
	DWORD windowProcess;
	GetWindowThreadProcessId(hwnd, &windowProcess);

	// was this window opened by our process?
	if(windowProcess == (DWORD)lParam)
	{
		// yes, it was
		LOGGER_LOG_DEBUG2("sendCloseMessage(): process %d opened window %d - about to post it WM_CLOSE",
			(DWORD)lParam,hwnd)

		// post the window a message to close
		PostMessage(hwnd,WM_CLOSE,0,0);
	}
	else
	{
		// no, it wasn't
		LOGGER_LOG_DEBUG2("sendCloseMessage(): process %d did not open window %d",(DWORD)lParam,hwnd)
	}

	return TRUE ;

}
