// ============================================================================
//
// FILE        : ServiceManager.h
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : interface definition for exported class ServiceManager
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

#if !defined(__SERVICE_MANAGER_H__)
#define __SERVICE_MANAGER_H__

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
#pragma message("exporting ServiceManager")

#else

#ifdef	SRVSTART_DLL_LOCAL
#pragma message("ServiceManager is local")
#define	SRVSTART_DLL_API

#else

#define SRVSTART_DLL_API __declspec(dllimport)
#pragma message("importing ServiceManager")

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
struct ServiceManagerData;

// ============================================================================
//
// NAMESPACE
//
// ============================================================================

// all the DLL classes are defined within the SrvStart namespace
namespace SrvStart {

// ============================================================================
//
// ServiceManager class
//
// ============================================================================
class SRVSTART_DLL_API ServiceManager
{
public:
	// public types

	// install / remove
	void install() throw (SrvStartException);
	void remove(bool ignore_errors=false) throw (SrvStartException);

	// set / get properties
	void setDesktopService(bool ds);
	void ServiceManager::setDisplayName(char *dn);
	void ServiceManager::setBinaryPath(char *bp);
	void ServiceManager::addBinaryPathParameter(char *bpp);

	bool  getDesktopService() const;
	char *ServiceManager::getDisplayName() const;
	char *ServiceManager::getBinaryPath() const;

	// constructor and destructor
	ServiceManager(char *sn) throw (SrvStartException);
	virtual ~ServiceManager();

private:	// member functions: internals

private:	// data members - hidden data
	struct ServiceManagerData *serviceManagerData;

};

} // namespace SrvStart

#endif // !defined(__SERVICE_MANAGER_H__)

