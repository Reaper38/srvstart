// ============================================================================
//
// FILE        : SrvStart.h
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : definition of globals in exported namespace SrvStart
//
//               Refer to services.htm for description and instructions for use.
//               You can obtain this file on the Web at
//
//                         http://www.nick.rozanski.com/services.htm
//
// MODIFICATION HISTORY
// --------------------
//
// Version   Date       Author          Comment
// -------   ----       ------          -------
//     1.0   31-Mar-00  Nick Rozanski   rewritten as DLL and EXE in C++ from SYBSTART;
//                                      some new functionality
//     1.1   30-Sep-00  Nick Rozanski   some new functionality
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

#if !defined(__SRV_START_H__)
#define __SRV_START_H__
// suppress warnings about the unsupported throw(...,...,...) syntax
#pragma warning(disable:4290)

// ============================================================================
//
// VERSION CONTROL
//
// ============================================================================

#define APPLICATION		"SRVSTART"
#define	VERSION			"1.20"

#define	COPYRIGHT		"Copyright (C) 1998 - 2000 Nick Rozanski (Nick@Rozanski.com)"
#define	DISTRIBUTION	"Distributed under the terms of the GNU General Public License as published by the Free Software Foundation"
#define	WARRANTY		"Distributed WITHOUT ANY WARRANTY whatsoever."

// export or import these symbols
// NB an invoking source file should #undef SRVSTART_DLL_EXPORT

// ============================================================================
//
// IMPORT / EXPORT
//
// ============================================================================

#ifdef SRVSTART_DLL_EXPORT
#define SRVSTART_DLL_API __declspec(dllexport)
#pragma message("exporting SrvStart")

#else

#define SRVSTART_DLL_API __declspec(dllimport)
#pragma message("importing SrvStart")

#endif

// ============================================================================
//
// NAMESPACE
//
// ============================================================================

// all the DLL classes are defined within the SrvStart namespace
namespace SrvStart {
	
	// version information
	const SRVSTART_DLL_API char *getApplication();
	const SRVSTART_DLL_API char *getVersion();

	const SRVSTART_DLL_API char *getCopyright();
	const SRVSTART_DLL_API char *getDistribution();
	const SRVSTART_DLL_API char *getWarranty();

	typedef enum SRVSTART_EXCEPTION
	{
		SRVSTART_EXCEPTION_COMMAND_FAILED,
		SRVSTART_EXCEPTION_CREATE_PROCESS_FAILED,
		SRVSTART_EXCEPTION_WAIT_FAILED,
		SRVSTART_EXCEPTION_WATCH_FAILED,
		SRVSTART_EXCEPTION_KILL_FAILED,
		SRVSTART_EXCEPTION_NOTIFY_FAILED,
		SRVSTART_EXCEPTION_INVALID_PARAMETER,
		SRVSTART_EXCEPTION_GENERAL_ERROR
	} ;

	const int SRVSTART_EXCEPTION_STRING_SIZE = 1000;
	class SRVSTART_DLL_API SrvStartException
	{
	public:
		// data members
		SRVSTART_EXCEPTION exceptionId;
		char className[SRVSTART_EXCEPTION_STRING_SIZE];
		char methodName[SRVSTART_EXCEPTION_STRING_SIZE];
		char errorMessage[SRVSTART_EXCEPTION_STRING_SIZE];
		char sourceFile[SRVSTART_EXCEPTION_STRING_SIZE];
		int  lineNumber;
		// constructor
		SrvStartException
		(
			SRVSTART_EXCEPTION pExceptionId,
			char *pClassName,
			char *pMethodName,
			char *pSourceFile,
			int   pLineNumber
		);
		virtual ~SrvStartException() { ; }
	private:
		// no default constructor
		SrvStartException() { ; }
	} ;


} // namespace SrvStart

//
// general-purpose macro for throwing exceptions
//
#define	THROW_SRVSTART_EXCEPTION(id,cl,mt)								\
	LOGGER_LOG_ERROR3("Exception %d in Class '%s' method '%s()'",id,cl,mt)	\
	throw SrvStartException(id,cl,mt,__FILE__,__LINE__);

#endif // !defined(__SRV_START_H__)
