// ============================================================================
//
// FILE        : Validation.h
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : interface definition for Validation class
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

// prevent multiple inclusion

#if !defined(__VALIDATION_H__)
#define __VALIDATION_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Validation  
{
public:
	// validation routines
	bool isInteger(char str[]) const;
	bool isRegularFile(char str[]) const;
	bool isDirectory(char str[]) const;
	bool isLikeYes(char str[]) const;

	// constructor / destructor
	Validation();
	virtual ~Validation();
};

#endif // !defined(__VALIDATION_H__)
