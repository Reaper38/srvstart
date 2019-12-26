// ============================================================================
//
// FILE        : ArgumentList.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : implementation of ArgumentList class
//
// MODIFICATION HISTORY
// --------------------
//
//  Refer to master source file exe.cpp for full modification history.
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

// ============================================================================
//
// PROJECT HEADER FILES
//
// ============================================================================

// system headers
#include <stdlib.h>
#include <string>
#include <Windows.h>
#include <windef.h>
#include <WinBase.h>

// support headers
#include <logger.h>

// class header
#include "Validation.h"
#include "ArgumentList.h"

// ============================================================================
//
// PUBLIC MEMBER FUNCTIONS
//
// ============================================================================


// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::ArgumentList
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : constructor
//
// ARGUMENTS       : argc, argv IN command-line arguments
//
// ============================================================================
ArgumentList::ArgumentList
(
	int argc,
	char* argv[]
)
{
	_argc  = argc;
	_argv  = argv;
	argIdx = 1;		// index 0 is calling program name
	argCh  = 0;

}

// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::~ArgumentList
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : destructor
//
// ============================================================================
ArgumentList::~ArgumentList() { ; }

// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::peekNextArgument (1st variant)
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : peek at next argument but do not advance argument pointer
//
// ARGUMENTS       : argumentType            OUT type of next argument
//                   argumentValidation      IN  validation to be performed
//                   isValid                 OUT results of validation
//                   nextArgument            OUT pointer to next argument
//                   argumentTransformation  IN  transformation to be performed
//
// ============================================================================
void ArgumentList::peekNextArgument
(
	ArgumentTypes           &argumentType,
	ArgumentValidations      argumentValidation,
	bool                    &isValid,
	char                    *nextArgument,
	ArgumentTransformations  argumentTransformation
)
{
	getNextArgument(
		nextArgument,
		argumentTransformation,
		argumentValidation,
		false,
		argumentType,
		isValid
	);
	LOGGER_LOG_DEBUG1("ArgumentList::peekNextArgument(): argIdx is now %d",argIdx)
}

// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::peekNextArgument (2nd variant)
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : peek at next argument but do not advance argument pointer
//
// ARGUMENTS       : argumentType            OUT type of next argument
//                   nextArgument            OUT pointer to next argument
//                   argumentTransformation  IN  transformation to be performed
//
// ============================================================================
void ArgumentList::peekNextArgument
(
	ArgumentTypes           &argumentType,
	char                    *nextArgument,
	ArgumentTransformations  argumentTransformation
)
{
	bool isValid;
	getNextArgument(nextArgument,argumentTransformation,AL_ANY,false,argumentType,isValid);
	LOGGER_LOG_DEBUG1("ArgumentList::peekNextArgument(): argIdx is now %d",argIdx)
}

// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::popNextArgument (1st variant)
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : pop next argument and advance argument pointer
//
// ARGUMENTS       : argumentType            OUT type of next argument
//                   argumentValidation      IN  validation to be performed
//                   isValid                 OUT results of validation
//                   nextArgument            OUT pointer to next argument
//                   argumentTransformation  IN  transformation to be performed
//
// ============================================================================
void ArgumentList::popNextArgument
(
	ArgumentTypes           &argumentType,
	ArgumentValidations      argumentValidation,
	bool                    &isValid,
	char                    *nextArgument,
	ArgumentTransformations  argumentTransformation
)
{
	getNextArgument(
		nextArgument,
		argumentTransformation,
		argumentValidation,
		true,
		argumentType,
		isValid
	);
	LOGGER_LOG_DEBUG1("ArgumentList::popNextArgument(): argIdx is now %d",argIdx)
}

// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::popNextArgument (1st variant)
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : pop next argument and advance argument pointer
//
// ARGUMENTS       : argumentType            OUT type of next argument
//                   nextArgument            OUT pointer to next argument
//                   argumentTransformation  IN  transformation to be performed
//
// ============================================================================
void ArgumentList::popNextArgument
(
	ArgumentTypes           &argumentType,
	char                    *nextArgument,
	ArgumentTransformations  argumentTransformation
)
{
	bool isValid;
	getNextArgument(nextArgument,argumentTransformation,AL_ANY,true,argumentType,isValid);
	LOGGER_LOG_DEBUG1("ArgumentList::popNextArgument(): argIdx is now %d",argIdx)
}

// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::getNumberOfArguments
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : return total number of arguments
//
// RETURNS         : total number of arguments
//
// ============================================================================
int ArgumentList::getNumberOfArguments() const { return _argc; }

// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::getArgv0
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : return argv[0] the ie name of executable
//
// ARGUMENTS       : argv0 OUT name of executable
//
// RETURNS         : n/a
//
// ============================================================================
void ArgumentList::getArgv0
(
	char *argv0
) const
{
	strcpy(argv0,_argv[0]);
	return;
}

// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::getArgv0
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : return full path of executable (sometimes)
//
//                   Microsoft provides us with the following "information":
//
//                   When a program is run from the command interpreter (CMD.EXE),
//                   _pgmptr is automatically initialized to the full path of the
//                   executable file. For example, if HELLO.EXE is in C:\BIN and
//                   C:\BIN is in the path, _pgmptr is set to C:\BIN\HELLO.EXE
//                   when you execute
//                      C> hello 
//
//                   When a program is not run from the command line, _pgmptr
//                   may be initialized to the program name (the file’s base
//                   name without the extension), or to a filename, a relative
//                   path, or a full path.
//
//                   Hence the return value.
//
// ARGUMENTS       : getExeFullPath OUT full path of executable
//
// RETURNS         : TRUE  valid value returned
//                   FALSE unable to determine full path
//
// ============================================================================

bool ArgumentList::getExeFullPath
(
	char *fullPath
) const
{
	bool fileFound = false;

	// use global variable _pgmptr
	//strcpy(fullPath,_pgmptr);
	GetModuleFileName(NULL, fullPath, _MAX_PATH);
	LOGGER_LOG_DEBUG1("executable path from _pgmptr is '%s'",fullPath)

	// split the output into its constituent parts if possible
	char drive[_MAX_DRIVE];
	char ext[_MAX_EXT];
	_splitpath(fullPath,drive,NULL,NULL,ext);

	// have we got a drive and extension?
	if((drive[0]!='\0')&&(ext[0]!='\0'))
	{
		// does the file exist?
		class Validation v;
		if(v.isRegularFile(fullPath))
		{
			// it does exist
			LOGGER_LOG_DEBUG1("path '%s' from _pgmptr does seem to exist",fullPath)
			fileFound = true;
		}
		else
		{
			// it doesn't exist
			LOGGER_LOG_DEBUG1("path '%s' from _pgmptr does not exist",fullPath)
		}
	}
	else
	{
		// it isn't a full path name
		LOGGER_LOG_DEBUG3("path '%s' from _pgmptr has empty drive (%s) or extension (%s)",
							fullPath,drive,ext)
	}

	// return
	if(!fileFound)
	{
		LOGGER_LOG_DEBUG("getExeFullPath() FAILED to find full path")
		(*fullPath)='\0';
	}
	else
	{
		LOGGER_LOG_DEBUG1("getExeFullPath() found full path '%s'",fullPath)
	}
	return fileFound;
}


// ============================================================================
//
// PRIVATE MEMBER FUNCTIONS
//
// ============================================================================


// ============================================================================
//
// MEMBER FUNCTION : ArgumentList::getNextArgument
//
// ACCESS SPECIFIER: private
//
// DESCRIPTION     : get and parse next argument
//
// ARGUMENTS       : nextArgument            OUT pointer to next argument
//                   argumentTransformation  IN  transformation to be performed
//                   argumentValidation      IN  validation to be performed
//                   increment               IN  true if get rather than just peek
//                   argumentType            OUT type of next argument
//                   isValid                 OUT results of validation
//
// ============================================================================
void ArgumentList::getNextArgument
(
	char                     nextArgument[],
	ArgumentTransformations  argumentTransformation,
	ArgumentValidations      argumentValidation,
	bool                     increment,
	ArgumentTypes           &argumentType,
	bool                    &isValid
)
{
	char *na, *ch;

	LOGGER_LOG_DEBUG2("getNextArgument(): argIdx = %d, argCh = %d",argIdx,argCh)

	// get the next argument
	na = (argIdx<_argc?(*(_argv+argIdx))+argCh:"");

	LOGGER_LOG_DEBUG2("next argument (index %d) is '%s'",argIdx,na)

	// what is the first character of the argument?
	switch(*na)
	{
		case '\0':
			argumentType = AL_EMPTY; // no more arguments
			if(nextArgument!=0) { (*nextArgument) = '\0'; }
			LOGGER_LOG_DEBUG("argument type is AL_EMPTY")
			break;

		case '-':
			na++; // skip the dash
			switch(*na)
			{
				case '\0':
					// this is just the single character -
					argumentType = AL_STDIN;
					// clear the output if not null
					if(nextArgument!=0) { *nextArgument = '\0'; }
					// go on to the next argument if so requested
					if(increment) { argIdx++; argCh = 0; }
					LOGGER_LOG_DEBUG("argument type is AL_STDIN")
					break;

				case '-':
					// this is a Gnu-style switch eg --help
					argumentType = AL_GNU_SWITCH;
					na++; // skip the second dash
					// copy the switch into the output if not null
					if(nextArgument!=0) { strcpy(nextArgument,na); }
					// go on to the next argument if so requested
					if(increment) { argIdx++; argCh = 0; }
					LOGGER_LOG_DEBUG("argument type is AL_GNU_SWITCH")
					break;

				default:
					// this is a traditional switch eg -h
					argumentType = AL_SWITCH;
					// is the switch value part of the same argument (eg -d1)?
					if((*(na+1))=='\0')
					{
						LOGGER_LOG_DEBUG("value is NOT part of switch")
						// copy the switch into the output if not null
						if(nextArgument!=0) { strcpy(nextArgument,na); }
						// go on to the next argument if so requested
						if(increment) { argIdx++; argCh = 0; }
					}
					else
					{
						LOGGER_LOG_DEBUG("value is part of switch")
						// put in an end of string after the switch
						if(nextArgument!=0)
						{
							*nextArgument     = (*na);
							*(nextArgument+1) = '\0';
						}
						// go on to the next argument if so requested
						if(increment) { argCh = 2; }
					}
					LOGGER_LOG_DEBUG("argument type is AL_SWITCH")
					break;
			}
			break;
		default:
			// this is a string
			argumentType = AL_STRING;
			if(nextArgument!=0) { strcpy(nextArgument,na); }
			// go on to the next argument if so requested
			if(increment) { argIdx++; argCh = 0; }
			LOGGER_LOG_DEBUG("argument type is AL_STRING")
			break;
	}

	// return if nextArgument not supplied
	if(nextArgument==0) { LOGGER_LOG_DEBUG("no transformation/validation") return; }

	// transform the argument if requested
	ch = nextArgument;
	LOGGER_LOG_DEBUG1("returned argument value is '%s'",ch)

	switch(argumentTransformation)
	{
		// convert to upper case
		case AL_TO_UPPER:
			while((*ch)!='\0') { *ch = toupper((*ch)); ch++; }
			LOGGER_LOG_DEBUG1("converted argument to upper case '%s'",nextArgument)
			break;
		// convert to upper case
		case AL_TO_LOWER:
			while((*ch)!='\0') { *ch = tolower((*ch)); ch++; }
			LOGGER_LOG_DEBUG1("converted argument to lower case '%s'",nextArgument)
			break;
		default:
			break;
	}

	// validate the argument if requested
	class Validation v;
	switch(argumentValidation)
	{
		case AL_IS_INTEGER:
			// check that the argument is a valid integer
			isValid = v.isInteger(nextArgument);
			break;

		case AL_IS_DIRECTORY:
			// check that the argument is a directory
			isValid = v.isDirectory(nextArgument);
			break;

		case AL_IS_FILE:
			// check that the argument is a regular file
			isValid = v.isRegularFile(nextArgument);
			break;

		default:
			break;
	}

}
