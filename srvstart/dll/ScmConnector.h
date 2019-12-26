// ============================================================================
//
// FILE        : ScmConnector.h
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : interface definition for exported class ScmConnector
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

#if !defined(__SCM_CONNECTOR_H__)
#define __SCM_CONNECTOR_H__

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
#pragma message("exporting ScmConnector")

#else

#ifdef	SRVSTART_DLL_LOCAL
#pragma message("ScmConnector is local")
#define	SRVSTART_DLL_API

#else

#define SRVSTART_DLL_API __declspec(dllimport)
#pragma message("importing ScmConnector")

#endif
#endif

// ============================================================================
//
// PROJECT HEADER FILES
//
// ============================================================================
// namespace header
#include "SrvStart.h"

// ============================================================================
//
// NAMESPACE
//
// ============================================================================

// all the DLL classes are defined within the SrvStart namespace
namespace SrvStart {
	
// ============================================================================
//
// ScmConnector class
//
// ============================================================================
class SRVSTART_DLL_API ScmConnector
{
public:
	// supported statuses
	typedef enum SCM_STATUSES { STATUS_INITIALISING,STATUS_STARTING,STATUS_RUNNING,STATUS_STOPPING,
								STATUS_STOPPED,STATUS_MUST_START_AS_CONSOLE,STATUS_FAILED };

	// constructor
	ScmConnector(char *svcName,bool allowConnectErrors = false) throw (SrvStartException);

	// destructor
	virtual ~ScmConnector();

	// service status
	void notifyScmStatus(SCM_STATUSES scmStatus,bool ignoreErrors = false) throw (SrvStartException);
	SCM_STATUSES getScmStatus() const;

	// action to take if STOP requested by SCM
	typedef void STOP_HANDLER_FUNCTION(void*);
	void installStopCallback(bool *stopRequestedVar) throw (SrvStartException);
	void installStopCallback(HANDLE *stopRequestedEvent) throw (SrvStartException);
	void installStopCallback(STOP_HANDLER_FUNCTION *stopRequestedFunction,void *genericPointer)
		throw (SrvStartException);

private: // no default constructor
	ScmConnector();

};

} // namespace SrvStart

#endif // !defined(__SCM_CONNECTOR_H__)
