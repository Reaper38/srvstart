// ============================================================================
//
// FILE        : SrvStart.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : Implementation of globals in exported namespace SrvStart
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

// we are exporting the namespace globals
#define	SRVSTART_DLL_EXPORT

// ============================================================================
//
// PROJECT HEADER FILES
//
// ============================================================================

// system headers
#include <string>

// support headers
#include <logger.h>

// class headers
#include "SrvStart.h"

// ============================================================================
//
// EMBED VERSION INFORMATION
//
// ============================================================================

#ifdef _DEBUG
#define	SRVSTART_VERSION_STRING	"*** " APPLICATION " version " VERSION " [debug] " COPYRIGHT
#else
#define	SRVSTART_VERSION_STRING	"*** " APPLICATION " version " VERSION " " COPYRIGHT
#endif
static char __version[] = SRVSTART_VERSION_STRING;
#pragma message(SRVSTART_VERSION_STRING)

// ============================================================================
//
// NAMESPACE DECLARATIONS
//
// ============================================================================

using namespace SrvStart;

// ============================================================================
//
// EXCEPTION MESSAGES
//
// ============================================================================
#define	COMMAND_FAILED_MESSAGE \
			"Failed to execute a command or program.  Check the program name syntax and parameters."

#define	CREATE_PROCESS_FAILED_MESSAGE \
			"Failed to create a process.  Check the program name, syntax and the PATH variable."

#define	WAIT_FAILED_MESSAGE \
			"Failed to create an event to wait (sleep).  Check system resources."

#define	WATCH_FAILED_MESSAGE \
			"Failed to watch an executing process."

#define	KILL_FAILED_MESSAGE \
			"Failed to kill an executing process."

#define	NOTIFY_FAILED_MESSAGE \
			"Failed to notify the Service Control Manager of the current status of the service."

#define	INVALID_PARAMETER_MESSAGE \
			"An invalid parameter was supplied to a function or method."

#define	GENERAL_ERROR_MESSAGE \
			"An internal error has occurred."

#define	DEFAULT_MESSAGE \
			"An internal error has occurred."

// ============================================================================
//
// GLOBAL NAMESPACE FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// NAMESPACE FUNCTION : SrvStart::<version methods>
//
// ACCESS SPECIFIER   : global
//
// DESCRIPTION        : get version information
//
// RETURNS            : version information
//
// ============================================================================

const SRVSTART_DLL_API char * SrvStart::getApplication() { return APPLICATION ; }
const SRVSTART_DLL_API char * SrvStart::getVersion() { return VERSION ; }

const SRVSTART_DLL_API char * SrvStart::getCopyright() { return COPYRIGHT ; }
const SRVSTART_DLL_API char * SrvStart::getDistribution() { return DISTRIBUTION ; }
const SRVSTART_DLL_API char * SrvStart::getWarranty() { return WARRANTY ; }

// ============================================================================
//
// PUBLIC MEMBER FUNCTIONS
//
// ============================================================================

// ============================================================================
//
// MEMBER FUNCTION : SrvStartException::SrvStartException
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : constructor
//
// ARGUMENTS       : pExceptionId  IN exception id
//                   pClassName    IN name of class in which exception occured
//                   pMethodName   IN name of method in which exception occured
//                   pSourceFile   IN source file in which exception occured
//                   pLineNumber   IN line number at which exception occured
//
// ============================================================================
SrvStartException::SrvStartException
(
	SRVSTART_EXCEPTION pExceptionId,
	char *pClassName,
	char *pMethodName,
	char *pSourceFile,
	int   pLineNumber
)
{
	LOGGER_LOG_DEBUG1("SrvStartException::SrvStartException(%d)",pExceptionId)

	// copy input
	exceptionId = pExceptionId;
	strncpy(className,pClassName,sizeof(className)-1);
	strncpy(methodName,pMethodName,sizeof(methodName)-1);
	strncpy(sourceFile,pSourceFile,sizeof(sourceFile)-1);
	lineNumber = pLineNumber;

	// construct error message
	switch(exceptionId)
	{
		case SRVSTART_EXCEPTION_COMMAND_FAILED:
			strncpy(errorMessage,COMMAND_FAILED_MESSAGE,sizeof(errorMessage)-1);
			break;

		case SRVSTART_EXCEPTION_CREATE_PROCESS_FAILED:
			strncpy(errorMessage,CREATE_PROCESS_FAILED_MESSAGE,sizeof(errorMessage)-1);
			break;

		case SRVSTART_EXCEPTION_WAIT_FAILED:
			strncpy(errorMessage,WAIT_FAILED_MESSAGE,sizeof(errorMessage)-1);
			break;

		case SRVSTART_EXCEPTION_WATCH_FAILED:
			strncpy(errorMessage,WATCH_FAILED_MESSAGE,sizeof(errorMessage)-1);
			break;

		case SRVSTART_EXCEPTION_KILL_FAILED:
			strncpy(errorMessage,KILL_FAILED_MESSAGE,sizeof(errorMessage)-1);
			break;

		case SRVSTART_EXCEPTION_NOTIFY_FAILED:
			strncpy(errorMessage,NOTIFY_FAILED_MESSAGE,sizeof(errorMessage)-1);
			break;

		case SRVSTART_EXCEPTION_INVALID_PARAMETER:
			strncpy(errorMessage,INVALID_PARAMETER_MESSAGE,sizeof(errorMessage)-1);
			break;

		case SRVSTART_EXCEPTION_GENERAL_ERROR:
			strncpy(errorMessage,GENERAL_ERROR_MESSAGE,sizeof(errorMessage)-1);
			break;

		default:
			strncpy(errorMessage,DEFAULT_MESSAGE,sizeof(errorMessage)-1);
			break;
	}

}
