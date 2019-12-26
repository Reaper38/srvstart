// ============================================================================
//
// FILE        : ServiceManager.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : Implementation of exported class ServiceManager
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

// support headers
#include <logger.h>

// class headers
#include "Sleeper.h"
#include "StringSubstituter.h"
#include "ScmConnector.h"
#include "CmdRunner.h"
#include "ServiceManager.h"

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


// ============================================================================
//
// LOCAL FUNCTION PROTOTYPES
//
// ============================================================================


// ============================================================================
//
// LOCAL CLASSES
//
// ============================================================================

//
// CmdRunnerData holds the internal data used by the class
//

struct ServiceManagerData
{
	// service name
	char *serviceName;

	// service parameters
	char *displayName;
	char *binaryPathName;
	bool  desktopService;

	// handle to SCM
	SC_HANDLE hSCM;

	// 	StringSubstituter
	StringSubstituter stringSubstituter;

	// constructor / destructor
	ServiceManagerData(char *sn)
	{
		LOGGER_LOG_DEBUG("ServiceManagerData::ServiceManagerData()")
		stringSubstituter.stringInit(serviceName);
		stringSubstituter.stringCopy(serviceName,sn);
		stringSubstituter.stringInit(displayName);
		// default display name = service name
		stringSubstituter.stringCopy(displayName,sn);
		stringSubstituter.stringInit(binaryPathName);
		hSCM = NULL;
		desktopService = false;
	} ;
	
	virtual ~ServiceManagerData()
	{
		stringSubstituter.stringDelete(serviceName);
		stringSubstituter.stringDelete(binaryPathName);
	} ;
} ;

// ============================================================================
//
// CODE MACROS
//
// ============================================================================


// ============================================================================
//
// PUBLIC MEMBER FUNCTIONS
//
// ============================================================================


// ============================================================================
//
// MEMBER FUNCTION : ServiceManager::ServiceManager
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : constructor
//
// ARGUMENTS       : sn IN system name of service
//
// THROWS          : SrvStartException
//
// ============================================================================
ServiceManager::ServiceManager
(
	char *sn
)
throw (SrvStartException)
{
	LOGGER_LOG_DEBUG1("ServiceManager::ServiceManager(%s)",sn)

	// create local data
	serviceManagerData = new ServiceManagerData(sn);

	// open the SCM for this computer
	LOGGER_LOG_DEBUG("opening SCM for create service")
	serviceManagerData->hSCM = OpenSCManager(
		NULL,                      // this computer
		NULL,                      // SERVICES_ACTIVE_DATABASE
		SC_MANAGER_CREATE_SERVICE  // allow creation of service
	);
	if(serviceManagerData->hSCM==NULL)
	{
		// failed to open handle to SCM
		LOGGER_LOG_ERROR1("ServiceManager(): failed to open SCM, error = %d",GetLastError)
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_GENERAL_ERROR,"ServiceManager","ServiceManager")
	}
	LOGGER_LOG_DEBUG("opened SCM for create service")

	// done
	return;
}

// ============================================================================
//
// MEMBER FUNCTION : ServiceManager::install
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : create a service with the supplied name and binary path.
//
// THROWS          : SrvStartException
//
// ============================================================================
void ServiceManager::install() throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("install()")

	// can the service interact with the desktop?
	DWORD serviceType = SERVICE_WIN32_OWN_PROCESS |
						(serviceManagerData->desktopService?SERVICE_INTERACTIVE_PROCESS:0);

	// create the service
	LOGGER_LOG_DEBUG3("creating service '%s' (%s) path '%s'",
					serviceManagerData->serviceName,
					serviceManagerData->displayName,
					serviceManagerData->binaryPathName)

	SC_HANDLE hService = CreateService(
		serviceManagerData->hSCM,           // Service Control Manager 
		serviceManagerData->serviceName,    // system name of service
		serviceManagerData->displayName,    // system name
		STANDARD_RIGHTS_REQUIRED,           // type of access to service
		serviceType,                        // type of service
		SERVICE_DEMAND_START,               // manual service start
		SERVICE_ERROR_IGNORE,               // carry on if service fails to start
		serviceManagerData->binaryPathName, // pointer to name of binary file
		NULL,                               // no load ordering group
		NULL,                               // no tag identifier (load ordering)
		NULL,                               // no dependencies
		NULL,                               // start using LocalSystem
		NULL                                // start using LocalSystem
	);

	// did this work?
	if(hService!=NULL)
	{
		// it worked
		if(serviceManagerData->desktopService)
		{
		LOGGER_LOG_INFO3("Successfully created desktop service '%s' (name %s) path '%s'",
						serviceManagerData->serviceName,
						serviceManagerData->displayName,
						serviceManagerData->binaryPathName)
		}
		else
		{
		LOGGER_LOG_INFO3("Successfully created non-desktop service '%s' (name %s) path '%s'",
						serviceManagerData->serviceName,
						serviceManagerData->displayName,
						serviceManagerData->binaryPathName)
		}
	}
	else
	{
		// failed to create service
		int error=GetLastError();
		LOGGER_LOG_ERROR4("Failed to create service '%s' (name %s) path '%s' (error %d)",
						serviceManagerData->serviceName,
						serviceManagerData->displayName,
						serviceManagerData->binaryPathName,
						error)

		// get details of the error
		char *msg = new char[SRVSTART_EXCEPTION_STRING_SIZE];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,error,0,msg,SRVSTART_EXCEPTION_STRING_SIZE,NULL);
		LOGGER_LOG_ERROR1("system message is '%s'",msg)
		delete[] msg;

		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_GENERAL_ERROR,"ServiceManager","install")
	}
}

// ============================================================================
//
// MEMBER FUNCTION : ServiceManager::remove
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : remove the named service
//
// ARGUMENTS       : ignore_errors IN if true, ignore "service not found" errors
//
// THROWS          : SrvStartException
//
// ============================================================================
void ServiceManager::remove
(
	bool ignore_errors
) throw (SrvStartException)
{
	LOGGER_LOG_DEBUG("remove()")

	// open the service
	SC_HANDLE hService = OpenService(
		serviceManagerData->hSCM,        // Service Control Manager 
		serviceManagerData->serviceName, // system name of service
		DELETE                           // type of access to service
	);

	// did this work?
	if(hService!=NULL)
	{
		// it worked
		LOGGER_LOG_DEBUG1("open service '%s' succeeded",serviceManagerData->serviceName)
	}
	else
	{
		// failed to open service
		if(!ignore_errors)
		{
			// failed to open service
			int error=GetLastError();
			LOGGER_LOG_ERROR2("Failed to open service '%s' (error %d)",
							serviceManagerData->serviceName,error)

			// get details of the error
			char *msg = new char[SRVSTART_EXCEPTION_STRING_SIZE];
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,error,0,msg,SRVSTART_EXCEPTION_STRING_SIZE,NULL);
			LOGGER_LOG_ERROR1("system message is '%s'",msg)
			delete[] msg;

			THROW_SRVSTART_EXCEPTION
				(SRVSTART_EXCEPTION_GENERAL_ERROR,"ServiceManager","remove")
		}
		else
		{
			LOGGER_LOG_DEBUG2("failed to open service '%s', error = %d (ignoring)",
								serviceManagerData->serviceName,GetLastError)
		}
	}

	// now delete the service
	if(DeleteService(hService)!=0)
	{
		// it worked
		LOGGER_LOG_INFO1("Deletion of service '%s' succeeded",serviceManagerData->serviceName)
	}
	else
	{
		// failed to delete service
		int error=GetLastError();
		LOGGER_LOG_ERROR2("Failed to delete service '%s' (error %d)",
						serviceManagerData->serviceName,error)

		// get details of the error
		char *msg = new char[SRVSTART_EXCEPTION_STRING_SIZE];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,error,0,msg,SRVSTART_EXCEPTION_STRING_SIZE,NULL);
		LOGGER_LOG_ERROR1("system message is '%s'",msg)
		delete[] msg;

		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_GENERAL_ERROR,"ServiceManager","remove")
	}

}

// ============================================================================
//
// MEMBER FUNCTION : ServiceManager::get|setDesktopService
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : can this service interact with the desktop?
//
// ARGUMENTS       : as below
//
// THROWS          : n/a
//
// ============================================================================
void ServiceManager::setDesktopService(bool ds) { serviceManagerData->desktopService = ds ; }
bool ServiceManager::getDesktopService() const { return serviceManagerData->desktopService; }

// ============================================================================
//
// MEMBER FUNCTION : ServiceManager::get|setDisplayName
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : display name for service (as appears in Control Panel|Services)
//
// ARGUMENTS       : as below
//
// THROWS          : n/a
//
// ============================================================================
void ServiceManager::setDisplayName(char *dn)
{
	serviceManagerData->stringSubstituter.stringCopy(serviceManagerData->serviceName,dn);
}

char *ServiceManager::getDisplayName() const { return serviceManagerData->serviceName; }

// ============================================================================
//
// MEMBER FUNCTION : ServiceManager::get|setBinaryPath
//                   ServiceManager::addBinaryPathParameter
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : binary path (ie executable and parameters)
//
// ARGUMENTS       : as below
//
// THROWS          : n/a
//
// ============================================================================
void ServiceManager::setBinaryPath(char *bp)
{
	LOGGER_LOG_DEBUG1("setBinaryPath(%s)",bp)
	serviceManagerData->stringSubstituter.stringCopy(serviceManagerData->binaryPathName,bp);
}
void ServiceManager::addBinaryPathParameter(char *bpp)
{
	serviceManagerData->stringSubstituter.stringAppend(serviceManagerData->binaryPathName," ");
	serviceManagerData->stringSubstituter.stringAppend(serviceManagerData->binaryPathName,bpp);
	LOGGER_LOG_DEBUG1("addBinaryPathParameter(): binary path is now '%s'",serviceManagerData->binaryPathName)

}

char *ServiceManager::getBinaryPath() const { return serviceManagerData->binaryPathName; }

// ============================================================================
//
// MEMBER FUNCTION : ServiceManager::~ServiceManager
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : destructor
//
// ARGUMENTS       : 
//
// ============================================================================
ServiceManager::~ServiceManager()
{
	LOGGER_LOG_DEBUG("ServiceManager::~ServiceManager()")
	delete serviceManagerData;

}

// ============================================================================
//
// STATIC MEMBER FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// PRIVATE MEMBER FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// MEMBER FUNCTION : ServiceManager::XXXX
//
// ACCESS SPECIFIER: private
//
// DESCRIPTION     : 
//
// THROWS          : SrvStartException
//
// ============================================================================

// ============================================================================
//
// LOCAL UTILITY FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// LOCAL FUNCTION  : XXXX
//
// DESCRIPTION     : 
//
// ARGUMENTS       : 
//
// THROWS          : SrvStartException
//
// ============================================================================
