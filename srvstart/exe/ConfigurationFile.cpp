// ============================================================================
//
// FILE        : ConfigurationFile.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : implementation of ConfigurationFile class
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
#include <algorithm>

// support headers
#include <logger.h>

// class header
#include "ConfigurationFile.h"

// ============================================================================
//
// PUBLIC MEMBER FUNCTIONS
//
// ============================================================================


// ============================================================================
//
// MEMBER FUNCTION : ConfigurationFile::openConfigurationFile
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : open named configuration file
//
// ARGUMENTS       : configPath IN path name of configuration file
//
// RETURNS         : true if successful, false otherwise
//
// ============================================================================
bool ConfigurationFile::openConfigurationFile
(
	char configPath[]
)
{
	// open configuration file
	LOGGER_LOG_DEBUG1("opening configuration file '%s'",configPath)
	configFile = new ifstream(configPath);
	return (configFile->is_open()!=0);
}

// ============================================================================
//
// MEMBER FUNCTION : ConfigurationFile::getNextConfigurationDirective
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : get next configuration directive from currently-open file
//
// ARGUMENTS       : directive OUT directive name (empty if no more directives)
//                   value     OUT directive value
//
// ============================================================================
void ConfigurationFile::getNextConfigurationDirective
(
	char directive[],
	char value[]
)
{
	char *line,*ch;
	char  section[CFGFILE_SECTION_SIZE];
	bool  rightSection = true;

	directive[0] = '\0';
	value[0]     = '\0';


	LOGGER_LOG_DEBUG1("searching for next directive in section '%s'",_requestedSection)

	// have we opened the configuration file yet?
	if(configFile->is_open()==0)
	{
		LOGGER_LOG_DEBUG("configuration file not open ... returning")
		return;
	}

	// get the next directive / value pair
	while(true)
	{
		line = readNextRealLine();
		switch(*line)
		{
			case '\0':
				// end of file reached
				LOGGER_LOG_DEBUG("reached end of configuration file")
				return;

			case CFGFILE_SECTION_OPEN:
				// new section
				ch = section;
				line++;
				LOGGER_LOG_DEBUG1("found section '%s'",line)

				while(((*line)!=CFGFILE_SECTION_CLOSE)&&((*line)!='\0')) { (*ch++) = (*line++); }
				(*ch) = '\0';
				// if not our section, ignore
				if(_requestedSection[0]!='\0')
				{
					rightSection = (strcmp(section,_requestedSection) == 0);
				}
				else
				{
					rightSection = true;
				}
				break;

			default:
				// a directive / value pair
				if(rightSection)
				{
					// extract the directive
					ch = directive;
					while(((*line)!='=')&&((*line)!='\0')) { (*ch++) = (*line++); }
					(*ch) = '\0'; if((*line)=='='){ line++; }
					// LOGGER_LOG_DEBUG1("extracted directive '%s'",directive)

					// extract the value
					ch = value;
					while((*line)!='\0') { (*ch++) = (*line++); }
					(*ch) = '\0';
					// LOGGER_LOG_DEBUG1("extracted value '%s'",value)

					return;
				}
		}
	}
}

// ============================================================================
//
// MEMBER FUNCTION : ConfigurationFile::setCommentCharacters
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : set the characters which are recognised as starting a comment
//
// ARGUMENTS       : commentCharacters IN comment characters
//
// ============================================================================
void ConfigurationFile::setCommentCharacters
(
	char commentCharacters[]
)
{
	if(_commentCharacters != 0)
	{
		delete [] _commentCharacters;
	}
	_commentCharacters = new char[strlen(commentCharacters)+1];
	strcpy(_commentCharacters,commentCharacters);
}

// ============================================================================
//
// MEMBER FUNCTION : ConfigurationFile::setRequestedSection
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : set the name of the requested section: only directives in
//                   this section or in no section will be returned
//
// ARGUMENTS       : requestedSection IN requested section
//
// ============================================================================
void ConfigurationFile::setRequestedSection
(
	char requestedSection[]
)
{
	strcpy(_requestedSection,requestedSection);
	LOGGER_LOG_DEBUG1("setRequestedSection '%s'",_requestedSection)
}

// ============================================================================
//
// MEMBER FUNCTION : ConfigurationFile::ConfigurationFile
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : constructor
//
// ARGUMENTS       : n/a
//
// RETURNS         : n/a
//
// ============================================================================
ConfigurationFile::ConfigurationFile()
{
	_requestedSection[0] = '\0';
	configFile           = 0;
	_commentCharacters   = new char[strlen(CFGFILE_DEFAULT_COMMENT_CHARACTERS)+1];
	strcpy(_commentCharacters,CFGFILE_DEFAULT_COMMENT_CHARACTERS);
}

// ============================================================================
//
// MEMBER FUNCTION : ConfigurationFile::ConfigurationFile
//
// ACCESS SPECIFIER: public
//
// DESCRIPTION     : destructor
//
// ============================================================================
ConfigurationFile::~ConfigurationFile()
{
	if(_commentCharacters != 0)
	{
		delete [] _commentCharacters;
	}
	if(configFile != 0)
	{
		if(configFile->is_open()!=0)
		{
			configFile->close();
		}
		delete configFile;
	}
}

// ============================================================================
//
// MEMBER FUNCTION : ConfigurationFile::readNextRealLine
//
// ACCESS SPECIFIER: private
//
// DESCRIPTION     : read next line from input file which is not blank or a comment
//
// RETURNS         : pointer to line read
//
// ============================================================================
char *ConfigurationFile::readNextRealLine()
{

	while(!configFile->eof())
	{
		// remove any leading spaces or tabs
		configFile->getline(configLine,CFGFILE_MAX_LINE_LENGTH);

		// is it a blank line?
		if(configLine[0]=='\0')
		{
			// LOGGER_LOG_DEBUG("blank line")
			continue;
		}

		if (all_of(begin(configLine), end(configLine), [](char c) {return iswspace(c); }))
			continue;

		// is it a comment?
		if(strchr(_commentCharacters,configLine[0])!=0)
		{
			// LOGGER_LOG_DEBUG1("comment line '%s'",configLine)
			continue;
		}

		// non-blank line - return the line
		// LOGGER_LOG_DEBUG1("non-blank line '%s'",configLine)
		return configLine;

	}

	// end of file reached
	configLine[0] = '\0';
	// LOGGER_LOG_DEBUG("end of file reached")
	return configLine;

}
