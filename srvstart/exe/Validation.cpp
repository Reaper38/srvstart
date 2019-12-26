// ============================================================================
//
// FILE        : Validation.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : implementation of Validation class
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
#include <ctype.h>
#include <sys/stat.h>

// support headers
#include <logger.h>

// class header
#include "Validation.h"

// ============================================================================
//
// PUBLIC MEMBER FUNCTIONS
//
// ============================================================================


// ============================================================================
//
// MEMBER FUNCTION : Validation::isInteger
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : check if string is a well-formed integer
//
// ARGUMENTS       : str IN string to check
//
// RETURNS         : true if string is a well-formed integer, else false
//
// ============================================================================
bool Validation::isInteger
(
	char str[]
) const
{
	char *ch = str;

	// reject an empty string
	if((ch)=='\0') { return false; }

	// delete leading spaces
	while((*ch)==' ') { ch++; }

	// optional leading minus
	if((*ch)=='-') { ch++; }

	// now digits
	while(isdigit(*ch)) { ch++; }

	// now trailing spaces
	while((*ch)==' ') { ch++; }

	// we should be at the end of the number
	return ((*ch)=='\0');

}

// ============================================================================
//
// MEMBER FUNCTION : Validation::isInteger
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : check if string is a regular file
//
// ARGUMENTS       : str IN string to check
//
// RETURNS         : true if string is a regular file, else false
//
// ============================================================================
bool Validation::isRegularFile
(
	char str[]
) const
{
	struct stat buf;
	stat(str,&buf);
	return (buf.st_mode&_S_IFREG)!=0;
}

// ============================================================================
//
// MEMBER FUNCTION : Validation::isInteger
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : check if string is a directory
//
// ARGUMENTS       : str IN string to check
//
// RETURNS         : true if string is a directory, else false
//
// ============================================================================
bool Validation::isDirectory
(
	char str[]
) const
{
	struct stat buf;
	stat(str,&buf);
	return (buf.st_mode&_S_IFDIR)!=0;
}

// ============================================================================
//
// MEMBER FUNCTION : Validation::isInteger
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : check if string is like the word "yes"
//
// ARGUMENTS       : str IN string to check
//
// RETURNS         : true if string is like the word yes, else false
//
// ============================================================================
bool Validation::isLikeYes
(
	char str[]
) const
{
	char *ch = str;

	// delete leading spaces
	while((*ch)==' ') { ch++; }

	// empty string means no
	if((ch)=='\0') { return false; }

	// y or Y means yes
	return ((*ch)=='y')||((*ch)=='Y');

}

// ============================================================================
//
// MEMBER FUNCTION : Validation::Validation
//                   Validation::~Validation
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : constructor / destructor
//
// ============================================================================
Validation::Validation() { }

Validation::~Validation() { }

