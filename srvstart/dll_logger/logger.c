// ============================================================================
//
// FILE        : logger.c
//
// AUTHOR      : Nick Rozanski
//
// DESCRIPTION : General-purpose function to log a text message to one or more
//               destinations (stdout, file or NT Event Log).
//
//               Refer to http://www.nick.rozanski.com/logger.htm for detailed instructions
//               on using the logger.
//
//
// PLATFORMS   : Win32 (Windows NT and Windows 95):
//               ----------------------------------
//                Compile flags (.obj) : /MD /DWIN32
//                Compile flags (.dll) : /MD /DWIN32 /DLOGGER_SHARED_LIB
//                Link flags           :
//               
//               Linux (needs glibc2.0.6):
//               -------------------------
//                Compile flags (.o)  : -I ../include -I ${SYBASE}/include -g -fPIC
//                Compile flags (.so) : -I ../include -I ${SYBASE}/include -g -fPIC -DLOGGER_SHARED_LIB
//                Link flags          : -shared
//
// SYBASE SUPPORT :
//
//               You do not need the Sybase header files to build the logger.  (If the 
//               Sybase header files are not present for the build, then the Sybase 
//               functionality is disabled, of course.)  To enable the Sybase
//               functionality when building the logger, define the macro
//
//                    LOGGER_BUILD_WITH_SYBASE_HEADERS
//
//               before running the build (ie /DLOGGER_BUILD_WITH_SYBASE_HEADERS for Win32,
//               -DLOGGER_BUILD_WITH_SYBASE_HEADERS for Linux).
//
// BUGS        : Win32
//               -----
//
//               Each copy of the C run-time (CRT) has its own heap and heap manager.  
//               (See Microsoft KB Article ID Q190799.)  Therefore, the calling program
//               MUST be built to use the same CRT which is used to build the Logger
//               DLL.  Otherwise, LoggerConfigure will fail in some cases.
//
//               In Visual C++, you must build the caller as described under "PLATFORMS".
//               In the IDE, set the flags using Settings|C/C++|Code Generation|
//               Use Run-Time Library.
//
//               Linux
//               -----
//
//               Currently LOGGER_SYBASE_SRVLOG is not supported on Linux.  This is
//               because of a bug in dlopen() (see www-gnats.gnu.org:8080/cgi-bin/wwwgnats.pl,
//               bug 753).
//
// MODIFICATION HISTORY
// --------------------
//
// Refer to master header file logger.h for full modification history.
//
// DISTRIBUTION
// ------------
// Copyright (C) 1998-2000 Nick Rozanski (Nick@Rozanski.com)
// Distributed under the terms of the GNU General Public License
//  as published by the Free Software Foundation
//  (675 Mass Ave, Cambridge, MA 02139, USA)
//
// The logger is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// ============================================================================

//
// which platform are we running?
//
#if	defined(WIN32)
#define	LOGGER_PLATFORM_IS_WIN32	1
#define	LOGGER_PLATFORM_IS_LINUX	0
#elif	defined(__linux__)
#define	LOGGER_PLATFORM_IS_WIN32	0
#define	LOGGER_PLATFORM_IS_LINUX	1
#else
#error	BUILD FAILURE - the Logger only runs on Win32 and Linux
#endif

// ============================================================================
//
// HEADER FILES
//
// ============================================================================

// ANSI headers
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

// Win32 headers
#if	LOGGER_PLATFORM_IS_WIN32
#include <windows.h>
#include <winbase.h>
#endif	// LOGGER_PLATFORM_IS_WIN32

// Linux headers
#if	LOGGER_PLATFORM_IS_LINUX
#include <linux/limits.h>
#include <dlfcn.h>
#include <syslog.h>
#include <libio.h>
#endif	// LOGGER_PLATFORM_IS_LINUX

// Sybase Open Server headers (if present)
#ifdef LOGGER_BUILD_WITH_SYBASE_HEADERS
#include <cspublic.h>
#include <ospublic.h>
#endif	// ifdef LOGGER_BUILD_WITH_SYBASE_HEADERS

// application header
#if	LOGGER_PLATFORM_IS_WIN32
#define LOGGER_DLLFN	__declspec(dllexport)
#else	// LOGGER_PLATFORM_IS_LINUX
#define LOGGER_DLLFN
#endif	// LOGGER_PLATFORM_IS_WIN32

#include "logger.h"

// ============================================================================
//
// EMBED VERSION INFORMATION
//
// ============================================================================

#ifdef LOGGER_BUILD_WITH_SYBASE_HEADERS
#define	LOGGER_VERSION_STRING	"*** " LOGGER_APPLICATION " version " LOGGER_VERSION " [with Sybase support] " LOGGER_COPYRIGHT
#else
#define	LOGGER_VERSION_STRING	"*** " LOGGER_APPLICATION " version " LOGGER_VERSION " " LOGGER_COPYRIGHT
#endif	// LOGGER_BUILD_WITH_SYBASE_HEADERS

static char __version[] = LOGGER_VERSION_STRING;
#if	LOGGER_PLATFORM_IS_WIN32
#pragma message(LOGGER_VERSION_STRING)
#endif

// ============================================================================
//
// LOCAL MACROS
//
// ============================================================================

//
// maximum filename size
//
#if	LOGGER_PLATFORM_IS_WIN32
#define	MAX_FILESIZE	MAX_PATH
#else	// LOGGER_PLATFORM_IS_LINUX
#define	MAX_FILESIZE	PATH_MAX
#endif	// LOGGER_PLATFORM_IS_WIN32

//
// the largest error message string we will return from LoggerConfigure
//
#define	LOGGER_ERROR_MSG_SIZE	255

//
// message class text
//
#define	LOGGER_INFO_TEXT			"INFORMATION"
#define	LOGGER_WARN_TEXT			"WARNING"
#define	LOGGER_ERROR_TEXT			"ERROR"
#define	LOGGER_DEBUG_TEXT			"DEBUG"
#define	LOGGER_AUDIT_SUCCESS_TEXT	"AUDIT_SUCCESS"
#define	LOGGER_AUDIT_FAILURE_TEXT	"AUDIT_FAILURE"
#define	LOGGER_OTHER_TEXT			"unclassified"

//
// Sybase dynamic libraries
//
#if	LOGGER_PLATFORM_IS_WIN32
#define	SYBASE_SRV_LOG_LIBRARY_NAME		"libsrv.dll"
#else	// LOGGER_PLATFORM_IS_LINUX
#define	SYBASE_SRV_LOG_LIBRARY_NAME		"libsrv.so"
#endif	// LOGGER_PLATFORM_IS_WIN32

#define	SYBASE_SRV_LOG_FUNCTION_NAME	"srv_log"

//
// miscellany
//
#define LOGGER_EOS		'\0'
#define LOGGER_UNUSED	0
#define LOGGER_USED		1

// ============================================================================
//
// GLOBAL VARIABLES
//
// ============================================================================

// debug level for use by callers
static int DebugLevel = 0;

// function pointer typedef for the "srv_log" function
#ifdef LOGGER_BUILD_WITH_SYBASE_HEADERS
typedef	CS_RETCODE (*srvlog_fptr)(SRV_SERVER*,CS_BOOL,CS_CHAR*,CS_INT);
#else // Sybase headers not in build
typedef	void (*srvlog_fptr)();
#endif // ifdef LOGGER_BUILD_WITH_SYBASE_HEADERS

// structure used to store filtering rules
typedef struct
{
	int       MessageClass;              // bitwise flags
	int       MessageSeverity;           // equality
	int       ThreadId;                  // equality
	char      SourceFile[MAX_FILESIZE];  // containing
	char      FuncName[1000];            // equality
} FilterData;

// static array of logger data - one per logger
typedef struct
{
	short int   Used;
	short int   Destination;
	char        Hostname[255];
	char        Application[255];
	char       *MsgBuffer;
	char        ANSIFileName[MAX_FILESIZE];
	FILE       *ANSIFilePtr;
	int         ANSIFileHandle;
#if	LOGGER_PLATFORM_IS_WIN32
	HANDLE      hWin32Console;
	char        Win32FileName[MAX_FILESIZE];
	HANDLE      hWin32File;
	HANDLE      hEventSource;
	char        Computer[1000];
#endif	// LOGGER_PLATFORM_IS_WIN32
#if	LOGGER_PLATFORM_IS_WIN32
	HINSTANCE   hDLL;
#else	// LOGGER_PLATFORM_IS_LINUX
	void       *LibHandle;
#endif	// LOGGER_PLATFORM_IS_WIN32
	srvlog_fptr Srvlog;
	short int   FilterSet;
	FilterData  Filter;
} LoggerData;

static LoggerData Loggers[LOGGER_MAX_LOGGERS];

// Win32 structure to protect shared Logger data in multi-thread environment
#if	LOGGER_PLATFORM_IS_WIN32
static CRITICAL_SECTION LoggerCriticalSection;
#endif	// LOGGER_PLATFORM_IS_WIN32

//
// Windows NT: thread local storage indexes for LoggerWriteMessage
//  (we use TLS rather than the stack because these structures are large)
//
#if	LOGGER_PLATFORM_IS_WIN32
DWORD TlsTmpBuffer;
DWORD TlsMsgBuffer;
DWORD TlsThisLogger;
#else	// LOGGER_PLATFORM_IS_LINUX
//
// static storage for Linux
//
static char       StaticTmpBuffer[LOGGER_BUFFERSIZE];
static char       StaticMsgBuffer[LOGGER_BUFFERSIZE];
static LoggerData StaticThisLogger[LOGGER_BUFFERSIZE];
#endif	// LOGGER_PLATFORM_IS_WIN32

// ============================================================================
//
// CODE MACROS
//
// ============================================================================

//
// ensure single-threaded access to the Logger static data (Windows NT DLL only)
//

#ifdef	LOGGER_SHARED_LIB

#if	LOGGER_PLATFORM_IS_WIN32
#define	START_SINGLE_THREAD		EnterCriticalSection(&LoggerCriticalSection);
#define	END_SINGLE_THREAD		LeaveCriticalSection(&LoggerCriticalSection);

#else	// LOGGER_PLATFORM_IS_LINUX
#define	START_SINGLE_THREAD		;
#define	END_SINGLE_THREAD		;
#endif	// LOGGER_PLATFORM_IS_WIN32

#else	// !LOGGER_SHARED_LIB

#define	START_SINGLE_THREAD		;
#define	END_SINGLE_THREAD		;

#endif	// LOGGER_SHARED_LIB

// ============================================================================
//
// LOCAL FUNCTIONS
//
// ============================================================================

void CloseLogger (LOGGER_ID LoggerId);

#ifdef	LOGGER_SHARED_LIB
#if	LOGGER_PLATFORM_IS_WIN32
// ============================================================================
//
// FUNCTION    : DllMain
//
// DESCRIPTION : this function is called by Windows NT to initialise the DLL
//               (aka DllEntryPoint)
//
// ARGUMENTS   : hModule
//               dwReasonForCall
//               lpReserved
//
// RETURNS     : TRUE
//
// ============================================================================
BOOL APIENTRY DllMain
(
	HANDLE hModule, 
    DWORD  dwReasonForCall,
	LPVOID lpReserved
)
{

#ifdef	RETURN_FAILURE
#undef	RETURN_FAILURE
#endif
#define	RETURN_FAILURE(r,f) \
	printf("DLLMain failed, reason=%d, function='%s', error=%d\n",r,f,GetLastError); \
	return FALSE;

	LOGGER_ID i;

    switch(dwReasonForCall)
	{
		case DLL_PROCESS_ATTACH:
			//
			// the DLL is attaching to the address space of the current process
			// as a result of the process starting up or as a result of a call
			// to LoadLibrary
			//
			// initialise the "critical section" object used to protect access
			// to the Logger static data
			//
			InitializeCriticalSection(&LoggerCriticalSection);

			// ensure single-threaded access to the Logger static data
			START_SINGLE_THREAD

			// initialise the Logger static data
			for(i=0;i<LOGGER_MAX_LOGGERS;i++){Loggers[i].Destination=LOGGER_UNUSED;}

			// end single-thread access to Logger static data
			END_SINGLE_THREAD

			// allocate thread local storage indexes for LoggerWriteMessage
			if((TlsTmpBuffer=TlsAlloc())==0xFFFFFFFF) { RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsAlloc") }
			if((TlsMsgBuffer=TlsAlloc())==0xFFFFFFFF) { RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsAlloc") }
			if((TlsThisLogger=TlsAlloc())==0xFFFFFFFF)    { RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsAlloc") }

			// allocate heap storage for the process's main thread
			if(!TlsSetValue(TlsTmpBuffer,malloc(LOGGER_BUFFERSIZE)))
			{
				RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsSetValue")
			}
			if(!TlsSetValue(TlsMsgBuffer,malloc(LOGGER_BUFFERSIZE)))
			{
				RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsSetValue")
			}
			if(!TlsSetValue(TlsThisLogger,malloc(sizeof(LoggerData))))
			{
				RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsSetValue")
			}

			break;
			
		case DLL_THREAD_ATTACH:
			// the current process is creating a new thread

			// allocate heap storage for the thread
			if(!TlsSetValue(TlsTmpBuffer,malloc(LOGGER_BUFFERSIZE)))
			{
				RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsSetValue")
			}
			if(!TlsSetValue(TlsMsgBuffer,malloc(LOGGER_BUFFERSIZE)))
			{
				RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsSetValue")
			}
			if(!TlsSetValue(TlsThisLogger,malloc(sizeof(LoggerData))))
			{
				RETURN_FAILURE(DLL_PROCESS_ATTACH,"TlsSetValue")
			}
			break;

		case DLL_THREAD_DETACH:
			//
			// a thread is exiting cleanly
			// free thread local storage used by thread
			//
			free(TlsGetValue(TlsTmpBuffer));
			free(TlsGetValue(TlsMsgBuffer));
			free(TlsGetValue(TlsThisLogger));
			break;

		case DLL_PROCESS_DETACH:
			//
			// the DLL is detaching from the address space of the calling process
			// as a result of either a clean process exit or of a call to FreeLibrary
			//
			break;
    }

    return TRUE;

}
#else	// LOGGER_PLATFORM_IS_LINUX

#endif	// LOGGER_PLATFORM_IS_WIN32
#endif	// LOGGER_SHARED_LIB

// ============================================================================
//
// FUNCTION    : LoggerGetDebugLevel, LoggerSetDebugLevel
//
// DESCRIPTION : get and set the debug level, respectively
//
//               this can be used as required by callers, but by convention
//               (and in logger_macros.h):
//
//                 debug level < 0 => error messages only logged
//                 debug level = 0 => information and error messages logged
//                 debug level > 0 => debug, information and error messages
//                                    logged
//
// ARGUMENTS   : debug level
//
// RETURNS     : debug level
//
// ============================================================================
int LOGGER_DLLFN LoggerGetDebugLevel() { return DebugLevel; }

int LOGGER_DLLFN LoggerSetDebugLevel(int DbgLvl) { DebugLevel = DbgLvl; return DebugLevel; }

// ============================================================================
//
// FUNCTION    : LoggerGetUnusedLogger
//
// DESCRIPTION : return the id of an unused logger, and mark it as used
//
// ARGUMENTS   : none
//
// RETURNS     : If there is an unused logger, return its id, otherwise return
//               LOGGER_NO_UNUSED_LOGGER
//
// ============================================================================
LOGGER_ID LOGGER_DLLFN LoggerGetUnusedLogger()
{
	LOGGER_ID i;

	// ensure single-threaded access to the Logger structures
	START_SINGLE_THREAD

	for(i=0;i<LOGGER_MAX_LOGGERS;i++)
	{
		if(Loggers[i].Used == LOGGER_UNUSED)
		{
			// mark this Logger as used
			Loggers[i].Used        = LOGGER_USED;
			Loggers[i].Destination = LOGGER_NONE;
			Loggers[i].FilterSet   = 0;

			// end single-thread access to Logger static data
			END_SINGLE_THREAD

			return i;
		}
	}

	// failed to find a free logger - end single-thread access to Logger static data
	END_SINGLE_THREAD

	return LOGGER_NO_UNUSED_LOGGER;
}

// ============================================================================
//
// FUNCTION    : LoggerMarkUnused
//
// DESCRIPTION : mark a logger as unused
//
// ARGUMENTS   : LoggerId
//
// RETURNS     : none
//
// ============================================================================
void LOGGER_DLLFN LoggerMarkUnused
(
	LOGGER_ID LoggerId
)
{
	// validate logger id
	if((LoggerId>=0)&&(LoggerId<LOGGER_MAX_LOGGERS))
	{

		// ensure single-threaded access to the Logger structures
		START_SINGLE_THREAD

		// close the logger (just in case)
		CloseLogger(LoggerId);

		// mark the logger as unused
		Loggers[LoggerId].Used      = LOGGER_UNUSED;
		Loggers[LoggerId].FilterSet = 0;

		// end single-thread access to Logger static data
		END_SINGLE_THREAD

	}

}

// ============================================================================
//
// FUNCTION    : LoggerConfigure
//
// DESCRIPTION : configure a logger, ie mark it as used and define its destination
//
// ARGUMENTS   : LoggerId
//
//                Id of logger to configure (as returned by LoggerLoggerGetUnusedLogger).
//                Must be in the range 0 .. (LOGGER_MAX_LOGGERS-1).
//
//               Hostname
//
//                Pointer to a null-terminated character string (max 255 chars) giving
//                the name of the logging host.  May be NULL pointer or empty string.
//
//               Application
//
//                Pointer to a null-terminated character string (max 255 chars) giving
//                the name of the logging application.  May be NULL pointer or empty string.
//
//               Destination 
//
//                Destination for this logger, that is, where messages should be
//                written.  It takes one of the following values:
//
//                 LOGGER_NONE              no logging
//
//                 LOGGER_FMTONLY           construct message and write it to
//                                           memory location DestDetails1 (which must
//                                           point to a static buffer at least
//                                           LOGGER_BUFFERSIZE bytes in size)
//
//                 LOGGER_ANSI_STDOUT       use ANSI 'C' functions to write to stdout
//
//                 LOGGER_ANSI_FILENAME     use ANSI 'C' functions to append to the
//                                           file named (*DestDetails1).  LoggerInitialise
//                                           will open the file
//
//                 LOGGER_ANSI_FILEPTR      use ANSI 'C' functions to write to the file
//                                           which has previously been opened by the caller,
//                                           using file pointer (*DestDetails1)
//
//                 LOGGER_WIN32_CONSOLE     use Win32 functions to write to console with handle
//                                           (*lpDestDetails1)
//
//                 LOGGER_WIN32_FILENAME    use Win32 functions to append to the
//                                           file named (*DestDetails1).  LoggerInitialise
//                                           will open the file
//
//                 LOGGER_WIN32_FILEHANDLE  use Win32 functions to write to the file
//                                           which has previously been opened by the caller,
//                                           using file handle (*DestDetails1)
//
//                 LOGGER_WIN32_EVENTLOG    write to the Windows NT Event Log on the
//                                           computer named (*DestDetails1)
//
//                 LOGGER_SYBASE_SRVLOG     write to the Sybase Open Server error log
//
//                 LOGGER_UNIX_SYSLOG    write to the Unix system logger (syslogd)
//
//               DestDetails1
//
//                The meanings of DestDetails1 and DestDetails2 depend on the value of
//                DestinationType:
//
//                 DestinationType         DestDetails1 is ...
//                 ----------------         ---------------------
//                 LOGGER_FMTONLY           char*
//                   (WL)                    (the address of a buffer into which formatted messages
//                                           will be written;  must have static scope)
//
//                 LOGGER_ANSI_STDOUT       ignored
//                   (WL)
//
//                 LOGGER_ANSI_FILENAME     char*
//                   (WL)                    (a null-terminated string specifying the pathname of
//                                           the file to append to)
//
//                 LOGGER_ANSI_FILEPTR      FILE*
//                   (WL)                   (previously opened by caller for writing, specifying
//                                          the file to append to)
//
//                 LOGGER_WIN32_CONSOLE     HANDLE* (NB not just a HANDLE)
//                   (W)                     (previously opened by caller for writing using
//                                           GetStdHandle or AllocConsole)
//
//                 LOGGER_WIN32_FILENAME    char*
//                   (W)                     (a null-terminated string specifying the pathname of
//                                           the file to append to)
//
//                 LOGGER_WIN32_FILEHANDLE  HANDLE* (NB not just a HANDLE)
//                   (W)                     (previously opened by caller for writing, specifying
//                                           the file to append to
//
//                 LOGGER_WIN32_EVENTLOG    char*
//                   (W)                    (a null-terminated string specifying the computer name
//                                          on which the message should be logged; NULL or the empty
//                                          string to log to this computer)
//
//                 LOGGER_SYBASE_SRVLOG     ignored
//                   (WL)
//
//                 Destination Types marked (W) are for Windows NT only.  Destination Types marked
//                 (L) are for Linux only.  Destination Types marked (WL) are for both.
//
//               DestDetails2
//
//                 For LOGGER_ANSI_FILENAME and LOGGER_WIN32_FILENAME, DestDetails2, if not NULL,
//                 should be a pointer to a boolean value (int 0 or 1).  If true, the output
//                 file will be truncated by LoggerConfigure.
//
//                 DestDetails2 is ignored for other destination types.
//
//               ErrorPtr
//
//                 If LoggerInitialise fails, and ErrorPtr is not NULL, LoggerInitialise sets
//                 (*ErrorPtr) to an integer containing the ANSI or Win32 error code.
//
// NOTES       : Invocation
//
//                LoggerInitialise can be called one or more times, or not at all.  If it is never
//                called, messages are logged to ANSI stdout.
//
//               Multi-threading Support
//
//                The Logger should work correctly in a multithreading Windows NT environment;
//                a Win32 "critical section" is used to single-thread through this whole function.
//                However I have not extensively tested this.
//
//                No multi-threading support is provided for Linux (yet).  You may find that messages
//                get garbled.
//
//               LOGGER_SYBASE_SRVLOG
//
//                The logger loads the Sybase Open Server DLL (LIBSRV.DLL for NT, libsrv.so for Linux)
//                dynamically as necessary.  This is so that the Logger will work in environments in
//                which Open Server is not installed.
//
//                If LoggerConfigure is invoked with th LOGGER_SYBASE_SRVLOG option, the Sybase Open
//                Server DLL must either be loaded or findable by the operating system.  Windows NT
//                uses %PATH% to find DLLs;  Linux uses the entries in /etc/ld.so.conf.
//
// RETURNS     : If the function succeeds, the return value is nonzero.  If the function fails,
//               the return value is zero.
//
//               Failure can occur if the supplied filename cannot be opened, or if an invalid
//               message destination is specified.  In the former case, an ANSI or Win32 error
//               number is returned in (*ErrorPtr) if ErrorPtr is not NULL.  Following any failure,
//               an error string is returned in (*ErrorMsgPtr) if ErrorMsgPtr is not null.
//
//               A value of -1 for (*ErrorPtr) signifies a bad call to LoggerInitialise.  A value
//               of -2 signifies that an action (eg attempt to open a file) failed.
//
// ============================================================================
int LOGGER_DLLFN LoggerConfigure
(
	LOGGER_ID  LoggerId,
	char       Hostname[],
	char       Application[],
	int        Destination,
	void      *DestDetails1,
	void      *DestDetails2,
	int       *ErrorPtr,
    char      *ErrorMsgPtr
)
{

#ifdef	RETURN_FAILURE
#undef	RETURN_FAILURE
#endif


#define	RETURN_FAILURE(msg) if(ErrorPtr != NULL)                                  \
								{(*ErrorPtr) = -2;}                               \
							if(ErrorMsgPtr != NULL)                               \
								{strncpy(ErrorMsgPtr,msg,LOGGER_ERROR_MSG_SIZE);} \
							goto TheEnd;

#define	RETURN_BAD_CALL(msg)	if(ErrorPtr != NULL)                                  \
									{(*ErrorPtr) = -1;}                               \
								if(ErrorMsgPtr != NULL)                               \
									{strncpy(ErrorMsgPtr,msg,LOGGER_ERROR_MSG_SIZE);} \
								goto TheEnd;

#define	CHECK_NOTNULL_DEST	if(DestDetails1 == NULL) \
								{ RETURN_BAD_CALL("DestDetails1 must not be NULL") }

#define	TRUNCATE_FILE_REQUESTED	\
			(DestDetails2==NULL)?0:(((*(int*)DestDetails2)==0)?0:1)

	int   rc = 0;
	char *ErrorStringPtr=NULL;

	// ensure single-threaded access to the Logger static data
	START_SINGLE_THREAD

	// validate logger id
	if((LoggerId<0)||(LoggerId>=LOGGER_MAX_LOGGERS))
	{
		RETURN_BAD_CALL("logger id out of range")
	}
	else
	{
		// close the logger (just in case)
		CloseLogger(LoggerId);

		// mark this logger as used
		Loggers[LoggerId].Used      = LOGGER_USED;
		Loggers[LoggerId].FilterSet = 0;
	}

	// save host name
	if(Hostname==NULL)
	{
		Loggers[LoggerId].Hostname[0] = LOGGER_EOS;
	}
	else
	{
		strcpy(Loggers[LoggerId].Hostname,Hostname);
	}
		// save application name
	if(Application==NULL)
	{
		Loggers[LoggerId].Application[0] = LOGGER_EOS;
	}
	else
	{
		strcpy(Loggers[LoggerId].Application,Application);
	}
	
	// get message destination
	Loggers[LoggerId].Destination = Destination;

	switch(Destination)
	{
		case LOGGER_NONE: case LOGGER_ANSI_STDOUT:

			// return success
			rc = 1;
			break;

		case LOGGER_FMTONLY:

			// store address of message buffer
			Loggers[LoggerId].MsgBuffer = (char*)DestDetails1;
			// return success
			rc = 1;
			break;

		case LOGGER_ANSI_FILENAME:

			// store file name
			CHECK_NOTNULL_DEST
			strcpy(Loggers[LoggerId].ANSIFileName,(char*)DestDetails1);

			// truncate or append?
			if(TRUNCATE_FILE_REQUESTED)
			{
				// open the file (truncate if it exists)
				Loggers[LoggerId].ANSIFilePtr = fopen(Loggers[LoggerId].ANSIFileName,"w");
			}
			else
			{
				// open the file (append if it exists)
				Loggers[LoggerId].ANSIFilePtr = fopen(Loggers[LoggerId].ANSIFileName,"a");
			}
			// was open successful?
			if(Loggers[LoggerId].ANSIFilePtr == NULL) { RETURN_FAILURE("failed to open file") }

			// return success
			rc = 1;
			break;

		case LOGGER_ANSI_FILEPTR:

			// store file pointer
			CHECK_NOTNULL_DEST
			Loggers[LoggerId].ANSIFilePtr = (FILE*)DestDetails1;

			// return success
			rc = 1;
			break;

#if	LOGGER_PLATFORM_IS_WIN32

		case LOGGER_WIN32_CONSOLE:

			// store console handle
			CHECK_NOTNULL_DEST
			Loggers[LoggerId].hWin32Console = *(HANDLE*)DestDetails1;

			// return success
			rc = 1;
			break;

#endif	// LOGGER_PLATFORM_IS_WIN32

#if	LOGGER_PLATFORM_IS_WIN32

		case LOGGER_WIN32_FILENAME:

			// store file name
			CHECK_NOTNULL_DEST
			strcpy(Loggers[LoggerId].Win32FileName,(char*)DestDetails1);

			// truncate or append?
			if(TRUNCATE_FILE_REQUESTED)
			{
				// open the file (truncate if it exists)
				Loggers[LoggerId].hWin32File = CreateFile(Loggers[LoggerId].Win32FileName,GENERIC_WRITE,
												FILE_SHARE_READ|FILE_SHARE_WRITE,
												NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
			}
			else
			{
				// open the file (append if it exists)
				Loggers[LoggerId].hWin32File = CreateFile(Loggers[LoggerId].Win32FileName,GENERIC_WRITE,
												FILE_SHARE_READ|FILE_SHARE_WRITE,
												NULL,OPEN_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
			}
			// was open successful?
			if(Loggers[LoggerId].hWin32File == INVALID_HANDLE_VALUE)
			{
				RETURN_FAILURE("failed to open file")
			}

			// return success
			rc = 1;
			break;

#endif	// LOGGER_PLATFORM_IS_WIN32

#if	LOGGER_PLATFORM_IS_WIN32

		case LOGGER_WIN32_FILEHANDLE:

			// store file handle
			CHECK_NOTNULL_DEST
			Loggers[LoggerId].hWin32File = *(HANDLE*)DestDetails1;

			// return success
			rc = 1;
			break;

#endif	// LOGGER_PLATFORM_IS_WIN32

#if	LOGGER_PLATFORM_IS_WIN32

		case LOGGER_WIN32_EVENTLOG:

			// store computer name
			if(DestDetails1 == NULL)
			{
				Loggers[LoggerId].Computer[0] = LOGGER_EOS;
			}
			else
			{
				strcpy(Loggers[LoggerId].Computer,(char*)DestDetails1);
			}

			// register event source so we can log events
			if(Loggers[LoggerId].Computer[0] = LOGGER_EOS)
			{
				Loggers[LoggerId].hEventSource = RegisterEventSource(NULL,Loggers[LoggerId].Application);
			}
			else
			{
				Loggers[LoggerId].hEventSource = RegisterEventSource(Loggers[LoggerId].Computer,
																	Loggers[LoggerId].Application);
			}

			if(Loggers[LoggerId].hEventSource == INVALID_HANDLE_VALUE)
			{
				if(ErrorPtr!=NULL) { (*ErrorPtr) = (int)GetLastError; }
				if(ErrorMsgPtr!=NULL)
				{
					strncpy(ErrorMsgPtr,"failed to register event source",LOGGER_ERROR_MSG_SIZE);
				}
				goto TheEnd;
			}

			// return success
			rc = 1;
			break;

#endif	// LOGGER_PLATFORM_IS_WIN32

		case LOGGER_SYBASE_SRVLOG:

			// load the Sybase Open Server DLL

#if	LOGGER_PLATFORM_IS_WIN32

			Loggers[LoggerId].hDLL = LoadLibrary(SYBASE_SRV_LOG_LIBRARY_NAME);
			if(Loggers[LoggerId].hDLL == NULL)
			{
				if(ErrorPtr!=NULL) { (*ErrorPtr) = (int)GetLastError; }
				if(ErrorMsgPtr!=NULL)
				{
					strncpy(ErrorMsgPtr,"failed to load library",LOGGER_ERROR_MSG_SIZE);
				}
				goto TheEnd;
			}
			// save the function address for "srv_log"
			Loggers[LoggerId].Srvlog = (srvlog_fptr)GetProcAddress(Loggers[LoggerId].hDLL,
											SYBASE_SRV_LOG_FUNCTION_NAME);
			if(!Loggers[LoggerId].Srvlog)
			{
				if(ErrorPtr!=NULL) { (*ErrorPtr) = (int)GetLastError; }
				if(ErrorMsgPtr!=NULL)
				{
					strncpy(ErrorMsgPtr,"failed to retrieve function address",LOGGER_ERROR_MSG_SIZE);
				}
				FreeLibrary(Loggers[LoggerId].hDLL);
				goto TheEnd;
			}

#else	// LOGGER_PLATFORM_IS_LINUX

			Loggers[LoggerId].LibHandle = dlopen(SYBASE_SRV_LOG_LIBRARY_NAME,RTLD_LAZY);
			if(Loggers[LoggerId].LibHandle == NULL)
			{
				if(ErrorPtr!=NULL) { (*ErrorPtr) = (int)-2; }
				if(ErrorMsgPtr!=NULL)
				{
					strncpy(ErrorMsgPtr,dlerror(),LOGGER_ERROR_MSG_SIZE);
				}
				goto TheEnd;
			}
			// save the function address for "srv_log"
			Loggers[LoggerId].Srvlog = (srvlog_fptr)dlsym(Loggers[LoggerId].LibHandle,
											SYBASE_SRV_LOG_FUNCTION_NAME);
			if(Loggers[LoggerId].Srvlog==NULL)
			{
				if(ErrorPtr!=NULL) { (*ErrorPtr) = (int)-2; }
				if(ErrorMsgPtr!=NULL)
				{
					strncpy(ErrorMsgPtr,dlerror(),LOGGER_ERROR_MSG_SIZE);
				}
				dlclose(Loggers[LoggerId].LibHandle);
				goto TheEnd;
			}

#endif	// LOGGER_PLATFORM_IS_WIN32

			// return success
			rc = 1;
			break;

#if	LOGGER_PLATFORM_IS_LINUX

		case LOGGER_UNIX_SYSLOG:

			// open syslog (not strictly necessary according to man (3)
			openlog(Loggers[LoggerId].Application,0,LOG_USER);

			// return success
			rc = 1;
			break;

#endif	// LOGGER_PLATFORM_IS_LINUX

		default:
			RETURN_BAD_CALL("invalid destination")
			break;
	
	}

TheEnd:

	// end single-thread access to Logger static data
	END_SINGLE_THREAD

	return rc;

}

// ============================================================================
//
// FUNCTION    : LoggerSetFilter
//
// DESCRIPTION : define filter criteria for a logger, or switch filtering off
//
// ARGUMENTS   : FilterAll   if true (!=0) then switch off filtering
//               MsgClass    class of message (information, debug etc)
//               MsgSeverity severity of message
//               ThreadId    identifier of calling thread
//               SourceFile  name of source file
//               FuncName    name of function
//
// LoggerId
//
//                Id of logger (as returned by LoggerLoggerGetUnusedLogger).
//                Must be in the range 0 .. (LOGGER_MAX_LOGGERS-1).
//
// MsgClass
//
//               Bitwise OR of one or more of the following values:
//                 LOGGER_BARE_FILTER
//                 LOGGER_INFO_FILTER
//                 LOGGER_WARN_FILTER
//                 LOGGER_ERROR_FILTER
//                 LOGGER_DEBUG_FILTER
//                 LOGGER_AUDIT_SUCCESS_FILTER
//                 LOGGER_AUDIT_FAILURE_FILTER
//
//                If the corresponding bit is set, then messages of that class will be logged
//                by the given logger.
//
// MsgSeverity
//
//               If greater than zero, only messages of this severity will be logged.
//               Use a negative value to log messages of any severity.
//
// ThreadId
//
//               If greater than zero, only messages from this thread will be logged.
//               Use a negative value to log messages from any thread.
//
// SourceFile
//
//                If not NULL or an empty string, then only messages from this source file
//                will be logged.  Note that the test here is inclusion (so you only need specify
//                a file name rather than a full path name).
//
// Func
//
//                If not NULL or an empty string, then only messages from this function
//                will be logged.
//
// RETURNS     : n/a
//
// ============================================================================
void LOGGER_DLLFN LoggerSetFilter
(
	LOGGER_ID LoggerId,
	int   FilterAll,
	int   MsgClass,
	int   MsgSeverity,
	int   ThreadId,
	char *SourceFile,
	char *FuncName
)

{
	// validate logger id
	if((LoggerId<0)||(LoggerId>=LOGGER_MAX_LOGGERS))
	{
		return;
	}
	if(!Loggers[LoggerId].Used)
	{
		return;
	}

	// is FilterAll set?
	if(FilterAll)
	{
		Loggers[LoggerId].FilterSet = 0;
		return;
	}

	// a real filter is being set
	Loggers[LoggerId].Filter.MessageClass    = MsgClass;
	Loggers[LoggerId].Filter.MessageSeverity = MsgSeverity;
	Loggers[LoggerId].Filter.ThreadId        = ThreadId;
	if(SourceFile!=NULL)
	{
		strncpy(Loggers[LoggerId].Filter.SourceFile,SourceFile,sizeof(Loggers[LoggerId].Filter.SourceFile));
	}
	if(FuncName!=NULL)
	{
		strncpy(Loggers[LoggerId].Filter.FuncName,FuncName,sizeof(Loggers[LoggerId].Filter.FuncName));
	}
	Loggers[LoggerId].FilterSet = 1;
	return;
}

// ============================================================================
//
// FUNCTION    : LoggerWriteMessage
//
// DESCRIPTION : log text message to each Logger which has been configured using
//               LoggerInitialise
//
// ARGUMENTS   : MsgClass    class of message (information, debug etc)
//               MsgSeverity severity of message
//               ThreadId    identifier of calling thread
//               SourceFile  name of source file
//               LineNumber  line number
//               FuncName    name of function
//               MsgText     message text 
//                ...          other arguments
//
// MsgClass, MsgSeverity
//
//               These parameters define the class and severity of the message, that is how
//               it should be viewed by the recipient.  MsgClass takes one of the following values:
//
//                 LOGGER_BARE          bare message (no other elements added to it)
//                 LOGGER_INFO          this is an informational message
//                 LOGGER_WARN          this is a warning message
//                 LOGGER_ERROR         this is an error message
//                 LOGGER_DEBUG         this is a debug message
//                 LOGGER_AUDIT_SUCCESS this is an audit (success) message
//                 LOGGER_AUDIT_FAILURE this is an audit (failure) message
//
//               MsgSeverity is an application-specific integer (a smaller number is generally
//               used to indicate greater severity; a negative severity is ignored).
//
// ThreadId
//
//                The identifier of the calling thread.  This supports programs which use NT
//                native threads, and those which implement their own threading.  If set to
//                -1, it is ignored;  if set to -2, the value returned by the Win32 call
//                GetCurrentThreadId is included in the message.  Otherwise, the value of
//                ThreadId is included in the message.
//
// SourceFile, LineNumber, FuncName
//
//                These parameters define where in the source the message came from.  They
//                represent the source file (eg __FILE__), line number (eg __LINE__), and
//                function name respectively.  The source file and function can be the empty
//                string, in which case they will be ignored;  similarly a line number less than
//                zero will be ignored.
//
// MsgText, ...
//
//                This is the text of the message.  It may include sprintf conversion
//                specifiers such as %s, %d etc.  These must match the remaining arguments to
//                the function, as if they were being passed to sprintf.
//
// EXAMPLES    :
//
// RETURNS     : n/a
//
//                The function does no error checking.  If anything goes wrong it fails silently.
//
// ============================================================================
void LOGGER_DLLFN LoggerWriteMessage
(
	int  MsgClass,
	int  MsgSeverity,
	int  ThreadId,
	char SourceFile[],
	int  LineNumber,
	char FuncName[],
	char MsgText[],
	...
)
{

	// local variables
	LOGGER_ID    LoggerId;
	char         ClassBuffer[30];
	char        *TmpBuffer;
	char        *MsgBuffer;
	LoggerData  *ThisLogger;
	char         ThreadBuffer[20];
	char         NumBuffer[20];
	va_list      ArgList;
	time_t       now1;
	struct tm   *now2;
	char         TimeString[30];
	struct stat  FstatBuffer;
#if	LOGGER_PLATFORM_IS_WIN32
	int          FilePosition;
	int          MsgLength;
	int          CharsWritten;
	WORD         wEventType;
	LPTSTR       lpszStrings[1];
#endif	// LOGGER_PLATFORM_IS_WIN32
	int          FilterInclude;

#if	LOGGER_PLATFORM_IS_WIN32

	// Windows NT - get thread local storage for this thread
	TmpBuffer =(char*)TlsGetValue(TlsTmpBuffer);
	MsgBuffer =(char*)TlsGetValue(TlsMsgBuffer);
	ThisLogger=(LoggerData*)TlsGetValue(TlsThisLogger);

#else	// LOGGER_PLATFORM_IS_LINUX

	// Linux - get static storage
	TmpBuffer =StaticTmpBuffer;
	MsgBuffer =StaticMsgBuffer;
	ThisLogger=StaticThisLogger;

#endif	// LOGGER_PLATFORM_IS_WIN32
		
	for(LoggerId=0;LoggerId<LOGGER_MAX_LOGGERS;LoggerId++)
	{

		// printf("+++ LoggerWriteMessage(%d,...)\n",LoggerId);

		// ensure single-threaded access to the Logger static data
		START_SINGLE_THREAD

		// is this logger used?
		if((Loggers[LoggerId].Used == LOGGER_USED)&&(Loggers[LoggerId].Destination != LOGGER_NONE))
		{

			// copy the data for this logger
			memcpy(ThisLogger,&Loggers[LoggerId],sizeof(LoggerData));

			// end single-thread access to Logger static data
			END_SINGLE_THREAD

			// does a filter apply?
			if(Loggers[LoggerId].FilterSet)
			{
				// check message class
				FilterInclude=
				(
					((MsgClass==LOGGER_BARE)&&(Loggers[LoggerId].Filter.MessageClass&LOGGER_BARE_FILTER))||
					((MsgClass==LOGGER_INFO)&&(Loggers[LoggerId].Filter.MessageClass&LOGGER_INFO_FILTER))||
					((MsgClass==LOGGER_WARN)&&(Loggers[LoggerId].Filter.MessageClass&LOGGER_WARN_FILTER))||
					((MsgClass==LOGGER_ERROR)&&(Loggers[LoggerId].Filter.MessageClass&LOGGER_ERROR_FILTER))||
					((MsgClass==LOGGER_DEBUG)&&(Loggers[LoggerId].Filter.MessageClass&LOGGER_DEBUG_FILTER))||
					((MsgClass==LOGGER_AUDIT_SUCCESS)&&(Loggers[LoggerId].Filter.MessageClass&LOGGER_AUDIT_SUCCESS_FILTER))||
					((MsgClass==LOGGER_AUDIT_FAILURE)&&(Loggers[LoggerId].Filter.MessageClass&LOGGER_AUDIT_FAILURE_FILTER))
				);
				// printf("+++ after class: FilterInclude=%d\n",FilterInclude);

				// if severity filter defined (>0), supplied severity must equal or exceed it
				if(Loggers[LoggerId].Filter.MessageSeverity>=0)
				{
					// printf("+++ severity: logger %d message %d\n",Loggers[LoggerId].Filter.MessageSeverity,MsgSeverity);
					FilterInclude=FilterInclude&&(MsgSeverity>=Loggers[LoggerId].Filter.MessageSeverity);
				}
				// printf("+++ after severity: FilterInclude=%d\n",FilterInclude);

				// if thread id filter defined (>0), supplied thread id must match
				if(Loggers[LoggerId].Filter.ThreadId>=0)
				{
					if(ThreadId=-2)
					{
#if	LOGGER_PLATFORM_IS_WIN32
						FilterInclude=FilterInclude&&(((int)GetCurrentThreadId())==Loggers[LoggerId].Filter.ThreadId);
#else	// LOGGER_PLATFORM_IS_LINUX
						FilterInclude=0;
#endif	// LOGGER_PLATFORM_IS_WIN32
					}
					else
					{
						FilterInclude=FilterInclude&&(ThreadId==Loggers[LoggerId].Filter.ThreadId);
					}
				}
				// printf("+++ after thread: FilterInclude=%d\n",FilterInclude);

				// if source file filter defined, then supplied file name must be a substring
				if(Loggers[LoggerId].Filter.SourceFile[0]!='\0')
				{
					if(SourceFile!=NULL)
					{
						FilterInclude=FilterInclude&&(strstr(SourceFile,Loggers[LoggerId].Filter.SourceFile)!=NULL);
					}
					else
					{
						FilterInclude=0;
					}
				}
				// printf("+++ after source file: FilterInclude=%d\n",FilterInclude);

				// if function name filter defined, then supplied function name must match
				if(Loggers[LoggerId].Filter.FuncName[0]!='\0')
				{
					if(FuncName!=NULL)
					{
						FilterInclude=FilterInclude&&(!strcmp(FuncName,Loggers[LoggerId].Filter.FuncName));
					}
					else
					{
						FilterInclude=0;
					}
				}
				// printf("+++ after function: FilterInclude=%d\n",FilterInclude);

				// does a filter apply?
				if(!FilterInclude)
				{
					// filter exclusion
					goto LOGGER_WRITE_MESSAGE_NEXT;
				}

			}

			// build up the full message string
			if(MsgClass==LOGGER_BARE)
			{
				// just log the message text
				strcpy(TmpBuffer,MsgText);
			}
			else
			{
	
				// get message class text
				switch(MsgClass)
				{
					case LOGGER_INFO:          strcpy(ClassBuffer,LOGGER_INFO_TEXT);          break;
					case LOGGER_WARN:          strcpy(ClassBuffer,LOGGER_WARN_TEXT);          break;
					case LOGGER_ERROR:         strcpy(ClassBuffer,LOGGER_ERROR_TEXT);         break;
					case LOGGER_DEBUG:         strcpy(ClassBuffer,LOGGER_DEBUG_TEXT);         break;
					case LOGGER_AUDIT_SUCCESS: strcpy(ClassBuffer,LOGGER_AUDIT_SUCCESS_TEXT); break;
					case LOGGER_AUDIT_FAILURE: strcpy(ClassBuffer,LOGGER_AUDIT_FAILURE_TEXT); break;
					default:                   strcpy(ClassBuffer,LOGGER_INFO_TEXT);          break;
				}

				//
				// get the current date and time, unless logging to the NT Event Log,
				// Sybase Open Server log or Unix syslog
				//
				if((ThisLogger->Destination != LOGGER_WIN32_EVENTLOG)&&
				   (ThisLogger->Destination != LOGGER_SYBASE_SRVLOG)&&
				   (ThisLogger->Destination != LOGGER_UNIX_SYSLOG))
				{
					// get the date and time
					now1 = time(NULL);
					now2 = localtime(&now1);
					// convert to a string
					sprintf(TimeString,"%4d/%02d/%02d %02d:%02d:%02d ",
							now2->tm_year+1900,now2->tm_mon+1,now2->tm_mday,
							now2->tm_hour,now2->tm_min,now2->tm_sec);
				}
				else
				{
					strcpy(TimeString,"");
				}

				// build up the first part of the message string
				if((ThisLogger->Hostname[0] != LOGGER_EOS)&&
				   (ThisLogger->Destination != LOGGER_WIN32_EVENTLOG)&&
				   (ThisLogger->Destination != LOGGER_UNIX_SYSLOG))
				{
					sprintf(TmpBuffer,"[%s] ",ThisLogger->Hostname);
				}
				else
				{
					TmpBuffer[0]=LOGGER_EOS;
				}

				// add the application name
				if((ThisLogger->Application[0] != LOGGER_EOS)&&
				   (ThisLogger->Destination != LOGGER_WIN32_EVENTLOG)&&
				   (ThisLogger->Destination != LOGGER_UNIX_SYSLOG))
				{
					strcat(TmpBuffer,ThisLogger->Application);
					strcat(TmpBuffer,": ");
					strcat(TmpBuffer,TimeString);
					strcat(TmpBuffer,ClassBuffer);
				}
				else
				{
					strcat(TmpBuffer,TimeString);
					strcat(TmpBuffer,ClassBuffer);
				}

				// add the severity to the message, if non-negative
				if(MsgSeverity>=0)
				{
					strcat(TmpBuffer," severity=");
					sprintf(NumBuffer,"%d",MsgSeverity);
					strcat(TmpBuffer,NumBuffer);
				}

				// add the thread id to the message, if required
				if(ThreadId != -1)
				{
					if(ThreadId == -2)
					{
						strcat(TmpBuffer," thread=");

#if	LOGGER_PLATFORM_IS_WIN32
						sprintf(ThreadBuffer,"%d",(int)GetCurrentThreadId());
#else	// LOGGER_PLATFORM_IS_LINUX
						strcpy(ThreadBuffer,"?");
#endif	// LOGGER_PLATFORM_IS_WIN32

						strcat(TmpBuffer,ThreadBuffer);
					}
					else
					{
						strcat(TmpBuffer," thread=");
						sprintf(ThreadBuffer,"%d",ThreadId);
						strcat(TmpBuffer,ThreadBuffer);
					}
				}

				// add the source file to the message, if non-null
				if(SourceFile != NULL)
				{
					if(*SourceFile != LOGGER_EOS)
					{
						strcat(TmpBuffer," source=");
						strcat(TmpBuffer,SourceFile);
					}
				}

				// add the line number to the message, if non-negative
				if(LineNumber >= 0)
				{
					strcat(TmpBuffer," line=");
					sprintf(NumBuffer,"%d",LineNumber);
					strcat(TmpBuffer,NumBuffer);
				}

				// add the function name to the message, if non-null
				if(FuncName != NULL)
				{
					if(*FuncName != LOGGER_EOS)
					{
						strcat(TmpBuffer," function=");
						strcat(TmpBuffer,FuncName);
					}
				}

				// add the message text
				strcat(TmpBuffer," text=");
				strcat(TmpBuffer,MsgText);

			}
	
			//
			// add a carriage return, unless destination is "format only"
			// or logging to the NT Event Log
			//
			if((ThisLogger->Destination != LOGGER_FMTONLY)&&
			   (ThisLogger->Destination != LOGGER_WIN32_EVENTLOG))
			{
				strcat(TmpBuffer,"\n");
			}

			// initialise the ANSI "varargs" library
		    va_start(ArgList,MsgText);

			// copy the variable arguments into the message
			vsprintf(MsgBuffer,TmpBuffer,ArgList);

			// end the "varargs" processing
		    va_end(ArgList);

			// write the message
			switch(ThisLogger->Destination)
			{

				case LOGGER_FMTONLY:

					strcpy(ThisLogger->MsgBuffer,MsgBuffer);
					break;

				case LOGGER_ANSI_STDOUT:

					fprintf(stdout,MsgBuffer); fflush(stdout);
					break;

				case LOGGER_ANSI_FILENAME: case LOGGER_ANSI_FILEPTR:
    		
					// check that the file handle is stil valid
					if(fstat(fileno(ThisLogger->ANSIFilePtr),&FstatBuffer)==0)
					{
						fprintf(ThisLogger->ANSIFilePtr,MsgBuffer);
						fflush(ThisLogger->ANSIFilePtr);
					}
					break;


#if	LOGGER_PLATFORM_IS_WIN32

				case LOGGER_WIN32_CONSOLE:
			
					// get message length
					MsgLength=strlen(MsgBuffer);

					// write to Win32 stdout
					WriteConsole(ThisLogger->hWin32Console,(CONST VOID*)MsgBuffer,
								MsgLength,&CharsWritten,NULL);
					break;

#endif	// LOGGER_PLATFORM_IS_WIN32

#if	LOGGER_PLATFORM_IS_WIN32

				case LOGGER_WIN32_FILENAME: case LOGGER_WIN32_FILEHANDLE:

					// get message length
					MsgLength=strlen(MsgBuffer);

					// move to end of file
					FilePosition=SetFilePointer(ThisLogger->hWin32File,
													0,NULL,FILE_END);

					// lock file for writing
					LockFile(ThisLogger->hWin32File,FilePosition,
								0,FilePosition+MsgLength,0);

					// write to the file
					WriteFile(ThisLogger->hWin32File,MsgBuffer,MsgLength,
									&CharsWritten,NULL);

					// unlock the file
					UnlockFile(ThisLogger->hWin32File,FilePosition,0,
								FilePosition+MsgLength,0);

					// flush to disk
					FlushFileBuffers(ThisLogger->hWin32File);

					break;

#endif	// LOGGER_PLATFORM_IS_WIN32

#if	LOGGER_PLATFORM_IS_WIN32

				case LOGGER_WIN32_EVENTLOG:

					// build up event string array
					lpszStrings[0] = MsgBuffer;
					switch(MsgClass)
					{
						case LOGGER_INFO: case LOGGER_DEBUG:
							wEventType=EVENTLOG_INFORMATION_TYPE;
							break;
						case LOGGER_WARN:
							wEventType=EVENTLOG_WARNING_TYPE;
							break;
						case LOGGER_ERROR:
							wEventType=EVENTLOG_ERROR_TYPE;
							break;
						case LOGGER_AUDIT_SUCCESS:
							wEventType=EVENTLOG_AUDIT_SUCCESS;
							break;
						case LOGGER_AUDIT_FAILURE:
							wEventType=EVENTLOG_AUDIT_FAILURE;
							break;
						default:
							wEventType=EVENTLOG_INFORMATION_TYPE;
							break;
					}

					// now report the event
					ReportEvent(ThisLogger->hEventSource,wEventType,0,0,
								NULL,1,0,(LPCTSTR*)lpszStrings,NULL);
					break;

#endif	// LOGGER_PLATFORM_IS_WIN32

				case LOGGER_SYBASE_SRVLOG:

#ifdef LOGGER_BUILD_WITH_SYBASE_HEADERS
					// printf("+++ %d: about to call srv_log(%s) @%p\n",LoggerId,MsgBuffer,(void*)ThisLogger->Srvlog);
					(void)(*(ThisLogger->Srvlog))(NULL,CS_TRUE,MsgBuffer,CS_NULLTERM);
					// printf("+++ call to srv_log() complete\n");
#endif
					// printf("+++ %d: about to break\n",LoggerId);
					break;

#if	LOGGER_PLATFORM_IS_LINUX

		case LOGGER_UNIX_SYSLOG:

					syslog(LOG_NOTICE,MsgBuffer);
					break;

#endif	// LOGGER_PLATFORM_IS_LINUX

				default:
					break;

			}

		}
		else
		{

			// end single-thread access to Logger static data
			END_SINGLE_THREAD

		}
LOGGER_WRITE_MESSAGE_NEXT:
		;

	} // end for loop

	return;
}

// ============================================================================
//
// FUNCTION    : CloseLogger
//
// DESCRIPTION : close a logger (release its resources)
//
// ARGUMENTS   : LoggerId
//
// RETURNS     : none
//
// ============================================================================
void CloseLogger
(
	LOGGER_ID LoggerId
)
{
	// close the given logger (if used)
	if(Loggers[LoggerId].Used == LOGGER_USED)
	{
		switch(Loggers[LoggerId].Destination)
		{
			case LOGGER_ANSI_FILENAME:
				// close the file
				fclose(Loggers[LoggerId].ANSIFilePtr);
				break;

#if	LOGGER_PLATFORM_IS_WIN32

			case LOGGER_WIN32_FILENAME:
				// close the file
				CloseHandle(Loggers[LoggerId].hWin32File);
				break;

			case LOGGER_WIN32_EVENTLOG:
				// deregister
				(void)DeregisterEventSource(Loggers[LoggerId].hEventSource);
				break;

#endif	// LOGGER_PLATFORM_IS_WIN32
		
			case LOGGER_SYBASE_SRVLOG:
				// unload the Sybase Open Server DLL

#if	LOGGER_PLATFORM_IS_WIN32
				FreeLibrary(Loggers[LoggerId].hDLL);
#else	// LOGGER_PLATFORM_IS_LINUX
				dlclose(Loggers[LoggerId].LibHandle);
#endif	// LOGGER_PLATFORM_IS_WIN32
				break;
		
#if	LOGGER_PLATFORM_IS_LINUX

		case LOGGER_UNIX_SYSLOG:

			// close syslog (not strictly necessary according to man (3)
			closelog();
			break;

#endif	// LOGGER_PLATFORM_IS_LINUX

			default:
				// no switch-off action
				break;
		}

	}

}

