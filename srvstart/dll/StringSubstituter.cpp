// ============================================================================
//
// FILE        : StringSubstituter.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : Implementation of StringSubstituter class
//
//               This class is a bit of an abomination.  Sorry.
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
#include <stdlib.h>
#include <iostream>
#include <fstream>
using namespace std;
#include <conio.h>

// support headers
#include <logger.h>

// class headers
#include "StringSubstituter.h"

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

const char INPUT_START		= '{';
const char INPUT_SEPARATOR	= ':';
const char INPUT_END		= '}';
const char HIDDEN_INDICATOR	= '-';

const char ENV_START		= '%';
const char ENV_END			= '%';

// ============================================================================
//
// PUBLIC MEMBER FUNCTIONS
//
// ============================================================================


// ============================================================================
//
// MEMBER FUNCTION : StringSubstituter::StringSubstituter
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : constructors
//
// ARGUMENTS       : bufsize IN buffer size
//
// ============================================================================
StringSubstituter::StringSubstituter(int bufSize)
{
	_bufSize   = bufSize;
	buf        = new char[_bufSize];
	tmpString1 = new char[_bufSize];
	tmpString2 = new char[_bufSize];
}

// ============================================================================
//
// MEMBER FUNCTION : StringSubstituter::~StringSubstituter
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : destructor
//
// ============================================================================
StringSubstituter::~StringSubstituter()
{
	delete []buf;
	delete []tmpString1;
	delete []tmpString2;
}

// ============================================================================
//
// MEMBER FUNCTION : StringSubstituter::stringInit
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : initialise string pointer
//
// ARGUMENTS       : str INOUT pointer to initialise
//
// ============================================================================
void StringSubstituter::stringInit
(
	char *&str
)
{
	// LOGGER_LOG_DEBUG("stringInit()")
	str = 0;
}

// ============================================================================
//
// MEMBER FUNCTION : StringSubstituter::stringCopy
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : garbage-collecting version of strcpy
//
//                   This function copies the string in src to dest.  If dest
//                   is not NULL, it frees the storage pointed to by dest.  It
//                   then allocates exactly enough storage, storing the result
//                   in dest, and copies src to dest.
//
// ARGUMENTS       : dest INOUT where to copy to (may be NULL or empty)
//                   src  IN    where to copy from
//
// ============================================================================
void StringSubstituter::stringCopy
(
	char       *&dest,
	const char  *src
)
{
	// LOGGER_LOG_DEBUG("stringCopy()")

	// has dest already been allocated?
	if(dest!=0)
	{
		// yes, it has
		// LOGGER_LOG_DEBUG1("stringCopy(): deleting dest (%p)",dest)
		delete[] dest;
	}

	// allocate new storage
	int len = strlen(src);
	// LOGGER_LOG_DEBUG1("stringCopy(): allocating %d+1 bytes",len)
	dest=new char[len+1];

	// copy string
	strcpy(dest,src);
	// LOGGER_LOG_DEBUG1("stringCopy(): dest is now '%s'",dest)
}

// ============================================================================
//
// MEMBER FUNCTION : StringSubstituter::stringAppend
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : garbage-collecting version of strcat
//
//                   This function appends the string in src to dest.  If dest
//                   is not NULL, it frees the storage pointed to by dest.  It
//                   then allocates exactly enough storage, storing the result
//                   in dest, and appends src to dest.
//
// ARGUMENTS       : dest INOUT where to copy to (may be NULL or empty)
//                   src  IN    where to copy from
//
// ============================================================================
void StringSubstituter::stringAppend
(
	char       *&dest,
	const char  *src,
	bool         addSpace
)
{
	// LOGGER_LOG_DEBUG("stringAppend")

	// has dest been allocated?
	if(dest!=0)
	{
		// yes, it has
		int len = strlen(dest)+strlen(src)+(addSpace?1:0);
		// LOGGER_LOG_DEBUG1("stringAppend(): reallocating %d bytes",len+1)
		char *tmp = dest;
		dest = new char[len+1];
		strcpy(dest,tmp);
		if(addSpace) { strcat(dest," "); }
		strcat(dest,src);
		// LOGGER_LOG_DEBUG1("stringAppend(): deleting old dest (%p)",tmp)
		delete[] tmp;
	}
	else
	{
		// no, it hasn't
		int len = strlen(src);
		// LOGGER_LOG_DEBUG1("stringAppend(): allocating %d bytes",len+1)
		dest = new char[len+1];
		// copy string
		strcpy(dest,src);
	}
	// LOGGER_LOG_DEBUG1("stringAppend(): dest is now '%s'",dest)

}

// ============================================================================
//
// MEMBER FUNCTION : StringSubstituter::stringDelete
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : delete storage pointed to by string if not NULL
//
// ARGUMENTS       : str INOUT storage to delete
//
// ============================================================================
void StringSubstituter::stringDelete
(
	char *str
)
{
	if(str!=0)
	{
		// LOGGER_LOG_DEBUG1("stringDelete(): deleting string (%p)",str)
		delete[] str;
	}
}

// ============================================================================
//
// MEMBER FUNCTION : StringSubstituter::stringSubstitute
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : substitute into given string buffer
//
// ARGUMENTS       : subBuf INOUT buffer to substitute into
//
// ============================================================================
void StringSubstituter::stringSubstitute
(
	char *&subBuf
)
{
	char *inCh  = subBuf;
	char *outCh = buf;
	char *tmpCh, *envCh;
	char  ch;
	bool  hiddenReply;
	
	LOGGER_LOG_DEBUG1("stringSubstitute: input string is '%s'",subBuf)
	
	while(true)
	{
		// read next character
		ch = (*inCh++);
		// what is it?
		switch(ch)
		{
			case '\0':
				// end of string
				(*outCh) = '\0';
				goto END_OF_INPUT;
				
			case INPUT_START:
				// substitute from stdin
				LOGGER_LOG_DEBUG1("stringSubstitute: stdin substitution at '%s'",inCh-1)
				// is this hidden text?
				if((*inCh)==HIDDEN_INDICATOR)
				{
					// yes, it is
					LOGGER_LOG_DEBUG("stringSubstitute: hidden text indicated")
					hiddenReply = true;
					inCh++;
				}
				else
				{
					// no, it isn't
					LOGGER_LOG_DEBUG("stringSubstitute: hidden text NOT indicated")
					hiddenReply = false;
				}

				// get prompt
				tmpCh = tmpString1;
				while(((*inCh)!=INPUT_SEPARATOR)&&((*inCh)!=INPUT_END)&&((*inCh)!='\0'))
				{
					// read the prompt
					(*tmpCh++) = (*inCh);
					if((*inCh)!='\0') { inCh++; }
				}
				// add terminator
				(*tmpCh) = '\0';
				bool foundSeparator;
				foundSeparator = ((*inCh)== INPUT_SEPARATOR);
				if((*inCh)!='\0') { inCh++; }
				LOGGER_LOG_DEBUG1("stringSubstitute: prompt is '%s'",tmpString1)

				if(foundSeparator)
				{
					// get default
					tmpCh = tmpString2;
					while(((*inCh)!=INPUT_END)&&((*inCh)!='\0'))
					{
						// read the default
						(*tmpCh++) = (*inCh);
						if((*inCh)!='\0') { inCh++; }
					}
					// add terminator
					(*tmpCh) = '\0';
					if((*inCh)!='\0') { inCh++; }
					LOGGER_LOG_DEBUG1("stringSubstitute: default is '%s'",tmpString2)
				}
				else
				{
					// no default provided
					tmpString2[0] = '\0';
				}
				
				// display prompt to stdout
				cout << tmpString1 << " [" << tmpString2 << "]: "; cout.flush();

				// read input
				if(hiddenReply)
				{
					// do not echo the entered reply (eg a password)
					// have to read input directly from console (without echo)
					// IS THERE A BETTER WAY TO DO THIS?
					tmpCh = tmpString1;
					while(true)
					{
						// get the next character from the console
						char ch = _getch();
						if(ch==13) { (*tmpCh) = '\0'; cout << '\n'; cout.flush(); break; }
						(*tmpCh++) = ch;
					}
				}
				else
				{
					// read input from stdin
					cin.getline(tmpString1,_bufSize);
				}
				LOGGER_LOG_DEBUG1("entered reply is '%s'",tmpString1)
				
				// if reply is empty, use default
				if (tmpString1[0] == '\0')
				{
					strcpy(tmpString1,tmpString2);
					LOGGER_LOG_DEBUG1("using default reply '%s'",tmpString1)
				}

				// copy reply string into output buffer
				tmpCh = tmpString1;
				while ((*tmpCh)!='\0') { (*outCh++) = (*tmpCh++); }

				// that's it
				break;

			case ENV_START:
				// substitute from stdin
				LOGGER_LOG_DEBUG1("stringSubstitute: environment substitution at '%s'",inCh-1)
				// get environment variable
				tmpCh = tmpString1;
				while(((*inCh)!=ENV_END)&&((*inCh)!='\0'))
				{
					// read the environment variable
					(*tmpCh++) = (*inCh);
					if((*inCh)!='\0') { inCh++; }
				}
				// add terminator
				(*tmpCh) = '\0';
				if((*inCh)!='\0') { inCh++; }
				LOGGER_LOG_DEBUG1("stringSubstitute: environment variable is '%s'",tmpString1)
				
				// get environment value
				envCh = tmpString2;
				if(GetEnvironmentVariable(tmpString1,envCh,_bufSize)==0)
				{
					LOGGER_LOG_INFO1("warning: unable to substitute environment variable '%s' (using blank)",tmpString1)
					(*envCh) = '\0';
				}
				LOGGER_LOG_DEBUG1("substitute: environment value is '%s'",envCh)

				// copy environment value into output buffer
				while ((*envCh)!='\0') { (*outCh++) = (*envCh++); }

				break;

			default:
				// ordinary character
				(*outCh++) = ch;

		}
	}
END_OF_INPUT:
	// copy processed string back into input
	LOGGER_LOG_DEBUG("stringSubstitute: finished loop")
	stringCopy(subBuf,buf);
	LOGGER_LOG_DEBUG1("stringSubstitute: output string is '%s'",subBuf)

}
