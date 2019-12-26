// ============================================================================
//
// FILE        : StringSubstituter.h
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : interface definition for StringSubstituter class
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

#if !defined(__STRING_SUBSTITUTER_H__)
#define __STRING_SUBSTITUTER_H__

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
#pragma message("exporting StringSubstituter")

#else

#ifdef	SRVSTART_DLL_LOCAL
#pragma message("StringSubstituter is local")
#define	SRVSTART_DLL_API

#else

#define SRVSTART_DLL_API __declspec(dllimport)
#pragma message("importing StringSubstituter")

#endif
#endif

// ============================================================================
//
// NAMESPACE
//
// ============================================================================

// all the DLL classes are defined within the SrvStart namespace
namespace SrvStart {
const int STRING_SUBSTITUTER_DEFAULT_BUFSIZE = 5000;

// ============================================================================
//
// StringSubstituter class
//
// ============================================================================
class SRVSTART_DLL_API StringSubstituter {
public:
	// string manipulations
	void stringInit(char *&str);
	void stringCopy(char *&dest, const char*src);
	void stringAppend(char *&dest, const char*src,bool addSpace=false);
	void stringDelete(char *str);

	// substitute environment values into string
	void stringSubstitute(char *&subBuf);
	
	// constructor and destructor
	StringSubstituter(int bufSize = STRING_SUBSTITUTER_DEFAULT_BUFSIZE);
	virtual ~StringSubstituter();

private:
	int _bufSize;
	char *buf;
	char *tmpString1;
	char *tmpString2;

	
};

} // namespace SrvStart

#endif // !defined(__STRING_SUBSTITUTER_H__)
