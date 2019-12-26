// ============================================================================
//
// FILE        : exe.cpp
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : Windows NT program used to start another program as an NT service
//               or ordinary console command (formerly known as sybstart)
//
//               Refer to services.htm for description and instructions for use.
//               You can obtain this file on the Web at
//
//                         http://www.nick.rozanski.com/services.htm
//
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

// ============================================================================
//
// PROJECT HEADER FILES
//
// ============================================================================

// system headers
#include <stdlib.h>
#include <string>
#include <stdio.h>
#include <iostream>

// support headers
#include <logger.h>

// class headers
#include "ArgumentList.h"
#include "ConfigurationFile.h"
#include "Validation.h"
#include "../dll/CmdRunner.h"
#include "../dll/SrvStart.h"
#include "../dll/ServiceManager.h"
#include "../dll/StringSubstituter.h"

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
using namespace std;
// ============================================================================
//
// CONSTANT DEFINITIONS
//
// ============================================================================

const int	MAX_ARG_SIZE		= 5000;
const int	DIRECTIVE_SIZE		= 128;
const int	VALUE_SIZE			= 5000;

const char	*COMMAND_MODE_ARG		= "cmd";
const char	*SERVICE_MODE_ARG		= "svc";
const char	*ANY_MODE_ARG			= "any";
const char	*INSTALL_ARG			= "install";
const char	*INSTALL_DESKTOP_ARG	= "install_desktop";
const char	*REMOVE_ARG				= "remove";

const char	*LIBDIR_NAME	= "LIB";
const char	*PATH_NAME		= "PATH";
const char	*SYBASE_NAME	= "SYBASE";

// ============================================================================
//
// LOCAL FUNCTION PROTOTYPES
//
// ============================================================================

void getDefaultLibDir(char sybase[],char defaultLibDir[]);
void getDefaultPath(char sybase[],char defaultPath[]);
void installService(char *serviceName,bool desktopService,ArgumentList argList)
				throw(SrvStartException);
void parseArgv(CmdRunner *cmdRunner,ArgumentList argList)
				throw(SrvStartException);
void parseConfigurationFile(CmdRunner *cmdRunner,char configFile[]);
void parseSwitch(CmdRunner *cmdRunner,ArgumentList &argList,bool &libDirSet,bool &pathSet);
void printSyntaxAndExit(bool success);
void removeService(char *serviceName) throw(SrvStartException);
void exitProcess(bool success);

// ============================================================================
//
// FUNCTION        : main
//
// DESCRIPTION     : entry point
//
// ARGUMENTS       : argc, argv
//
// ============================================================================
void main(int argc, char* argv[])
{
	// default logger configuration
	LoggerConfigure(LOGGER_DEFAULT_LOGGER,0,const_cast<char*>(SrvStart::getApplication()),
					LOGGER_ANSI_STDOUT,0,0,0,0);

	// set the appropriate debug level for Debug or Release versions
#ifdef	LOGGER_DEBUG_ON
	LoggerSetDebugLevel(2);
#else
	LoggerSetDebugLevel(1);
#endif

	LOGGER_LOG_DEBUG("main")

	// ArgumentList variables
	ArgumentList                 argList(argc,argv);
	char                         arg[MAX_ARG_SIZE];
	char                         svc_name[sizeof(arg)];
	ArgumentList::ArgumentTypes  argType;
	CmdRunner::START_MODES       mode;

	// clear service name
	svc_name[0]='\0';

	// get first command-line argument (converted to lowercase)
	argList.peekNextArgument(argType,arg,ArgumentList::AL_TO_LOWER);
	LOGGER_LOG_DEBUG1("first argument type is %d",argType)
	switch(argType)
	{
		case ArgumentList::AL_SWITCH: case ArgumentList::AL_GNU_SWITCH:
			// -h, -?, --help etc (print syntax and exit)
			printSyntaxAndExit(true);
			break;

		case ArgumentList::AL_STRING:
			// keyword
			if(!strcmp(arg,COMMAND_MODE_ARG))
			{
				LOGGER_LOG_DEBUG("mode is 'command'")
				argList.popNextArgument(argType,arg,ArgumentList::AL_TO_LOWER);
				mode = CmdRunner::COMMAND_MODE ; // command mode
			}
			else if(!strcmp(arg,SERVICE_MODE_ARG))
			{
				LOGGER_LOG_DEBUG("mode is 'service'")
				argList.popNextArgument(argType,arg,ArgumentList::AL_TO_LOWER);
				mode = CmdRunner::SERVICE_MODE ; // service mode
				// since service mode, log to NT Event Log
				LoggerConfigure(LOGGER_DEFAULT_LOGGER,0,const_cast<char*>(SrvStart::getApplication()),
						LOGGER_WIN32_EVENTLOG,0,0,0,0);
			}
			else if(!strcmp(arg,ANY_MODE_ARG))
			{
				LOGGER_LOG_DEBUG("mode is 'any'")
				argList.popNextArgument(argType,arg,ArgumentList::AL_TO_LOWER);
				mode = CmdRunner::ANY_MODE ;		// any mode
			}
			else if(!strcmp(arg,INSTALL_ARG))
			{
				LOGGER_LOG_DEBUG("mode is 'install'")
				argList.popNextArgument(argType,arg,ArgumentList::AL_TO_LOWER);
				mode = CmdRunner::INSTALL_MODE ;	// install mode
				// since install mode, log to stdout
				LoggerConfigure(LOGGER_DEFAULT_LOGGER,0,const_cast<char*>(SrvStart::getApplication()),
						LOGGER_ANSI_STDOUT,0,0,0,0);
			}
			else if(!strcmp(arg,INSTALL_DESKTOP_ARG))
			{
				LOGGER_LOG_DEBUG("mode is 'install (desktop)'")
				argList.popNextArgument(argType,arg,ArgumentList::AL_TO_LOWER);
				mode = CmdRunner::INSTALL_DESKTOP_MODE ;	// install (desktop) mode
				// since install mode, log to stdout
				LoggerConfigure(LOGGER_DEFAULT_LOGGER,0,const_cast<char*>(SrvStart::getApplication()),
						LOGGER_ANSI_STDOUT,0,0,0,0);
			}
			else if(!strcmp(arg,REMOVE_ARG))
			{
				LOGGER_LOG_DEBUG("mode is 'remove '")
				argList.popNextArgument(argType,arg,ArgumentList::AL_TO_LOWER);
				mode = CmdRunner::REMOVE_MODE ;	// remove mode
				// since remove mode, log to stdout
				LoggerConfigure(LOGGER_DEFAULT_LOGGER,0,const_cast<char*>(SrvStart::getApplication()),
						LOGGER_ANSI_STDOUT,0,0,0,0);
			}
			else
			{
				// invalid mode - assume this argument is the service name
				argList.popNextArgument(argType,arg);
				strcpy(svc_name,arg);
				LOGGER_LOG_DEBUG1("unrecognised mode '%s' supplied: assuming service name",svc_name)
				mode = CmdRunner::SERVICE_MODE ;
			}
			break;

		default:
			// empty command line
			LOGGER_LOG_ERROR("Invalid syntax")
			printSyntaxAndExit(false);
	}

	if(svc_name[0]=='\0')
	{
		// get the window / service name
		argList.popNextArgument(argType,arg);
		if(argType!=ArgumentList::AL_STRING)
		{
			LOGGER_LOG_ERROR1("Expecting window / service name, found '%s'",arg)
			printSyntaxAndExit(false);
		}
		strcpy(svc_name,arg);
		LOGGER_LOG_DEBUG1("window / service name is '%s'",svc_name)
	}

	// if install mode, install service and exit
	if((mode==CmdRunner::INSTALL_MODE)||(mode==CmdRunner::INSTALL_DESKTOP_MODE))
	{
		try
		{
			// try and install service
			installService(svc_name,(mode==CmdRunner::INSTALL_DESKTOP_MODE),argList);
		}
		catch(SrvStartException e)
		{
			// an exception has been trapped - log it
			LOGGER_LOG_ERROR3("Exception %d trapped in source file '%s' line %d",
		 						e.exceptionId,e.sourceFile,e.lineNumber)
			LOGGER_LOG_ERROR2("Class '%s' method '%s'",e.className,e.methodName)
			LOGGER_LOG_ERROR1("%s",e.errorMessage)

			// write it to stdout too
			cout << "ERROR: Exception " << e.exceptionId <<
					" trapped in source file '" << e.sourceFile <<
					"' line " << e.lineNumber << "\n";
			cout << "ERROR: Class '" << e.className << "' method '" << e.methodName << "'\n";
			cout << e.errorMessage << "\n";

			exitProcess(false);
		}

		// install succeeded
		exitProcess(true);
	}

	// if remove mode, remove service and exit
	if(mode==CmdRunner::REMOVE_MODE)
	{
		try
		{
			// try and remove service
			removeService(svc_name);
		}
		catch(SrvStartException e)
		{
			// an exception has been trapped - log it
			LOGGER_LOG_ERROR3("Exception %d trapped in source file '%s' line %d",
		 						e.exceptionId,e.sourceFile,e.lineNumber)
			LOGGER_LOG_ERROR2("Class '%s' method '%s'",e.className,e.methodName)
			LOGGER_LOG_ERROR1("%s",e.errorMessage)

			// write it to stdout too
			cout << "ERROR: Exception " << e.exceptionId <<
					" trapped in source file '" << e.sourceFile <<
					"' line " << e.lineNumber << "\n";
			cout << "ERROR: Class '" << e.className << "' method '" << e.methodName << "'\n";
			cout << e.errorMessage << "\n";

			exitProcess(false);
		}

		// install succeeded
		exitProcess(true);
	}

	// otherwise - run command or service
	try
	{
		// create CmdRunner object
		LOGGER_LOG_DEBUG("about to create CmdRunner object")
		CmdRunner cmdRunner(mode,svc_name);
		LOGGER_LOG_DEBUG("CmdRunner object created")

		// parse remaining command line arguments (includes reading configuration file)
		parseArgv(&cmdRunner,argList);

		// log startup information
		LOGGER_LOG_INFO4("%s version %s %s (%s)",
			const_cast<char*>(getApplication()),
			const_cast<char*>(getVersion()),
			const_cast<char*>(getCopyright()),
			const_cast<char*>(getDistribution()))
		LOGGER_LOG_INFO1("To hide the startup message run %s with switch -d 0",
			const_cast<char*>(getApplication()))

		// start cmdRunner
		cmdRunner.start();

	}
	catch(SrvStartException e)
	{
		// an exception has been trapped - log it
		LOGGER_LOG_ERROR3("Exception %d trapped in source file '%s' line %d",
		 					e.exceptionId,e.sourceFile,e.lineNumber)
		LOGGER_LOG_ERROR2("Class '%s' method '%s'",e.className,e.methodName)
		LOGGER_LOG_ERROR1("%s",e.errorMessage)

		// write it to stdout too
		cout << "ERROR: Exception " << e.exceptionId <<
				" trapped in source file '" << e.sourceFile <<
				"' line " << e.lineNumber << "\n";
		cout << "ERROR: Class '" << e.className << "' method '" << e.methodName << "'\n";
		cout << e.errorMessage << "\n";

		exitProcess(false);
	}

	// start() returns when the program has stopped
	exitProcess(true);

}

// ============================================================================
//
// FUNCTION        : printSyntaxAndExit
//
// DESCRIPTION     : print command-line syntax and exit
//
// ARGUMENTS       : success IN whether to exit with a success status
//
// ============================================================================
void printSyntaxAndExit
(
	bool success
)
{
	// switch off any debugging / info messages
	LoggerSetDebugLevel(-1);

	// print syntax
	cout << "\n\n" << APPLICATION << " version " << VERSION << "\n"
	     << COPYRIGHT << "\n\n";

	cout << "\
Start a program with a given environment, and optionally as a NT service\n\
 or install / remove an NT service which uses " << APPLICATION << "\n\
\n\
Syntax for command mode:\n\
 srvstart cmd window_title [options] command [program_parameters...]\n\
\n\
Syntax for service mode:\n\
 srvstart [ svc ] service_name [options] command [program_parameters...]\n\
\n\
Syntax for any mode (try service, then command):\n\
 srvstart any service_name [options] command [program_parameters...]\n\
\n\
Syntax for install mode:\n\
 srvstart install|install_desktop service_name -c controlfile\n\
\n\
Syntax for remove mode:\n\
 srvstart remove service_name\n\
\n\
service_name is short (internal) name of NT service\n\
\n\
options:\n\
\n\
  -e var=value assign value to environment variable var\n\
  -p path      assign value of %PATH%\n\
  -l libdir    assign value of %LIB%\n\
  -s sybase    assign value of %SYBASE% variable\n\
  -q sybase    assign default %PATH% based on this value of %SYBASE%\n\
\n\
  -x priority  run at given execution priority (normal/high/real/idle)\n\
  -w           start in new window (command mode only)\n\
  -m           start in minimised new window (command mode only)\n\
  -y sec       delay sec seconds before reporting 'started' (service mode only)\n\
  -t sec       program status check interval (service mode only)\n\
\n\
  -d level     debug level (0/1/2, 2 highest)\n\
  -o target    send debug output to target (filename or - for stdout or LOG for NT Event Log)\n\
  -c ctrlfile  get srvstart options from file ctrlfile\n\
  -h           display this help message\n\
\n\
Substitute parameters in command using {prompt} or {prompt:default}\n\
Substitute environment vars in command using %variable%\n\
Refer to exe.cpp for details.\n\n";

	cout << DISTRIBUTION << "\n";

	// exit
	exitProcess(success);
}

// ============================================================================
//
// FUNCTION        : exitProcess
//
// DESCRIPTION     : exit this process
//
// ARGUMENTS       : success IN whether to exit with a success status
//
// ============================================================================
void exitProcess
(
	bool success
)
{
	if(success)
	{
		// exit with a success status
#ifdef	LOGGER_DEBUG_ON
		LOGGER_LOG_INFO("SRVSTART is terminating with a SUCCESS status")
#endif
		exit(EXIT_SUCCESS);
	}
	else
	{
		// exit with a failure status
		LOGGER_LOG_ERROR("SRVSTART is terminating with a FAILURE status")
		exit(EXIT_FAILURE);
	}
}

// ============================================================================
//
// FUNCTION        : parseArgv
//
// DESCRIPTION     : parse command-line arguments
//
// ARGUMENTS       : cmdRunner IN CmdRunner object to apply arguments to
//                   argList   IN argument list
//
// THROWS          : SrvStartException
//
// ============================================================================
void parseArgv
(
	CmdRunner    *cmdRunner,
	ArgumentList  argList
) throw(SrvStartException)
{
	LOGGER_LOG_DEBUG("parseArgv()")

	static char                 arg[MAX_ARG_SIZE];
	ArgumentList::ArgumentTypes argType;
	bool                        libDirSet = false, pathSet = false;

	// parse switches on rest of command line
	while(true)
	{
		argList.peekNextArgument(argType);
		switch(argType)
		{
			case ArgumentList::AL_EMPTY:
				// we have reached the end of the argument list
				LOGGER_LOG_DEBUG("parseArgv(): no more arguments")
				goto NO_MORE_ARGS;

			case ArgumentList::AL_SWITCH:
				// a switch
				LOGGER_LOG_DEBUG("parseArgv(): switch")
				parseSwitch(cmdRunner,argList,libDirSet,pathSet);
				break;

			default:
				// program to run
				LOGGER_LOG_DEBUG("parseArgv(): program to run")
				argList.popNextArgument(argType,arg);
				LOGGER_LOG_DEBUG1("parseArgv(): program is '%s'",arg)
				cmdRunner->setStartupCommand(arg);
				// get program arguments (if any)
				while(true)
				{
					// get next argument
					argList.popNextArgument(argType,arg);
					if(argType!=ArgumentList::AL_EMPTY)
					{
						// add this argument to command line
						LOGGER_LOG_DEBUG1("parseArgv(): adding '%s' to command",arg)
						cmdRunner->addStartupCommandArgument(arg);
					}
					else
					{
						// end of arguments
						goto NO_MORE_ARGS;
					}
				}
				break;
		}
	}

NO_MORE_ARGS:
	// completed argument parsing
	LOGGER_LOG_DEBUG("parseArgv(): completed argument parsing")
	return ;

}

// ============================================================================
//
// FUNCTION        : parseSwitch
//
// DESCRIPTION     : parse a command-line switch
//
// ARGUMENTS       : cmdRunner IN CmdRunner object to apply arguments to
//                   argList   IN argument list
//                   libDirSet IN true if %LIB% has been set already
//                   pathSet   IN true if %PATH% has been set already
//
// THROWS          : SrvStartException
//
// ============================================================================
void parseSwitch
(
	CmdRunner   *cmdRunner,
	ArgumentList &argList,
	bool         &libDirSet,
	bool         &pathSet
) throw(SrvStartException)
{
	LOGGER_LOG_DEBUG("parseSwitch()")

	static char                 arg[MAX_ARG_SIZE];
	ArgumentList::ArgumentTypes argType;
	bool                        isValid;

	argList.popNextArgument(argType,arg);
	switch(arg[0])
	{
		case 'd':	LOGGER_LOG_DEBUG("switch -d")
			// debug level
			argList.popNextArgument(argType,ArgumentList::AL_IS_INTEGER,isValid,arg);
			if(isValid)
			{
				LoggerSetDebugLevel(atoi(arg)-1);
			}
			else
			{
				// bad debug level supplied
				LoggerSetDebugLevel(2);
				LOGGER_LOG_ERROR1("Invalid debug level %s",arg)
				THROW_SRVSTART_EXCEPTION
					(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseSwitch")
			}
			break;

		case 'e':	LOGGER_LOG_DEBUG("switch -e")
			// environment variable
			{
				char  envName[MAX_ARG_SIZE];
				char  envValue[MAX_ARG_SIZE];
				int   equalsPosition;
				argList.popNextArgument(argType,arg);
				equalsPosition=(strchr(arg,'=')-arg);
				if(equalsPosition==0)
				{
					LOGGER_LOG_ERROR1("missing = in -e switch '%s'",arg)
					printSyntaxAndExit(false);
				}
				// get environment variable name
				strncpy(envName,arg,equalsPosition);
				envName[equalsPosition]='\0';
				// get environment variable value
				strcpy(envValue,arg+equalsPosition+1);
				// add environment variable to list
				LOGGER_LOG_DEBUG2("-e '%s' = '%s'",envName,envValue)
				cmdRunner->addEnv(envName,envValue);
			}
			break;

		case 'h':
		case '?':	LOGGER_LOG_DEBUG("switch -h/?")
			// help
			printSyntaxAndExit(true);
			break;
		
		case 'l':	LOGGER_LOG_DEBUG("switch -l")
			// value of %LIB%
			argList.popNextArgument(argType,arg);
			LOGGER_LOG_DEBUG2("-l '%s' = '%s'",LIBDIR_NAME,arg)
			cmdRunner->addEnv(LIBDIR_NAME,arg);
			libDirSet = true;
			break;

		case 'm':	LOGGER_LOG_DEBUG("switch -m")
			// start minimised
			cmdRunner->setStartMinimised(true);
			break;

		case 'o':	LOGGER_LOG_DEBUG("switch -o")
			// debug output
			argList.popNextArgument(argType,arg);
			if(argType == ArgumentList::AL_STDIN)
			{
				// log to stdout
				LOGGER_LOG_DEBUG("log to stdout")
				int loggerError;
				if(LoggerConfigure(LOGGER_DEFAULT_LOGGER,"",const_cast<char*>(APPLICATION),
										LOGGER_ANSI_STDOUT,0,0,&loggerError,0)==0)
				{
					LOGGER_LOG_ERROR1("Logger initialisation failed, error = %d",loggerError)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_GENERAL_ERROR,"","parseSwitch")
				}
			}
			else
			if(!strcmp(arg,"LOG"))
			{
				// log to event log
				LOGGER_LOG_DEBUG("log to Event Log")
				int loggerError;
				if(LoggerConfigure(LOGGER_DEFAULT_LOGGER,"",const_cast<char*>(APPLICATION),
										LOGGER_WIN32_EVENTLOG,"",0,&loggerError,0)==0)
				{
					LOGGER_LOG_ERROR1("Logger initialisation failed, error = %d",loggerError)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_GENERAL_ERROR,"","parseSwitch")
				}
			}
			else
			{
				// log to file
				LOGGER_LOG_DEBUG1("log to file '%s'",arg)
				int loggerError;
				if(LoggerConfigure(LOGGER_DEFAULT_LOGGER,"",const_cast<char*>(APPLICATION),
										LOGGER_ANSI_FILENAME,arg,0,&loggerError,0)==0)
				{
					LOGGER_LOG_ERROR1("Logger initialisation failed, error = %d",loggerError)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_GENERAL_ERROR,"","parseSwitch")
				}
			}

			break;

		case 'p':	LOGGER_LOG_DEBUG("switch -p")
			// value of %PATH%
			argList.popNextArgument(argType,arg);
			LOGGER_LOG_DEBUG2("-p '%s' = '%s'",PATH_NAME,arg)
			cmdRunner->addEnv(PATH_NAME,arg);
			pathSet = true;
			break;

		case 'q':	LOGGER_LOG_DEBUG("switch -q")
			// value of %PATH% based on %SYBASE%
			{
				char tmp[MAX_ARG_SIZE];
				argList.popNextArgument(argType,arg);
				getDefaultPath(arg,tmp);
				cmdRunner->addEnv(PATH_NAME,tmp);
				pathSet = true;
			}
			break;

		case 's':	LOGGER_LOG_DEBUG("switch -s")
			// value of %SYBASE%
			{
				char tmp[MAX_ARG_SIZE];
				argList.popNextArgument(argType,arg);
				cmdRunner->addEnv(SYBASE_NAME,arg);
				if(!libDirSet)
				{
					getDefaultLibDir(arg,tmp);
					LOGGER_LOG_DEBUG1("lib dir not set, using default '%s'",tmp)
					cmdRunner->addEnv(LIBDIR_NAME,tmp);
					libDirSet = true;
				}
				if(!pathSet)
				{
					getDefaultPath(arg,tmp);
					LOGGER_LOG_DEBUG1("path not set, using default '%s'",tmp)
					cmdRunner->addEnv(PATH_NAME,tmp);
					pathSet = true;
				}
			}
			break;

		case 't':	LOGGER_LOG_DEBUG("switch -t")
			// wait interval
			argList.popNextArgument(argType,ArgumentList::AL_IS_INTEGER,isValid,arg);
			if(isValid)
			{
				cmdRunner->setWaitInterval(atoi(arg));
			}
			else
			{
				LOGGER_LOG_ERROR1("Invalid wait interval (-t) %s",arg)
				printSyntaxAndExit(false);
			}
			break;

		case 'w':	LOGGER_LOG_DEBUG("switch -w")
			// start in new window
			cmdRunner->setStartInNewWindow(true);
			break;

		case 'x':	LOGGER_LOG_DEBUG("switch -x")
			// execution priority
			argList.popNextArgument(argType,ArgumentList::AL_ANY,isValid,arg,ArgumentList::AL_TO_LOWER);
			if(!strcmp(arg,"high")) { cmdRunner->setExecutionPriority(CmdRunner::HIGH_PRIORITY); }
			else if(!strcmp(arg,"idle")) { cmdRunner->setExecutionPriority(CmdRunner::IDLE_PRIORITY); }
			else if(!strcmp(arg,"normal")) { cmdRunner->setExecutionPriority(CmdRunner::NORMAL_PRIORITY); }
			else if(!strcmp(arg,"real")) { cmdRunner->setExecutionPriority(CmdRunner::REAL_PRIORITY); }
			else
			{
				LOGGER_LOG_ERROR1("Invalid execution priority (-x) %s",arg)
				printSyntaxAndExit(false);
			}
			break;

		case 'y':	LOGGER_LOG_DEBUG("switch -y")
			// startup delay
			argList.popNextArgument(argType,ArgumentList::AL_IS_INTEGER,isValid,arg);
			if(isValid)
			{
				cmdRunner->setStartupDelay(atoi(arg));
			}
			else
			{
				LOGGER_LOG_ERROR1("Invalid startup delay (-y) %s",arg)
				printSyntaxAndExit(false);
			}
			break;

		case 'c':	LOGGER_LOG_DEBUG("switch -c")
			// configuration file
			argList.popNextArgument(argType,ArgumentList::AL_IS_FILE,isValid,arg);
			if(isValid)
			{
				parseConfigurationFile(cmdRunner,arg);
			}
			else
			{
				LOGGER_LOG_ERROR1("Configuration file '%s' not found",arg)
				THROW_SRVSTART_EXCEPTION
					(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseSwitch")
			}
			break;

		default:
			LOGGER_LOG_ERROR1("Invalid switch %s",arg)
			printSyntaxAndExit(false);
			break;
	}
}

// ============================================================================
//
// FUNCTION        : parseConfigurationFile
//
// DESCRIPTION     : parse a configuration file
//
// ARGUMENTS       : cmdRunner  IN CmdRunner object to apply arguments to
//                   configFile IN name of configuration file
//
// THROWS          : SrvStartException
//
// ============================================================================
void parseConfigurationFile
(
	CmdRunner *cmdRunner,
	char       configFile[]
) throw(SrvStartException)
{
	ConfigurationFile cf;
	static char       directive[DIRECTIVE_SIZE];
	static char       value[VALUE_SIZE];
	bool              libDirSet=false,pathSet=false;

	// control file directive identifiers
#define	W_EMPTY		-2
#define	W_INVALID	-1
	typedef enum 
	{
		W_AUTO_RESTART = 0,
		W_DEBUG,
		W_DEBUG_OUT,
		W_ENV,
		W_LIB,
		W_LOCAL_DRIVE,
		W_MINIMISED,
		W_NET_DRIVE,
		W_NEW_WINDOW,
		W_PATH,
		W_PRIORITY,
		W_RESTART_INTERVAL,
		W_SYBASE,
		W_SYBPATH,
		W_SHUTDOWN,
		W_SHUTDOWN_METHOD,
		W_STARTUP,
		W_STARTUP_DELAY,
		W_STARTUP_DIR,
		W_WAIT,
		W_WAIT_TIME
	} directive_ids;

	//
	// control file directives
	// !! THESE MUST ALWAYS BE IN ALPHABETICAL ORDER !!
	//

	typedef struct
	{
		char          directive_name[DIRECTIVE_SIZE];
		directive_ids directive_id;
	} directive_array;

	directive_array directives [] =
	{
		"auto_restart",		W_AUTO_RESTART,
		"debug",			W_DEBUG,
		"debug_out",		W_DEBUG_OUT,
		"env",				W_ENV,
		"lib",				W_LIB,
		"local_drive",		W_LOCAL_DRIVE,
		"minimised",		W_MINIMISED,
		"network_drive",	W_NET_DRIVE,
		"new_window",		W_NEW_WINDOW,
		"path",				W_PATH,
		"priority",			W_PRIORITY,
		"restart_interval",	W_RESTART_INTERVAL,
		"shutdown",			W_SHUTDOWN,
		"shutdown_method",	W_SHUTDOWN_METHOD,
		"startup",			W_STARTUP,
		"startup_delay",	W_STARTUP_DELAY,
		"startup_dir",		W_STARTUP_DIR,
		"sybase",			W_SYBASE,
		"sybpath",			W_SYBPATH,
		"wait",				W_WAIT,
		"wait_time",		W_WAIT_TIME
	};

	directive_array *directive_id;
	int this_directive_id;

	// open the configuration file
	LOGGER_LOG_DEBUG1("about to open configuration file '%s'",configFile)
	if(!cf.openConfigurationFile(configFile))
	{
		LOGGER_LOG_ERROR1("failed to open configuration file '%s'",configFile)
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
	}

	// set the name of the section we are interested in
	cf.setRequestedSection(cmdRunner->getSrvName());
	LOGGER_LOG_DEBUG1("requested section is '%s'",cmdRunner->getSrvName())

	// read the configuration file directives
	while(true)
	{
		// get the next directive
		cf.getNextConfigurationDirective(directive,value);

		// have we reached the end of file?
		if(directive[0]=='\0')
		{
			LOGGER_LOG_DEBUG("end of directive file reached")
			break;
		}
		LOGGER_LOG_DEBUG2("next directive '%s' = '%s'",directive,value)

		// look up directive in directive list
		LOGGER_LOG_DEBUG1("about to call bsearch for directive '%s'",directive)
		directive_id = (directive_array*)bsearch((void*)directive,(void*)directives,
						(int)(sizeof(directives)/sizeof(directive_array)),
						sizeof(directive_array),
						(int(__cdecl *)(const void *,const void *))strcmp);

		if (directive_id != 0)
		{
			// we have found the word - convert the pointer to an integer
			this_directive_id = directive_id->directive_id;
			LOGGER_LOG_DEBUG1("directive id is %d",this_directive_id)
		}
		else
		{
			LOGGER_LOG_DEBUG("did not find directive")
			this_directive_id = W_INVALID;
		}

		// take the appropriate action for this directive
		class Validation v;
		switch(this_directive_id)
		{
			case W_AUTO_RESTART:
				// auto restart?
				cmdRunner->setAutoRestart(v.isLikeYes(value));
				break;

			case W_DEBUG:
				if(v.isInteger(value))
				{
					LoggerSetDebugLevel(atoi(value)-1);
				}
				else
				{
					// bad debug level supplied
					LoggerSetDebugLevel(2);
					LOGGER_LOG_ERROR1("Invalid debug level directive %s",value)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
				}
				break;

			case W_DEBUG_OUT:
				if(!strcmp(value,"-"))
				{
					// log to stdout
					int loggerError;
					if(LoggerConfigure(LOGGER_DEFAULT_LOGGER,"",const_cast<char*>(APPLICATION),
											LOGGER_ANSI_STDOUT,0,0,&loggerError,0)==0)
					{
						LOGGER_LOG_ERROR1("Logger initialisation failed, error = %d",loggerError)
						THROW_SRVSTART_EXCEPTION
							(SRVSTART_EXCEPTION_GENERAL_ERROR,"","parseConfigurationFile")
					}
				}
				else
				if(!strcmp(value,"LOG"))
				{
					// log to event log
					int loggerError;
					if(LoggerConfigure(LOGGER_DEFAULT_LOGGER,"",const_cast<char*>(APPLICATION),
											LOGGER_WIN32_EVENTLOG,"",0,&loggerError,0)==0)
					{
						LOGGER_LOG_ERROR1("Logger initialisation failed, error = %d",loggerError)
						THROW_SRVSTART_EXCEPTION
							(SRVSTART_EXCEPTION_GENERAL_ERROR,"","parseConfigurationFile")
					}
				}
				else
				{
					// log to file

					// get filename and substitute environment variables
					StringSubstituter stringSubstituter;
					char *logFile;
					stringSubstituter.stringInit(logFile);

					// if the first character of the log file is '>', then truncate the log file first
					int truncateFile;
					// truncate the log file?
					if(value[0]=='>')
					{
						// strip off the leading '>' and truncate the file
						stringSubstituter.stringCopy(logFile,value+1);
						truncateFile=1;
					}
					else
					{
						// don't truncate the file
						stringSubstituter.stringCopy(logFile,value);
						truncateFile=0;
					}

					// substitute any environment variables in the log file
					stringSubstituter.stringSubstitute(logFile);

					// configure the logger
					int loggerError;
					if(LoggerConfigure(LOGGER_DEFAULT_LOGGER,"",const_cast<char*>(APPLICATION),
											LOGGER_ANSI_FILENAME,logFile,(void*)&truncateFile,
											&loggerError,0)==0)
					{
						LOGGER_LOG_ERROR1("Logger initialisation failed, error = %d",loggerError)
						stringSubstituter.stringDelete(logFile);
						THROW_SRVSTART_EXCEPTION
							(SRVSTART_EXCEPTION_GENERAL_ERROR,"","parseConfigurationFile")
					}
					stringSubstituter.stringDelete(logFile);
				}
				break;

			case W_ENV:
				// environment variable
				{
					char  envName[MAX_ARG_SIZE];
					char  envValue[MAX_ARG_SIZE];
					int   equalsPosition;
					equalsPosition=(strchr(value,'=')-value);
					if(equalsPosition==0)
					{
						LOGGER_LOG_ERROR1("missing = in environment directive '%s'",value)
						THROW_SRVSTART_EXCEPTION
							(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
					}
					// get environment variable name
					strncpy(envName,value,equalsPosition);
					envName[equalsPosition]='\0';
					// get environment variable value
					strcpy(envValue,value+equalsPosition+1);
					// add environment variable to list
					LOGGER_LOG_DEBUG2("environment '%s' = '%s'",envName,envValue)
					cmdRunner->addEnv(envName,envValue);
				}
				break;

			case W_LOCAL_DRIVE:
				// map local drive (ie SUBST)
				{
					// find position of =
					int equalsPosition=(strchr(value,'=')-value);
					if(equalsPosition==0)
					{
						LOGGER_LOG_ERROR1("missing = in environment directive '%s'",value)
						THROW_SRVSTART_EXCEPTION
							(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
					}

					// get drive letter
					char driveLetter;
					int i = 0;
					while(value[i]==' ') { i++;}
					driveLetter = value[i];

					// get drive path and substitute environment variables
					char drivePath[MAX_PATH];
					strcpy(drivePath,value+equalsPosition+1);

					// map local drive
					LOGGER_LOG_DEBUG2("local drive %c = '%s'",driveLetter,drivePath)
					cmdRunner->mapLocalDrive(driveLetter,drivePath);
				}
				break;

			case W_LIB:
				// value of %LIB%
				LOGGER_LOG_DEBUG2("'%s' = '%s'",LIBDIR_NAME,value)
				cmdRunner->addEnv(LIBDIR_NAME,value);
				libDirSet = true;
				break;

			case W_MINIMISED:
				// start minimised?
				cmdRunner->setStartMinimised(v.isLikeYes(value));
				break;

			case W_NET_DRIVE:
				// map network drive (ie NET USE)
				{
					// find position of =
					int equalsPosition=(strchr(value,'=')-value);
					if(equalsPosition==0)
					{
						LOGGER_LOG_ERROR1("missing = in environment directive '%s'",value)
						THROW_SRVSTART_EXCEPTION
							(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
					}

					// get drive letter
					char driveLetter;
					int i = 0;
					while(value[i]==' ') { i++;}
					driveLetter = value[i];

					// get network path and substitute environment variables
					char networkPath[MAX_PATH];
					strcpy(networkPath,value+equalsPosition+1);

					// map network drive
					LOGGER_LOG_DEBUG2("network drive '%s' = '%s'",driveLetter,networkPath)
					cmdRunner->mapNetworkDrive(driveLetter,networkPath);
				}
				break;

			case W_NEW_WINDOW:
				// start in new window
				cmdRunner->setStartInNewWindow(v.isLikeYes(value));
				break;

			case W_PATH:
				// value of %PATH%
				LOGGER_LOG_DEBUG2("'%s' = '%s'",PATH_NAME,value)
				cmdRunner->addEnv(PATH_NAME,value);
				pathSet = true;
				break;

			case W_PRIORITY:
				if(!strcmp(value,"high")) { cmdRunner->setExecutionPriority(CmdRunner::HIGH_PRIORITY); }
				else if(!strcmp(value,"idle")) { cmdRunner->setExecutionPriority(CmdRunner::IDLE_PRIORITY); }
				else if(!strcmp(value,"normal")) { cmdRunner->setExecutionPriority(CmdRunner::NORMAL_PRIORITY); }
				else if(!strcmp(value,"real")) { cmdRunner->setExecutionPriority(CmdRunner::REAL_PRIORITY); }
				else
				{
					LOGGER_LOG_ERROR1("Invalid execution priority %s",value)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
				}
				break;

			case W_RESTART_INTERVAL:
				// restart interval
				if(v.isInteger(value))
				{
					cmdRunner->setAutoRestartInterval(atoi(value));
				}
				else
				{
					LOGGER_LOG_ERROR1("Invalid restart interval %s",value)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
				}
				break;

			case W_SHUTDOWN:
				// shutdown command
				cmdRunner->setShutdownCommand(value);
				cmdRunner->setShutdownMethod(CmdRunner::SHUTDOWN_BY_COMMAND);
				break;

			case W_SHUTDOWN_METHOD:
				// shutdown command
				if(!strcmp(value,"kill")) { cmdRunner->setShutdownMethod(CmdRunner::SHUTDOWN_BY_KILL); }
				else if(!strcmp(value,"command")) { cmdRunner->setShutdownMethod(CmdRunner::SHUTDOWN_BY_COMMAND); }
				else if(!strcmp(value,"winmessage")) { cmdRunner->setShutdownMethod(CmdRunner::SHUTDOWN_BY_WINMESSAGE); }
				else
				{
					LOGGER_LOG_ERROR1("Invalid shutdown method %s",value)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
				}
				break;

			case W_STARTUP:
				// startup command
				cmdRunner->setStartupCommand(value);
				break;

			case W_STARTUP_DELAY:
				// startup delay
				if(v.isInteger(value))
				{
					cmdRunner->setStartupDelay(atoi(value));
				}
				else
				{
					LOGGER_LOG_ERROR1("Invalid startup delay %s",value)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
				}
				break;

			case W_STARTUP_DIR:
				// startup directory
				cmdRunner->setStartupDirectory(value);
				break;

			case W_SYBASE:
				// value of %SYBASE%
				{
					char tmp[MAX_ARG_SIZE];
					cmdRunner->addEnv(SYBASE_NAME,value);
					if(!libDirSet)
					{
						getDefaultLibDir(value,tmp);
						LOGGER_LOG_DEBUG1("lib dir not set, using default '%s'",tmp)
						cmdRunner->addEnv(LIBDIR_NAME,tmp);
						libDirSet = true;
					}
					if(!pathSet)
					{
						getDefaultPath(value,tmp);
						LOGGER_LOG_DEBUG1("path not set, using default '%s'",tmp)
						cmdRunner->addEnv(PATH_NAME,tmp);
						pathSet = true;
					}
				}
				break;

			case W_SYBPATH:
				// value of %PATH% based on %SYBASE%
				{
					char tmp[MAX_ARG_SIZE];
					getDefaultPath(value,tmp);
					cmdRunner->addEnv(PATH_NAME,tmp);
					pathSet = true;
				}
				break;

			case W_WAIT:
				// wait command
				cmdRunner->setWaitCommand(value);
				break;

			case W_WAIT_TIME:
				// wait interval
				if(v.isInteger(value))
				{
					cmdRunner->setWaitInterval(atoi(value));
				}
				else
				{
					LOGGER_LOG_ERROR1("Invalid wait interval %s",value)
					THROW_SRVSTART_EXCEPTION
						(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
				}
				break;

			default:
				LOGGER_LOG_ERROR2("Invalid directive '%s' = '%s'",directive,value)
				THROW_SRVSTART_EXCEPTION
					(SRVSTART_EXCEPTION_INVALID_PARAMETER,"","parseConfigurationFile")
				break;

		}
	}

}

// ============================================================================
//
// FUNCTION        : getDefaultLibDir
//
// DESCRIPTION     : return default LIB directory based on given value of %SYBASE%
//
// ARGUMENTS       : sybase        IN  value of %SYBASE%
//                   defaultLibDir OUT default LIB directory 
//
// ============================================================================
void getDefaultLibDir
(
	char sybase[],
	char defaultLibDir[]
)
{
	sprintf(defaultLibDir,"%s\\lib",sybase);
	LOGGER_LOG_DEBUG1("default LIB is '%s'",defaultLibDir)
}

// ============================================================================
//
// FUNCTION        : getDefaultPath
//
// DESCRIPTION     : return default PATH based on given value of %SYBASE%
//
// ARGUMENTS       : sybase      IN  value of %SYBASE%
//                   defaultPath OUT default PATH
//
// ============================================================================
void getDefaultPath
(
	char sybase[],
	char defaultPath[]
)
{
	char *systemRoot;

	// get the location of the Windows NT directory
	systemRoot = getenv("SYSTEMROOT");
	LOGGER_LOG_DEBUG1("systemRoot is '%s'",systemRoot)

	// build up the default path
	sprintf(defaultPath,"%s\\install;%s\\bin;%s\\dll;%s;%s\\system32",
				sybase,sybase,sybase,systemRoot,systemRoot);
	LOGGER_LOG_DEBUG1("default PATH is '%s'",defaultPath)
}

// ============================================================================
//
// FUNCTION        : installService
//
// DESCRIPTION     : install a srvstart.exe-based service with given parameters
//
// ARGUMENTS       : serviceName    IN  service name
//                   desktopService IN can this service interact with the desktop?
//                   argList        IN argument list
//
// ============================================================================
void installService
(
	char         *serviceName,
	bool          desktopService,
	ArgumentList  argList
) throw(SrvStartException)
{
	LOGGER_LOG_DEBUG1("installService(%s)",serviceName)

	// get path name of this executable
	char *thisExe = new char[_MAX_PATH];
	if(!argList.getExeFullPath(thisExe))
	{
		// failed to get full path
		LOGGER_LOG_ERROR("Failed to get full path of SrvStart executable - you must run this from a command prompt")
		THROW_SRVSTART_EXCEPTION
			(SRVSTART_EXCEPTION_GENERAL_ERROR,"","installService")
	}

	// ServiceManager object
	ServiceManager serviceManager(serviceName);

	// assign binary path name
	serviceManager.setBinaryPath(thisExe);

	// get name of control file
	ArgumentList::ArgumentTypes argType;
	char                        arg[MAX_ARG_SIZE];
	argList.popNextArgument(argType,arg,ArgumentList::AL_TO_LOWER);
	if(argType == ArgumentList::AL_SWITCH)
	{
		// a switch: is it -c?
		if(arg[0]=='c')
		{
			// next argument must be the path name of the control file
			bool isValid;
			argList.popNextArgument(argType,ArgumentList::AL_IS_FILE,isValid,arg);
			if(isValid)
			{
				// add the parameter
				LOGGER_LOG_DEBUG1("control file '%s' exists",arg)
				serviceManager.addBinaryPathParameter("svc");
				serviceManager.addBinaryPathParameter(serviceName);
				serviceManager.addBinaryPathParameter("-c");
				serviceManager.addBinaryPathParameter(arg);
			}
			else
			{
				LOGGER_LOG_ERROR1("Configuration file '%s' not found",arg)
				exitProcess(false);
			}
		}
		else
		{
			// switch is not -c
			LOGGER_LOG_ERROR1("invalid switch for install mode (-%s)",arg)
			printSyntaxAndExit(false);
		}
	}
	else
	{
		// argument is not a switch
		LOGGER_LOG_ERROR1("invalid syntax for install mode (%s)",arg)
		printSyntaxAndExit(false);
	}

	// set desktop interaction
	serviceManager.setDesktopService(desktopService);

	// install service (will throw exception if does not work)
	serviceManager.install();

	return;
}

// ============================================================================
//
// FUNCTION        : removeService
//
// DESCRIPTION     : remove a (srvstart.exe-based) service with given name
//
// ARGUMENTS       : serviceName IN service name
//
// ============================================================================
void removeService
(
	char *serviceName
) throw(SrvStartException)
{
	LOGGER_LOG_DEBUG1("removeService(%s)",serviceName)

	// ServiceManager object
	ServiceManager serviceManager(serviceName);

	// remove service (will throw exception if does not work)
	serviceManager.remove();

	return;
}
