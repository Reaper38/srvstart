// ============================================================================
//
// FILE        : ScmConnector.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : Implementation of exported class ScmConnector
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

// support headers
#include <logger.h>

// class headers
#include "Sleeper.h"
#include "ScmConnector.h"

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

const int SERVICE_MAIN_WAIT_SECONDS = 1;

// ============================================================================
//
// STATIC (LOCAL) FUNCTION PROTOTYPES
//
// ============================================================================

void threadMain(void *arg);
void WINAPI serviceMain(DWORD argc,LPTSTR *argv);
void WINAPI serviceCtrlHandler(DWORD opcode);
void reportServiceStatus(DWORD status,DWORD checkPoint=0,DWORD waitHint=0) throw(SrvStartException);
BOOL WINAPI shutdownHandler(DWORD ctrlType);

// ============================================================================
//
// LOCAL MACROS
//
// ============================================================================

#define	CATCH_AND_RETURN(f)	\
	catch(...) {	\
		LOGGER_LOG_ERROR1("%s(): caught exception - returning",f) return; }

// ============================================================================
//
// LOCAL CLASSES
//
// ============================================================================

//
// we use the local class ThreadMainData for several reasons
//  - it vastly simplifies the ScmConnector interface
//  - we don't really want to publicise the internal data structure for this class
//  - we have to store our internal data in a global variable, so that the Win32
//    service management functions (serviceMain and serviceCtrlHandler) can access it
//

class ThreadMainData
{

public:	// member functions

	// =========== //
	// constructor //
	// =========== //
	ThreadMainData(char *svcName,bool allowConnectErrors)
	{
		// parameters
		_svcName = new char[strlen(svcName)+1];
		strcpy(_svcName,svcName);
		_allowConnectErrors = allowConnectErrors;
		// callbacks
		_stopRequestedVar      = 0;
		_stopRequestedEvent    = 0;
		_stopRequestedFunction = 0;
		_genericPointer        = 0;
		// internals
		_scmStatus  = ScmConnector::STATUS_INITIALISING;
		_checkPoint = 0;
	}

	// ========== //
	// destructor //
	// ========== //
	~ThreadMainData() { delete _svcName; }

	// =================== //
	// callbacks - install //
	// =================== //
	void installStopCallback(bool *stopRequestedVar)
	{
		LOGGER_LOG_DEBUG1("installStopCallback(bool @%p)",stopRequestedVar)
		_stopRequestedVar = stopRequestedVar;
		(*_stopRequestedVar) = false;
	}
	void installStopCallback(HANDLE *stopRequestedEvent)
	{
		_stopRequestedEvent = stopRequestedEvent;
	}
	void installStopCallback(ScmConnector::STOP_HANDLER_FUNCTION *stopRequestedFunction,void *genericPointer)
	{
		_stopRequestedFunction = stopRequestedFunction;
		_genericPointer        = genericPointer;
	}


	// =============== //
	// callbacks - get //
	// =============== //
	bool *getStopCallbackVar() const { return _stopRequestedVar; }
	HANDLE *getStopCallbackEvent() const { return _stopRequestedEvent; }
	ScmConnector::STOP_HANDLER_FUNCTION *getStopCallbackFunction() const { return _stopRequestedFunction; }
	void *getCallbackGenericPointer() const { return _genericPointer; }

	// ============== //
	// set properties //
	// ============== //
	void setScmStatus(ScmConnector::SCM_STATUSES scmStatus) { _scmStatus = scmStatus; }
	void setServiceStatusHandle(SERVICE_STATUS_HANDLE hServiceStatus) { _hServiceStatus = hServiceStatus; }

	// ============== //
	// get properties //
	// ============== //
	char *getSvcName() const { return _svcName; }
	bool allowConnectErrors() const { return _allowConnectErrors; }
	ScmConnector::SCM_STATUSES getScmStatus() const { return _scmStatus; }
	SERVICE_STATUS_HANDLE getServiceStatusHandle() const { return _hServiceStatus; }
	int getAndIncrementCheckpoint() { return ++_checkPoint; }

private:	// data members
	// parameters
	char *_svcName;
	bool _allowConnectErrors;

	// callbacks
	bool *_stopRequestedVar;
	HANDLE *_stopRequestedEvent;
	ScmConnector::STOP_HANDLER_FUNCTION *_stopRequestedFunction;
	void *_genericPointer; // generic pointer supplied to installStopCallback

	// internals
	ScmConnector::SCM_STATUSES _scmStatus;
	SERVICE_STATUS_HANDLE _hServiceStatus;
	int _checkPoint;

	// prevent default constructor
	ThreadMainData();

};

// ============================================================================
//
// GLOBAL VARIABLES
//
// ============================================================================

// this is pretty awful but we have to use global storage
// since there is no other way of passing this data into the serviceMain routine
//  (what about Win32 Thread Local Storage??)
ThreadMainData *G_threadMainData;

// ============================================================================
//
// PUBLIC MEMBER FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// MEMBER FUNCTION : ScmConnector::ScmConnector
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : constructor
//
// ARGUMENTS       : srvName            IN name of command/service
//                   allowConnectErrors IN if true, then failure to connect to
//                                         SCM is not a fatal error
//
// THROWS          : SrvStartException
//
// ============================================================================
ScmConnector::ScmConnector
(
	char *srvName,
	bool  allowConnectErrors
)
throw (SrvStartException)
{
	LOGGER_LOG_DEBUG1("ScmConnector::ScmConnector(%s)",srvName)

	// initialise our global storage
	G_threadMainData = new ThreadMainData(srvName,allowConnectErrors);

	// straight away, start a thread to try and connect to SCM
	unsigned long serviceMainThread  = _beginthread(threadMain,0,NULL);

	// check if created ok
	if(serviceMainThread<0)
	{
		LOGGER_LOG_ERROR1("failed to create service thread, error = %d",serviceMainThread)
		G_threadMainData->setScmStatus(ScmConnector::STATUS_FAILED);
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_GENERAL_ERROR,"ScmConnector","ScmConnector")
	}

	// wait for thread status to change from "initialising"
	//  - for a successful connect, serviceMain changes it to "starting"
	//  - for a failed connect, threadMain changes it to "start as console"
	while(true)
	{
		// sleep for one second
		Sleeper::Sleep(1,"SCM connection");

		// are we still initialising?
		if(G_threadMainData->getScmStatus()!=STATUS_INITIALISING)
		{
			// something has happened
			LOGGER_LOG_DEBUG("status is no longer STATUS_INITIALISING")
			break;
		}
	}

}

// ============================================================================
//
// MEMBER FUNCTION : ScmConnector::~ScmConnector
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : destructor
//
// ============================================================================
ScmConnector::~ScmConnector()
{
	LOGGER_LOG_DEBUG("ScmConnector::~ScmConnector()")

}

// ============================================================================
//
// MEMBER FUNCTION : ScmConnector::notifyScmStatus
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : notify status to SCM
//
// ARGUMENTS       : status        IN status, one of:
//                                      STATUS_STARTING
//                                      STATUS_RUNNING
//                                      STATUS_STOPPING
//                                      STATUS_STOPPED
//                   ignoreErrors  IN if true, do not throw an exception if error
//                                    occurs
//
// THROWS          : SrvStartException
//
// ============================================================================
void ScmConnector::notifyScmStatus
(
	SCM_STATUSES scmStatus,
	bool         ignoreErrors
)
throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("ScmConnector::notifyScmStatus()")

	// set internal status
	G_threadMainData->setScmStatus(scmStatus);

#define	RETHROW_IF_NOT_IGNORE_ERRORS	\
	catch (...) { if(!ignoreErrors) { throw; } }

	switch(scmStatus)
	{
		case STATUS_INITIALISING:
		case STATUS_STARTING:
			// report starting status to SCM
			LOGGER_LOG_DEBUG1("notifying status %d (SERVICE_START_PENDING)",SERVICE_START_PENDING)
			try
			{
				reportServiceStatus(SERVICE_START_PENDING,G_threadMainData->getAndIncrementCheckpoint(),SERVICE_MAIN_WAIT_SECONDS);
			}
			RETHROW_IF_NOT_IGNORE_ERRORS
			break;

		case STATUS_RUNNING:
			// report running status to SCM
			LOGGER_LOG_DEBUG1("notifying status %d (SERVICE_RUNNING)",SERVICE_RUNNING)
			try { reportServiceStatus(SERVICE_RUNNING); }
			RETHROW_IF_NOT_IGNORE_ERRORS
			break;

		case STATUS_STOPPING:
			// report stopping status to SCM
			LOGGER_LOG_DEBUG1("notifying status %d (SERVICE_STOP_PENDING)",SERVICE_STOP_PENDING)
			try { reportServiceStatus(SERVICE_STOP_PENDING); }
			RETHROW_IF_NOT_IGNORE_ERRORS
			break;

		case STATUS_STOPPED:
			// report stopping status to SCM
			LOGGER_LOG_DEBUG1("notifying status %d (SERVICE_STOPPED)",SERVICE_STOPPED)
			try { reportServiceStatus(SERVICE_STOPPED); }
			RETHROW_IF_NOT_IGNORE_ERRORS
			break;

		default:
			// error - ignore
			LOGGER_LOG_ERROR1("ScmConnector::notifyScmStatus() called with invalid status %d",scmStatus)
			THROW_SRVSTART_EXCEPTION
				(SRVSTART_EXCEPTION_NOTIFY_FAILED,"ScmConnector","notifyScmStatus")
			break;
	}
}

// ============================================================================
//
// MEMBER FUNCTION : ScmConnector::getScmStatus
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : notify status to SCM
//
// RETURNS         : status, one of:
//                               STATUS_STARTING
//                               STATUS_RUNNING
//                               STATUS_STOPPING
//                               STATUS_STOPPED etc.
//
// ============================================================================
ScmConnector::SCM_STATUSES ScmConnector::getScmStatus() const
{
	LOGGER_LOG_DEBUG("ScmConnector::getScmStatus()")

	return G_threadMainData->getScmStatus();
}

// ============================================================================
//
// MEMBER FUNCTION : ScmConnector::installStopCallback
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : install "stop" callback (boolean)
//
// ARGUMENTS       : stopRequested IN pointer to global boolean
//
// THROWS          : SrvStartException
//
// ============================================================================
void ScmConnector::installStopCallback
(
	bool *stopRequestedVar
) throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("ScmConnector::installStopCallback(bool)")

	// is the supplied pointer ok?
	if(stopRequestedVar != 0)
	{
		// copy the location of the variable and set it to false
		G_threadMainData->installStopCallback(stopRequestedVar);
		(*stopRequestedVar) = false;
	}
	else
	{
		// NULL pointer exception
		LOGGER_LOG_ERROR("installStopCallback(bool): NULL callback pointer")
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_INVALID_PARAMETER,"ScmConnector","installStopCallback")
	}
}

// ============================================================================
//
// MEMBER FUNCTION : ScmConnector::installStopCallback
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : install "stop" callback (event handle)
//
// ARGUMENTS       : stopRequestedEvent IN pointer to event handle (NB event
//                                         must already have been created using
//                                         Win32 CreateEvent)
//
// THROWS          : SrvStartException
//
// ============================================================================
void ScmConnector::installStopCallback
(
	HANDLE *stopRequestedEvent
) throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("ScmConnector::installStopCallback(HANDLE)")

	// is the supplied pointer ok?
	if(stopRequestedEvent != 0)
	{
		// copy the location of the event handle
		G_threadMainData->installStopCallback(stopRequestedEvent);
	}
	else
	{
		// NULL pointer exception
		LOGGER_LOG_ERROR("installStopCallback(HANDLE): NULL callback pointer")
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_INVALID_PARAMETER,"ScmConnector","installStopCallback")
	}
}

// ============================================================================
//
// MEMBER FUNCTION : ScmConnector::installStopCallback
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : install "stop" callback
//
// ARGUMENTS       : stopRequestedEvent IN pointer to function
//                   genericPointer     IN pointer which will be passed to
//                                      (*stopRequestedEvent) when it is called
//
// THROWS          : SrvStartException
//
// ============================================================================
void ScmConnector::installStopCallback
(
	STOP_HANDLER_FUNCTION *stopRequestedFunction,
	void                  *genericPointer
) throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("ScmConnector::installStopCallback(STOP_HANDLER_FUNCTION)")

	// is the supplied function pointer ok?
	if(stopRequestedFunction != 0)
	{
		// copy the location of the function
		G_threadMainData->installStopCallback(stopRequestedFunction,genericPointer);
	}
	else
	{
		// NULL pointer exception
		LOGGER_LOG_ERROR("installStopCallback(STOP_HANDLER_FUNCTION): NULL callback pointer")
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_INVALID_PARAMETER,"ScmConnector","installStopCallback")
	}
}

// ============================================================================
//
// PRIVATE MEMBER FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// LOCAL UTILITY FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// LOCAL FUNCTION  : threadMain
//
// DESCRIPTION     : thread entry point for the thread which is started by this
//                   object.  This thread performs all of the interactions
//                   with the SCM.
//
// ARGUMENTS       : arg IN void pointer to the ThreadMainData object
//
// ============================================================================
void threadMain
(
	void *arg
)
{
	LOGGER_LOG_DEBUG("threadMain()")

	LOGGER_LOG_DEBUG1("trying to connecting to SCM for service '%s'",G_threadMainData->getSvcName())

	// fill in service details
	SERVICE_TABLE_ENTRY	DispatchTable[2];
	DispatchTable[0].lpServiceName = TEXT(G_threadMainData->getSvcName());
	DispatchTable[0].lpServiceProc = serviceMain;
	DispatchTable[1].lpServiceName = NULL;
	DispatchTable[1].lpServiceProc = NULL;

	// connect to the Service Control Manager
	// if we are not running as a service, this will time out
	if(!StartServiceCtrlDispatcher(DispatchTable))
	{
		LOGGER_LOG_DEBUG("failed in call to StartServiceCtrlDispatcher")
		// failed to connect to Service Control Manager
		if(G_threadMainData->allowConnectErrors())
		{
			// failure allowed - assume running from the console
			LOGGER_LOG_INFO1("failed to connect to SCM for service %s (assuming console)",G_threadMainData->getSvcName())
			G_threadMainData->setScmStatus(ScmConnector::STATUS_MUST_START_AS_CONSOLE);
		}
		else
		{
			// this is an error - report it
			LOGGER_LOG_ERROR2("failed to connect to SCM for service %s, error=%u",G_threadMainData->getSvcName(),GetLastError())
			// return with a failure status
			G_threadMainData->setScmStatus(ScmConnector::STATUS_FAILED);
		}
	}

	// this thread can just terminate now
	LOGGER_LOG_DEBUG("threadMain is terminating")
	return;
}

// ============================================================================
//
// LOCAL FUNCTION  : serviceMain
//
// DESCRIPTION     : thread entry point for service thread
//
// ARGUMENTS       : argc, argv IN command-line arguments (not used)
//
// ============================================================================
void WINAPI serviceMain
(
	DWORD   argc,
	LPTSTR *argv
)
{
	LOGGER_LOG_DEBUG("serviceMain()")

	// register the service's Service Control Handler (SCH)
	// the SCH will handle all requests passed to it by the Service Control Manager (SCM)
	LOGGER_LOG_DEBUG1("registering SCH for service '%s'",G_threadMainData->getSvcName())
	G_threadMainData->setServiceStatusHandle(
			RegisterServiceCtrlHandler(G_threadMainData->getSvcName(),serviceCtrlHandler));

	if (G_threadMainData->getServiceStatusHandle() == 0)
	{
		// failed to register Service Control Handler
		LOGGER_LOG_ERROR1("failed to register SCH, error=%d",GetLastError())
		// return with a failure status
		G_threadMainData->setScmStatus(ScmConnector::STATUS_FAILED);
		// report a "start pending" status
		reportServiceStatus(SERVICE_STOPPED);
		return;
	}

	// we have connected - change our status from "initialising" to "starting"
	G_threadMainData->setScmStatus(ScmConnector::STATUS_STARTING);

	// report a "start pending" status
	try
	{
		reportServiceStatus(SERVICE_START_PENDING,G_threadMainData->getAndIncrementCheckpoint(),SERVICE_MAIN_WAIT_SECONDS);
	}
	CATCH_AND_RETURN("serviceMain")

	// install the console control handler to trap shutdown signals
	LOGGER_LOG_DEBUG("about to install console control (shutdown) handler")
	if(SetConsoleCtrlHandler(shutdownHandler,TRUE)==0)
	{
		LOGGER_LOG_ERROR1("failed to install console control (shutdown) handler, error = %d",GetLastError())
		G_threadMainData->setScmStatus(ScmConnector::STATUS_FAILED);
		return;
	}
 
	// wait for things to happen
	int waitCount = 0;
	while(true)
	{
		// sleep
		Sleeper::Sleep(SERVICE_MAIN_WAIT_SECONDS,"service event");

		// get current status of program
		ScmConnector::SCM_STATUSES srvstartStatus = G_threadMainData->getScmStatus();

		switch(srvstartStatus)
		{
			case ScmConnector::STATUS_STARTING:
				// the service is still starting up
				LOGGER_LOG_DEBUG("serviceMain wait: service has not started yet")
				// we need to keep reporting this status to the SCM
				try
				{
					reportServiceStatus(SERVICE_START_PENDING,
										G_threadMainData->getAndIncrementCheckpoint(),SERVICE_MAIN_WAIT_SECONDS);
				}
				CATCH_AND_RETURN("serviceMain")

				// warn if we have been here too long
				if(((++waitCount)|60)==0)
				{
					// log a warning message
					LOGGER_LOG_INFO2("WARNING: service '%s' has been starting for %d minutes",
										G_threadMainData->getSvcName(),waitCount/60)
				}
				break;

			case ScmConnector::STATUS_RUNNING:
				// the service has started
				// its status has already been reported to the SCM
				LOGGER_LOG_DEBUG("serviceMain wait: service is running")
				// clear wait count
				waitCount = 0;
				break;

			case ScmConnector::STATUS_STOPPING:
				// the service is stopping
				LOGGER_LOG_DEBUG("serviceMain wait: service is stopping")
				// we need to keep reporting this status to the SCM
				try { reportServiceStatus(SERVICE_STOP_PENDING); }
				CATCH_AND_RETURN("serviceMain")
				// warn if we have been here too long
				if(((waitCount++)|60)==0)
				{
					// log a warning message
					LOGGER_LOG_INFO2("WARNING: service '%s' has been stopping for %d minutes",
										G_threadMainData->getSvcName(),waitCount/60)
				}
				break;

			case ScmConnector::STATUS_STOPPED:
				// the service has stopped
				// its status has already been reported to the SCM
				LOGGER_LOG_DEBUG("serviceMain wait: service has stopped")
				// return
				LOGGER_LOG_DEBUG("returning from serviceMain")
				return;

			default:
				LOGGER_LOG_ERROR1("global wait status %d - invalid",srvstartStatus)
				G_threadMainData->setScmStatus(ScmConnector::STATUS_FAILED);
				try { reportServiceStatus(SERVICE_STOPPED); }
				CATCH_AND_RETURN("serviceMain")
				break;
		}
	}

}

// ============================================================================
//
// LOCAL FUNCTION  : serviceCtrlHandler
//
// DESCRIPTION     : service control handler (responds to service status
//                   requests from SCM)
//
// ARGUMENTS       : opcode IN request from SCM (stop, status)
//
// ============================================================================
void WINAPI serviceCtrlHandler
(
	DWORD opcode
)
{
	LOGGER_LOG_DEBUG1("serviceCtrlHandler: opcode is %d",opcode)

	ScmConnector::SCM_STATUSES svcStatus;
	bool stopActionTaken;

	// act on the supplied opcode
	switch(opcode)
	{
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			// STOP SERVICE requested, or system is shutting down
			LOGGER_LOG_DEBUG("serviceCtrlHandler: STOP requested")
			stopActionTaken = false;

			// tell everybody we are shutting down
			G_threadMainData->setScmStatus(ScmConnector::STATUS_STOPPING);
			// report "stopping" status to SCM
			try { reportServiceStatus(SERVICE_STOP_PENDING); }
			CATCH_AND_RETURN("serviceCtrlHandler")

			// take appropriate action according to installed callbacks
			if(G_threadMainData->getStopCallbackVar() != 0)
			{
				// we need to set the supplied variable to true
				LOGGER_LOG_DEBUG("serviceCtrlHandler: setting stop variable true")
				(*G_threadMainData->getStopCallbackVar()) = true;
				stopActionTaken = true;
			}

			if(G_threadMainData->getStopCallbackEvent() != 0)
			{
				// we need to notify the supplied event
				LOGGER_LOG_DEBUG("serviceCtrlHandler: notifying stop event")
				
				if(!SetEvent(*G_threadMainData->getStopCallbackEvent()))
				{
					// failed to notify event
					LOGGER_LOG_ERROR2("failed to notify stop event for service %s, error=%u",
						G_threadMainData->getSvcName(),GetLastError())

				}
				LOGGER_LOG_DEBUG("serviceCtrlHandler: stop event notified")
				stopActionTaken = true;
			}


			if(G_threadMainData->getStopCallbackFunction() != 0)
			{
				// we need to call the supplied function
				LOGGER_LOG_DEBUG("serviceCtrlHandler: call stop function")
				// pass the previously-supplied generic pointer as argument
				(*G_threadMainData->getStopCallbackFunction())(G_threadMainData->getCallbackGenericPointer());
				LOGGER_LOG_DEBUG("serviceCtrlHandler: stop function called")
				stopActionTaken = true;
			}

			// make sure we have done something!
			if(!stopActionTaken)
			{
				// issue a warning message
				LOGGER_LOG_ERROR1("WARNING: there is no stop action for service '%s'",G_threadMainData->getSvcName())
			}

			break;

		case SERVICE_CONTROL_INTERROGATE:
			// INTERROGATE STATUS requested
			LOGGER_LOG_DEBUG("serviceCtrlHandler: INTERROGATE requested")

			// get current status of started process
			svcStatus = G_threadMainData->getScmStatus();

			switch(svcStatus)
			{
				case ScmConnector::STATUS_STARTING:
					// the service is starting
					LOGGER_LOG_DEBUG("service is starting")
					try { reportServiceStatus(SERVICE_START_PENDING); }
					CATCH_AND_RETURN("serviceCtrlHandler")
					break;

				case ScmConnector::STATUS_RUNNING:
					// the service is running
					LOGGER_LOG_DEBUG("service is running")
					try { reportServiceStatus(SERVICE_RUNNING); }
					CATCH_AND_RETURN("serviceCtrlHandler")
					break;

				case ScmConnector::STATUS_STOPPING:
					// the service is stopping
					LOGGER_LOG_DEBUG("service is stopping")
					try { reportServiceStatus(SERVICE_STOP_PENDING); }
					CATCH_AND_RETURN("serviceCtrlHandler")
					break;

				default:
					// the service has stopped or something bad has happened
					LOGGER_LOG_DEBUG("service has stopped")
					try { reportServiceStatus(SERVICE_STOPPED); }
					CATCH_AND_RETURN("serviceCtrlHandler")
					break;
			}
			break;

		default:
			// unsupported or unknown opcode
			LOGGER_LOG_ERROR1("serviceCtrlHandler: unsupported or unknown op code %d",opcode)
			;
	}

	// return from the control handler - NB signalling STOPPED twice really upsets NT!!
	LOGGER_LOG_DEBUG("returning from serviceCtrlHandler")
	return;

}

// ============================================================================
//
// LOCAL FUNCTION  : shutdownHandler
//
// DESCRIPTION     : respond to signals sent to the console process
//                   the one we are interested in is CTRL_SHUTDOWN_EVENT
//                   Windows NT sends this to all services when the system is
//                    shutting down
//
// ARGUMENTS       : dwCtrlType IN type of control signal received
//
// ============================================================================
BOOL WINAPI shutdownHandler
(
  DWORD ctrlType
)
{
	if(ctrlType==CTRL_SHUTDOWN_EVENT)
	{
		LOGGER_LOG_DEBUG1("shutdownHandler: ctrlType is %d - shutting down",ctrlType)
		G_threadMainData->setScmStatus(ScmConnector::STATUS_STOPPING);
	}
	else
	{
		LOGGER_LOG_DEBUG1("shutdownHandler: ctrlType is %d - ignoring this",ctrlType)
	}

	return FALSE;
}

// ============================================================================
//
// LOCAL FUNCTION : SrvStartThread::reportServiceStatus
//
// DESCRIPTION    : report current status of service to SCM
//
// ARGUMENTS      : status     IN status to report (see Win32 SetServiceStatus
//                                for valid values)
//                  checkpoint IN checkpoint, used during startup only
//                  waitHint   IN wait hint, used during startup only
//
// THROWS          : SrvStartException
//
// ============================================================================
void reportServiceStatus
(
	DWORD status,
	DWORD checkPoint,
	DWORD waitHint
) throw (SrvStartException)
{
	SERVICE_STATUS serviceStatus;

	switch(status)
	{
		case SERVICE_STOPPED:
			LOGGER_LOG_DEBUG1("Reporting status %d (SERVICE_STOPPED)",status)
			break;

		case SERVICE_START_PENDING:
			LOGGER_LOG_DEBUG1("Reporting status %d (SERVICE_START_PENDING)",status)
			break;

		case SERVICE_STOP_PENDING:
			LOGGER_LOG_DEBUG1("Reporting status %d (SERVICE_STOP_PENDING)",status)
			break;

		case SERVICE_RUNNING:
			LOGGER_LOG_DEBUG1("Reporting status %d (SERVICE_RUNNING)",status)
			break;

		case SERVICE_CONTINUE_PENDING:
			LOGGER_LOG_DEBUG1("Reporting status %d (SERVICE_CONTINUE_PENDING)",status)
			break;

		case SERVICE_PAUSE_PENDING:
			LOGGER_LOG_DEBUG1("Reporting status %d (SERVICE_PAUSE_PENDING)",status)
			break;

		case SERVICE_PAUSED:
			LOGGER_LOG_DEBUG1("Reporting status %d (SERVICE_PAUSED)",status)
			break;

		default:
			LOGGER_LOG_DEBUG1("Reporting status %d (?)",status)
			break;
	}

	// initialise service status information
	// type of service:  Win32 service running in its own process
	serviceStatus.dwServiceType             = SERVICE_WIN32;
	// current state
	serviceStatus.dwCurrentState            = status;
	// what control codes will be accepted: stop
	serviceStatus.dwControlsAccepted        = SERVICE_ACCEPT_STOP;
	// other status information
	serviceStatus.dwWin32ExitCode           = 0;
	serviceStatus.dwServiceSpecificExitCode = 0;
	serviceStatus.dwCheckPoint              = checkPoint;
	serviceStatus.dwWaitHint                = waitHint;

	if (!SetServiceStatus(G_threadMainData->getServiceStatusHandle(),&serviceStatus))
	{
		// failed to report service status
		LOGGER_LOG_ERROR2("failed to report status %d, error=%u",status,GetLastError())
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_NOTIFY_FAILED,"","reportServiceStatus")
	}
}
