/******************************************************************************
**
** FILE        : svc.c
**
** AUTHOR      : Nick Rozanski, Sybase (UK) Professional Services
**
** DESCRIPTION : Simple Windows NT command-line program to manage NT services
**
**               This program allows simple interactive management of Windows
*                NT services.  All services can be listed, and individual
**               services can be displayed, installed, modified or removed.
**
**               The program will prompt for all needed information from stdin.
**               For most prompts, typing ? will display a help message.
**
**               For information on Windows NT services and the function calls
**               in this program, refer to the Win32 API (obtainable on-line
**               from www.microsoft.com) or a good Windows NT reference (if such
**               a thing exists).
**
**               YOU ARE ADVISED TO BACK UP YOUR REGISTRY BEFORE RUNNING THIS
**               PROGRAM.
**
** Restrictions
** ------------
**
**               Because modifying services is a potentially destructive
**               activity, the program is deliberately restricted in what it
**               can do.  A service to be installed, modified or removed
**               must have the following characteristics:
**
**               1.  It must be a service on the local machine.
**               2.  It must be of type WIN32_OWN_PROCESS.
**               3.  It must be of start type 'automatic,' 'demand' or
**                   'disabled'.
**               4.  It must not have any service dependencies.
**
**               You cannot modify a service so that it breaks these criteria.
**
**               All types of service can be listed or displayed.
**
** SYNOPSIS    : svc [-d]
**
**               -d starts the program in debug mode, which prints some internal
**               debug messages.
**
**               The program prompts for all its information from stdin.  It asks
**               for confirmation before it makes any changes.
**
** BUGS
** ----
**
**               The program does not manage service groups.
**
**               The program does not confirm the existence of any supplied logon
**               or dependent service.
**
**               If a logon other than LocalSystem is supplied, the program
**               should at least check that it has the right to log on as a
**               Windows NT service (and if it doesn't, grant it).
**
**               Removing a service does not take effect until you exit the program
**               and no one else has a handle to it.
**
**
** MODIFICATION HISTORY
** --------------------
**
** Version   Date       Author          Comment
** -------   ----       ------          -------
**     1.0   30-June-98    Nick Rozanski   First Release Version
**
** Copyright (C) 1998 Nick Rozanski (Nick.Rozanski@sybase.com)
** Distributed under the terms of the GNU General Public License
**  as published by the Free Software Foundation
**  (675 Mass Ave, Cambridge, MA 02139, USA)
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
** License for more details.
**
******************************************************************************/

/******************************************************************************
**                                                                           **
** ANSI HEADER FILES                                                         **
**                                                                           **
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <conio.h>

/******************************************************************************
**                                                                           **
** WINDOWS NT HEADER FILES                                                   **
**                                                                           **
******************************************************************************/

#include <windows.h>
#include <winsvc.h>

/******************************************************************************
**                                                                           **
** LOCAL MACROS                                                              **
**                                                                           **
******************************************************************************/

#define	SI_VERSION		"1.0 (30 June 1998)"
#define	SI_COPYRIGHT	"Copyright (C) 1998 Nick Rozanski (Nick.Rozanski@sybase.com)"
#define	SI_DISTRIBUTION	"Distributed under the terms of the GNU General Public License\nas published by the Free Software Foundation"
#define	SI_WARRANTY		"Distributed WITHOUT ANY WARRANTY whatsoever."

#define	TRUE	1
#define	FALSE	0

#define	EOS			'\0'
#define	ASTERISK	'*'
#define	BSL			'\\'
#define	COMMENT		'#'
#define	RETURN		13
#define	NEWLINE		'\n'
#define	QUESTION	'?'
#define	QUOTE		'"'
#define	SPACE		' '


/* IMPORTANT MAXIMA - INCREASE IF NECESSARY */

/* BUFFER_SIZE is the size of all character buffers (names etc) */
#define	BUFFER_SIZE		5000
/*
** S1 is a "dependent service" of S2 if S1 must be running before S2 can start)
** MAXIMUM_DEPENDENCIES is the maximum number of dependent services
**  that any service can have and still be managed by this program
** For any given service S2, there can be up to MAXIMUM_DEPENDENCIES services
**  like S1 or the program won't handle it
*/
#define	MAXIMUM_DEPENDENCIES	50

/* readline flags */
#define	FLAG_NONE	0
#define	FLAG_CLEAR	1
#define	FLAG_LOWER	2
#define	FLAG_UPPER	4

/* miscellaneous macros */
#define	SERVICE_BATCHSIZE	10
#define	INSERT_LINEFEED		40

#define	DISPLAY_ACTION	'd'
#define	LIST_ACTION		'l'
#define	INSTALL_ACTION	'i'
#define	MODIFY_ACTION	'm'
#define	REMOVE_ACTION	'r'
#define	EXIT_ACTION		'x'

#define	LOCALSYSTEM	"LocalSystem"

/******************************************************************************
**                                                                           **
** GLOBAL TYPES                                                              **
**                                                                           **
******************************************************************************/
/* SVC_CONFIG holds all data for a service */
struct _SVC_CONFIG
{
	SC_HANDLE hService;
	char      ServiceName[BUFFER_SIZE];
	DWORD     dwServiceType;
	BOOL      bInteract;
	DWORD     dwStartType;
	DWORD     dwErrorControl;
	char      BinaryPathName[BUFFER_SIZE];
	char      DisplayName[BUFFER_SIZE];
	char      ServiceStartName[BUFFER_SIZE];
	char      Password[BUFFER_SIZE];
	int       nDependentServices;
	char      DependentServiceList[MAXIMUM_DEPENDENCIES][BUFFER_SIZE];
	int       nDependentServiceNameLength;
	BOOL      bServiceIsManageable;
};
typedef struct _SVC_CONFIG SVC_CONFIG, * LPSVC_CONFIG;

/******************************************************************************
**                                                                           **
** GLOBAL VARIABLES                                                          **
**                                                                           **
******************************************************************************/
/*
** configuration information for the current service
**  -- global because it is so large
*/
SVC_CONFIG GSvcConfig;

/* for debugging */
BOOL Gdebug = FALSE;
char Gmsgtxt[BUFFER_SIZE];


/******************************************************************************
**                                                                           **
** HELP TEXT FOR EACH USER RESPONSE                                          **
**                                                                           **
******************************************************************************/
#define	NOHELP_LINES 0
char* Gnohelp[] = { NULL };

const char* Gaction_help[] = {
		"Choose an action, one of",
		" l - list existing services;",
		" d - display a existing service;",
		" i - install a new service;",
		" m - modify an existing service;",
		" r - remove an existing service;",
		" x - exit." };
#define	ACTION_HELP_LINES	(sizeof(Gaction_help)/sizeof(char*))

const  char* Gbinary_path_name_help[] = {
		"Enter the binary path name for the service to execute.",
		"This must be the fully-qualified path name",
		" of an executable (.EXE) file.",
		"Batch (.BAT) files cannot be used.",
		"If the executable takes any command-line parameters,",
		" include these as part of the command." };
#define	BINARY_PATH_NAME_HELP_LINES	(sizeof(Gbinary_path_name_help)/sizeof(char*))

const char* Gdisplay_name_help[] = {
		"Enter the display name of the service",
		" (as displayed by Control Panel | Services,",
		"  net start, and other utilities.)" };
#define	DISPLAY_NAME_HELP_LINES	(sizeof(Gdisplay_name_help)/sizeof(char*))

const char* Gerror_control_help[] = {
		"Enter the error control for the service, one of",
		" i - ignore, ie startup/boot logs the error and continues;",
		" n - normal, ie startup/boot logs the error,",
		"    displays a message and continues;",
		" s - severe, ie startup/boot logs the error,",
		"    continues if startup is for Last Known Good Configuration,",
		"    otherwise reboots with Last Known Good Configuration;",
		" c - critical, ie startup/boot logs the error,",
		"    fails if startup is for Last Known Good Configuration,",
		"    otherwise reboots with Last Known Good Configuration." };
#define	ERROR_CONTROL_HELP_LINES	(sizeof(Gerror_control_help)/sizeof(char*))

const char* Ghas_dependencies_help[] = {
		"Service B has a 'dependency' on another service A",
		" if B cannot start until A is running.",
		"Choose y if this service has any dependencies -",
		" that is, if there are any services which must be running",
		" before this one can start." };
#define	HAS_DEPENDENCIES_HELP_LINES	(sizeof(Ghas_dependencies_help)/sizeof(char*))

const char* Ginteract_help[] = {
		"Choose 'y' if the service can interact with the desktop",
		" (display messages, open a console etc)." };
#define	INTERACT_HELP_LINES	(sizeof(Ginteract_help)/sizeof(char*))

const char* Gservice_name_help[] = {
		"Enter the short (internal) name of the service.",
		"This name is used to identify the service internally",
		" and is not usually displayed to users.",
		"It is the name of the registry key for the service in",
		" HKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Services.",
		"Type * for a list of existing services." };
#define	SERVICE_NAME_HELP_LINES	(sizeof(Gservice_name_help)/sizeof(char*))

const char* Gstart_localsystem_help[] = {
		"Choose 'y' if you want to start the service using",
		" the 'LocalSystem' account. This is the default and is",
		" the way most services start.  You do not have to",
		" provide a password with this option.",
		"The 'LocalSystem' account uses the System environment",
		" as specified in Control Panel | System | Environment.",
		"If you answer 'n', you will have to provide",
		" an alternative NT account and password.",
		"The service will then start under the environment",
		" of this account. (Remember that the System PATH",
		" is always prepended to an account PATH.)" };
#define	LOCALSYSTEM_HELP_LINES	(sizeof(Gstart_localsystem_help)/sizeof(char*))

const char* Gservice_start_name_help[] = {
		"Enter the name of the Windows NT account which will",
		" be used to start the service.",
		"To start using the 'LocalSystem' account, enter 'LocalSystem'",
		" (you do not have to enter a password for this account.)",
		"Otherwise enter an account name in the form 'domain\\user',",
		" or just '.\\user' for a user on this domain." };
#define	SERVICE_START_NAME_HELP_LINES	(sizeof(Gservice_start_name_help)/sizeof(char*))

const char* Gstart_type_help[] = {
		"Choose the start type of the service, one of",
		" a - automatic during system startup;",
		" d - demand, ie on user request via Control Panel | Services,",
		"     net start or similar utility;",
		" x - disabled, ie the service cannot be started.",
		"Note that the 'system' and 'boot' start types",
		" are not supported by this program." };
#define	START_TYPE_HELP_LINES	(sizeof(Gstart_type_help)/sizeof(char*))

const char* Guse_current_dependencies_help[] = {
		"Choose 'y' to keep the current list of dependencies.",
		" (Choose 'l' to display the current list and be prompted again.)",
		"If you choose 'n', you will have to enter the short name",
		" of each dependent service for this service." };
#define	USE_CURRENT_DEPENDENCIES_HELP_LINES	(sizeof(Guse_current_dependencies_help)/sizeof(char*))


/******************************************************************************
**                                                                           **
** DEBUG MACROS                                                              **
**                                                                           **
******************************************************************************/
#define	DEBUG(msg)	\
	if(Gdebug) {printf(" DEBUG in %s at %d: %s\n",__FILE__,__LINE__,msg);}
#define	DEBUG1(msg,p1)	\
	if(Gdebug) {sprintf_s(Gmsgtxt,msg,p1);printf(" DEBUG in %s at %d: %s\n",__FILE__,__LINE__,Gmsgtxt);}
#define	DEBUG2(msg,p1,p2)	\
	if(Gdebug) {sprintf_s(Gmsgtxt,msg,p1,p2);printf(" DEBUG in %s at %d: %s\n",__FILE__,__LINE__,Gmsgtxt);}

/******************************************************************************
**                                                                           **
** FUNCTION PROTOTYPES                                                       **
**                                                                           **
******************************************************************************/
/* service management functions */
BOOL check_service_is_manageable(QUERY_SERVICE_CONFIG ServiceConfig);
void display_service_config();
void get_service_config(char action);
void install_service(SC_HANDLE hServiceControlManager);
void list_services(SC_HANDLE hServiceControlManager);
void modify_service();
void read_service_config();
void read_service_name(char action, SC_HANDLE hServiceControlManager);
void remove_service();

/* utility functions */
BOOL util_confirm_action(char action, char ServiceName[]);
BOOL util_is_valid_path(char BinaryPathName[], char filename[]);
char* util_make_dependencies_array();
void util_print_error_and_exit(const char* message);
void util_readline(const char* prompt_ptr, const char* default_reply_ptr, const char* domain_ptr,
	const char* helptext_ptr[], short int helplines, short int linelength, char* line_ptr,
	int flags);

/******************************************************************************
**
** FUNCTION    : main
**
** DESCRIPTION : svc program entry point
**
** ARGUMENTS   : argc    number of command-line arguments
**               argv    command-line argument vector
**
** RETURNS     : n/a
**
******************************************************************************/
int main
(
	int   argc,
	char* argv[]
)
{
	char       action;
	char       reply[BUFFER_SIZE];
	SC_HANDLE  hServiceControlManager;

	/* the promotional bit */
	printf("\nSVC version %s\n\n", SI_VERSION);
	printf("%s\n", SI_COPYRIGHT);
	printf("%s\n", SI_DISTRIBUTION);
	printf("%s\n", SI_WARRANTY);

	/* has debug mode been requested on the command line? */
	if (argc > 1) { Gdebug = ((!strcmp(argv[1], "-d")) || (!strcmp(argv[1], "-D"))); }

	/* try and open a handle to the Service Control Manager Database */
	hServiceControlManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceControlManager == (SC_HANDLE)NULL)
	{
		util_print_error_and_exit("Unable to connect to Service Control Manager");
	}
	DEBUG1("opened handle %p to Service Control Manager", hServiceControlManager)

		/* enter the main program loop */
		memset(&GSvcConfig, 0, sizeof(GSvcConfig));

	while (TRUE)
	{
		/* prompt for the action - display, install, modify, remove, list or exit */
		printf("\n");
		util_readline("Action?", "l", "dimrlx", Gaction_help, ACTION_HELP_LINES,
			BUFFER_SIZE, reply, FLAG_LOWER);
		action = reply[0];

		/* exit? */
		if (action == EXIT_ACTION) { break; }

		/* list? */
		if (action == LIST_ACTION)
		{
			list_services(hServiceControlManager);
			continue;
		}

		/*
		** get the service name and open a handle to it
		**  (for install, the handle will just be set to NULL)
		*/
		while (TRUE)
		{
			/* prompt for the service name */
			read_service_name(action, hServiceControlManager);
			if (GSvcConfig.hService != NULL)
			{
				DEBUG2("opened handle %p to Service '%s'", GSvcConfig.hService, GSvcConfig.ServiceName)
			}

			/* for display, modify, remove, get the current service configuration */
			if (action != INSTALL_ACTION)
			{
				get_service_config(action);
				if (GSvcConfig.bServiceIsManageable)
				{
					/* we can work with this kind of service */
					break;
				}
				else
				{
					/* we can't work with this kind of service - try again */
					GSvcConfig.ServiceName[0] = EOS;
					continue;
				}
			}
			else
			{
				/* action is install */
				break;
			}
		}

		/* now, take the appropriate action on the service */
		switch (action)
		{
		case DISPLAY_ACTION:

			/* display the current configuration of the service */
			display_service_config();

			break;

		case INSTALL_ACTION:

			/* get the details of the new service */
			read_service_config();

			/* install the new service */
			install_service(hServiceControlManager);

			break;

		case MODIFY_ACTION:

			/* get the details of the service to be modified */
			read_service_config();

			/* modify the service */
			modify_service();

			break;

		case REMOVE_ACTION:

			/* remove the service */
			remove_service();

			break;

		default:; /* keep the compiler happy */

		}

		/* go back for next command */
	}

	/* exit with a SUCCESS status */
	ExitProcess(EXIT_SUCCESS);
	return EXIT_SUCCESS;
}

/******************************************************************************
**
** FUNCTION    : check_service_is_manageable
**
** DESCRIPTION : check whether the service configuration is simple enough
**               so that we can manage it, that is:
**
** ARGUMENTS   : ServiceConfig  service configuration
**
** RETURNS     : TRUE or FALSE
**
******************************************************************************/
BOOL check_service_is_manageable
(
	QUERY_SERVICE_CONFIG ServiceConfig
)
{
#define	ERRSTRING	"ERROR: this service %s.\n You cannot modify or remove it using this program.\n"

	/* the service type must be Win32 Own Process */
	if ((ServiceConfig.dwServiceType != SERVICE_WIN32_OWN_PROCESS) &&
		(ServiceConfig.dwServiceType != (SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS)))
	{
		printf(ERRSTRING, "is not a 'Win32 own process' service");
		return FALSE;
	}

	/* the start type must not be boot or system */
	if ((ServiceConfig.dwStartType == SERVICE_BOOT_START) ||
		(ServiceConfig.dwStartType == SERVICE_SYSTEM_START))
	{
		printf(ERRSTRING, "is a 'boot' or 'system' start service");
		return FALSE;
	}

	/* the service cannot be in a load order group */
	if (ServiceConfig.lpLoadOrderGroup != NULL)
		if ((*ServiceConfig.lpLoadOrderGroup) != EOS)
		{
			printf(ERRSTRING, "is a member of a load group");
			return FALSE;
		}

	/* the service cannot have dependencies */
	/*
	if(ServiceConfig.lpDependencies!=NULL)
	if((*ServiceConfig.lpDependencies)!=EOS)
	{
		printf(ERRSTRING,"is dependent on others for startup");
		return FALSE;
	}
	*/

	return TRUE;
}

/******************************************************************************
**
** FUNCTION    : display_service_config
**
** DESCRIPTION : display the current configuration of the selected service (as
**               retrieved from the SCM Database)
**
** ARGUMENTS   : none (uses global GSvcConfig)
**
** RETURNS     : n/a
**
******************************************************************************/
void display_service_config()
{
	int i;

	printf("\nCONFIGURATION FOR SERVICE '%s':\n", GSvcConfig.ServiceName);

	printf(" Display name: '%s'.\n", GSvcConfig.DisplayName);
	switch (GSvcConfig.dwServiceType & (~SERVICE_INTERACTIVE_PROCESS))
	{
	case SERVICE_WIN32_OWN_PROCESS:   printf(" Service type: Win32 own process.\n"); break;
	case SERVICE_WIN32_SHARE_PROCESS: printf(" Service type: Win32 shared process.\n"); break;
	case SERVICE_KERNEL_DRIVER:       printf(" Service type: device driver.\n"); break;
	case SERVICE_FILE_SYSTEM_DRIVER:  printf(" Service type: filesystem driver.\n"); break;
	default: printf(" Service type: unknown (%d).\n", GSvcConfig.dwServiceType); break;
	}
	printf("  (This service %s interact with the desktop.)\n",
		(GSvcConfig.dwServiceType & SERVICE_INTERACTIVE_PROCESS) != 0 ? "can" : "cannot");
	switch (GSvcConfig.dwStartType)
	{
	case SERVICE_BOOT_START:   printf(" Start type: boot.\n"); break;
	case SERVICE_SYSTEM_START: printf(" Start type: system.\n"); break;
	case SERVICE_AUTO_START:   printf(" Start type: auto.\n"); break;
	case SERVICE_DEMAND_START: printf(" Start type: demand.\n"); break;
	case SERVICE_DISABLED:     printf(" Start type: disabled.\n"); break;
	default: printf(" Start type: unknown (%d).\n", GSvcConfig.dwStartType); break;
	}
	switch (GSvcConfig.dwErrorControl)
	{
	case SERVICE_ERROR_IGNORE:   printf(" Error control: ignore.\n"); break;
	case SERVICE_ERROR_NORMAL:   printf(" Error control: normal.\n"); break;
	case SERVICE_ERROR_SEVERE:   printf(" Error control: severe."); break;
	case SERVICE_ERROR_CRITICAL: printf(" Error control: critical.\n"); break;
	default: printf(" Error control: unknown (%d).\n", GSvcConfig.dwErrorControl); break;
	}
	printf(" Binary path name: '%s'.\n", GSvcConfig.BinaryPathName);
	printf(" Service start name: '%s'.\n", GSvcConfig.ServiceStartName);

	/* display dependent service(s) */
	if (GSvcConfig.nDependentServices == 0)
	{
		printf(" Dependencies: none.\n");
	}
	else
	{
		printf(" Dependencies: %d\n", GSvcConfig.nDependentServices);
		for (i = 0; i < GSvcConfig.nDependentServices; i++)
		{
			if (GSvcConfig.DependentServiceList[i][0] == SC_GROUP_IDENTIFIER)
			{
				/* this is a service group */
				printf("  - %s (service group)\n", GSvcConfig.DependentServiceList[i] + 1);
			}
			else
			{
				/* this is a service */
				printf("  - %s\n", GSvcConfig.DependentServiceList[i]);
			}
		}
		DEBUG2("%d services, name length %d", GSvcConfig.nDependentServices,
			GSvcConfig.nDependentServiceNameLength)
	}
}

/******************************************************************************
**
** FUNCTION    : get_service_config
**
** DESCRIPTION : get the current service config from the SCM Database
**
** ARGUMENTS   : action    action to be performed
**
**               For a detailed explanation refer to the QUERY_SERVICE_CONFIG
**               topic in the Win32 API help.
**
** RETURNS     : TRUE if service is of a type we can work with, FALSE otherwise
**
******************************************************************************/
void get_service_config
(
	char action
)
{

	BOOL                  rc;
	QUERY_SERVICE_CONFIG  ServiceConfig;
	LPQUERY_SERVICE_CONFIG lpServiceConfig = NULL;
	DWORD                 BytesNeeded;
	char* lpSource, * lpDest;

	/* find out how big a buffer we need */
	rc = QueryServiceConfig(GSvcConfig.hService, NULL, 0, &BytesNeeded);
	if ((rc == 0) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
	{
		/* allocate the required space */
		lpServiceConfig = (LPQUERY_SERVICE_CONFIG)malloc(BytesNeeded);
		/* now make the real call */
		rc = QueryServiceConfig(GSvcConfig.hService, lpServiceConfig, BytesNeeded, &BytesNeeded);
		if (rc == 0)
		{
			util_print_error_and_exit("Unable to obtain service configuration");
		}
	}
	else
	{
		util_print_error_and_exit("Unable to obtain service configuration");
	}

	/* retrieve the service configuration information */
	memcpy(&ServiceConfig, lpServiceConfig, sizeof(ServiceConfig));

	/* copy the relevant bits of information for the service */
	GSvcConfig.dwServiceType = ServiceConfig.dwServiceType;
	GSvcConfig.bInteract = (GSvcConfig.dwServiceType & SERVICE_INTERACTIVE_PROCESS) != 0 ? TRUE : FALSE;
	GSvcConfig.dwStartType = ServiceConfig.dwStartType;
	GSvcConfig.dwErrorControl = ServiceConfig.dwErrorControl;
	strcpy_s(GSvcConfig.BinaryPathName, ServiceConfig.lpBinaryPathName);
	strcpy_s(GSvcConfig.DisplayName, ServiceConfig.lpDisplayName);
	strcpy_s(GSvcConfig.ServiceStartName, ServiceConfig.lpServiceStartName);

	/* get the dependent services (if any) */
	GSvcConfig.nDependentServices = 0;
	GSvcConfig.nDependentServiceNameLength = 0;
	if (ServiceConfig.lpDependencies != NULL)
	{
		/*
		** the dependent services pointer is not NULL
		** the names are stored as name1 EOS name2 EOS ... namen EOS EOS
		**  where EOS is the end-of-string character (0)
		*/
		lpSource = ServiceConfig.lpDependencies;
		/* copy the dependent service names, one at a time, into our array */
		while ((*lpSource) != EOS)
		{
			lpDest = &GSvcConfig.DependentServiceList[GSvcConfig.nDependentServices++][0];
			DEBUG1("copying service %d", GSvcConfig.nDependentServices)
				/* copy the service name */
				while ((*lpSource) != EOS)
				{
					(*lpDest++) = (*lpSource++);
					/* DependentServiceNameLength is the total length of all the service names */
					GSvcConfig.nDependentServiceNameLength++;
				}
			/* copy the EOS */
			(*lpDest++) = (*lpSource++);
			GSvcConfig.nDependentServiceNameLength++;
			DEBUG2("dependent service %d='%s'", GSvcConfig.nDependentServices,
				GSvcConfig.DependentServiceList[GSvcConfig.nDependentServices - 1])
		}
	}

	/* see if this service is simple enough so that we can deal with it */
	if (action != DISPLAY_ACTION)
	{
		GSvcConfig.bServiceIsManageable = check_service_is_manageable(ServiceConfig);
	}
	else
	{
		GSvcConfig.bServiceIsManageable = TRUE;
	}

	/* free the allocated storage */
	free(lpServiceConfig);

	DEBUG1("service type is %d", GSvcConfig.dwStartType)
		DEBUG1("start type is %d", GSvcConfig.dwStartType)
		DEBUG1("error control is %d", GSvcConfig.dwErrorControl)
		DEBUG1("path name is '%s'", GSvcConfig.BinaryPathName)
		DEBUG1("display name is '%s'", GSvcConfig.DisplayName)
		DEBUG1("start name is '%s'", GSvcConfig.ServiceStartName)

}

/******************************************************************************
**
** FUNCTION    : install_service
**
** DESCRIPTION : install the service into the SCM Database (confirm first)
**
** ARGUMENTS   : hServiceControlManager   handle to SCM Database
**
** RETURNS     : n/a
**
******************************************************************************/
void install_service
(
	SC_HANDLE    hServiceControlManager
)
{

	char  msgtxt[BUFFER_SIZE];
	char* lpDependencies;

	/* confirm the user really wants to install */
	if (!util_confirm_action(INSTALL_ACTION, GSvcConfig.ServiceName))
	{
		printf("Service '%s' was not installed.\n", GSvcConfig.ServiceName);
		return;
	}

	/* make the dependencies array (in the form name1 EOS name2 EOS ... namen EOS EOS) */
	lpDependencies = util_make_dependencies_array();

	/* service type */
	GSvcConfig.dwServiceType = GSvcConfig.bInteract ?
		SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS :
		SERVICE_WIN32_OWN_PROCESS;

	/* install the service */
	printf("Installing service '%s' ...", GSvcConfig.ServiceName); fflush(stdout);
	if (strcmp(GSvcConfig.ServiceStartName, LOCALSYSTEM))
	{
		/* the service will start under a named account */
		GSvcConfig.hService = CreateService(
			hServiceControlManager,
			GSvcConfig.ServiceName,
			GSvcConfig.DisplayName,
			SERVICE_ALL_ACCESS,
			GSvcConfig.dwServiceType,
			GSvcConfig.dwStartType,
			GSvcConfig.dwErrorControl,
			GSvcConfig.BinaryPathName,
			NULL,	/* LoadOrderGroup */
			NULL,	/* Tag Id */
			lpDependencies,
			GSvcConfig.ServiceStartName,
			GSvcConfig.Password
		);
	}
	else
	{
		/*
		** the service will start under the LocalSystem account
		**  start name has to be NULL in this case
		*/
		GSvcConfig.hService = CreateService(
			hServiceControlManager,
			GSvcConfig.ServiceName,
			GSvcConfig.DisplayName,
			SERVICE_ALL_ACCESS,
			GSvcConfig.dwServiceType,
			GSvcConfig.dwStartType,
			GSvcConfig.dwErrorControl,
			GSvcConfig.BinaryPathName,
			NULL,	/* LoadOrderGroup */
			NULL,	/* Tag Id */
			lpDependencies,
			NULL,	/* ServiceStartName */
			NULL	/* Password */
		);
	}
	printf(" Complete.\n");

	/* free the dependencies array storage */
	if (lpDependencies != NULL) { free(lpDependencies); }

	/* check the return status */
	if (GSvcConfig.hService == NULL)
	{
		sprintf_s(msgtxt, "Service '%s' was not installed", GSvcConfig.ServiceName);
		util_print_error_and_exit(msgtxt);
	}
	else
	{
		printf("Service '%s' was installed successfully.\n", GSvcConfig.ServiceName);
	}

}

/******************************************************************************
**
** FUNCTION    : list_services
**
** DESCRIPTION : list all services in the SCM Database
**
** ARGUMENTS   : hServiceControlManager
**
** RETURNS     : n/a
**
******************************************************************************/
void list_services
(
	SC_HANDLE hServiceControlManager
)
{
	BOOL                rc;
	ENUM_SERVICE_STATUS ServiceList[SERVICE_BATCHSIZE];
	DWORD               i;
	DWORD               BytesNeeded;
	DWORD               ServicesReturned;
	DWORD               Resume = 0;

	printf("\nLIST OF INSTALLED SERVICES:\n");

	while (TRUE)
	{
		/* get a "batch" of service entries */
		rc = EnumServicesStatus(hServiceControlManager, SERVICE_WIN32,
			SERVICE_ACTIVE | SERVICE_INACTIVE,
			ServiceList, sizeof(ServiceList), &BytesNeeded,
			&ServicesReturned, &Resume);
		if (rc == 0)
		{
			if (GetLastError() != ERROR_MORE_DATA)
			{
				/* call to EnumServicesStatus failed */
				util_print_error_and_exit("Failed to get batch of service entries");
			}
		}

		/* print the service information for this batch */
		for (i = 0; i < ServicesReturned; i++)
		{
			printf("%s: %s\n", ServiceList[i].lpServiceName, ServiceList[i].lpDisplayName);
		}

		/* any more service batches? */
		if (Resume == 0) { break; }
	}
}

/******************************************************************************
**
** FUNCTION    : modify_service
**
** DESCRIPTION : modify the service in the SCM Database (confirm first)
**
** ARGUMENTS   : none (uses global GSvcConfig)
**
** RETURNS     : n/a
**
******************************************************************************/
void modify_service()
{
	BOOL  rc;
	char  msgtxt[BUFFER_SIZE];
	char* lpDependencies;

	/* confirm the user really wants to modify */
	if (!util_confirm_action(MODIFY_ACTION, GSvcConfig.ServiceName))
	{
		printf("Service '%s' was not modified.\n", GSvcConfig.ServiceName);
		return;
	}

	/* make the dependencies array (in the form name1 EOS name2 EOS ... namen EOS EOS) */
	lpDependencies = util_make_dependencies_array();

	/* service type */
	GSvcConfig.dwServiceType = GSvcConfig.bInteract ?
		SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS :
		SERVICE_WIN32_OWN_PROCESS;

	/* modify the service */
	printf("Modifying service '%s' ...", GSvcConfig.ServiceName); fflush(stdout);
	rc = ChangeServiceConfig(GSvcConfig.hService,
		GSvcConfig.dwServiceType,
		GSvcConfig.dwStartType,
		GSvcConfig.dwErrorControl,
		GSvcConfig.BinaryPathName,
		NULL,	/* LoadOrderGroup */
		NULL,	/* Tag Id */
		lpDependencies,
		GSvcConfig.ServiceStartName,
		GSvcConfig.Password,
		GSvcConfig.DisplayName
	);
	printf(" Complete.\n");

	/* free the dependencies array storage */
	if (lpDependencies != NULL) { free(lpDependencies); }

	/* check the return status */
	if (rc == 0)
	{
		sprintf_s(msgtxt, "Service '%s' was not modified", GSvcConfig.ServiceName);
		util_print_error_and_exit(msgtxt);
	}
	else
	{
		printf("Service '%s' was modified successfully.\n", GSvcConfig.ServiceName);
	}

}

/******************************************************************************
**
** FUNCTION    : read_service_config
**
** DESCRIPTION : prompt for and desired service details, read them from stdin
**               and validate them
**
** ARGUMENTS   : none (uses global GSvcConfig)
**
** RETURNS     : n/a
**
******************************************************************************/
void read_service_config()
{
	char reply[BUFFER_SIZE];
	char default_reply[2];
	char msgtxt[BUFFER_SIZE];
	char filename[BUFFER_SIZE];
	char password[BUFFER_SIZE];
	int  i;

	printf("\nENTER DETAILS FOR SERVICE %s:\n\n", GSvcConfig.ServiceName);

	/* get display name */
	util_readline("Enter display name", GSvcConfig.DisplayName, "", Gdisplay_name_help,
		DISPLAY_NAME_HELP_LINES, BUFFER_SIZE, reply, FLAG_NONE);
	strcpy_s(GSvcConfig.DisplayName, reply);
	DEBUG1("display name is '%s'\n", GSvcConfig.DisplayName)

		/* can the service interact with the desktop? */
		default_reply[0] = GSvcConfig.bInteract ? 'y' : 'n';
	default_reply[1] = EOS;
	printf("\n");
	util_readline("Can the service interact with the desktop?", default_reply, "yn", Ginteract_help,
		INTERACT_HELP_LINES, BUFFER_SIZE, reply, FLAG_LOWER);
	GSvcConfig.bInteract = reply[0] == 'y' ? TRUE : FALSE;
	DEBUG1("interact flag is %d", GSvcConfig.bInteract)

		/* set default start type */
		switch (GSvcConfig.dwStartType)
		{
		case SERVICE_AUTO_START:   default_reply[0] = 'a'; break;
		case SERVICE_DEMAND_START: default_reply[0] = 'd'; break;
		case SERVICE_DISABLED:     default_reply[0] = 'x'; break;
		default:                   default_reply[0] = 'd';
		}
	default_reply[1] = EOS;

	/* get start type */
	printf("\n");
	util_readline("Enter start type", default_reply, "adx", Gstart_type_help,
		START_TYPE_HELP_LINES, BUFFER_SIZE, reply, FLAG_LOWER);
	switch (reply[0])
	{
	case 'a':
		GSvcConfig.dwStartType = SERVICE_AUTO_START;
		DEBUG(" start type is 'auto'")
			break;
	case 'd':
		GSvcConfig.dwStartType = SERVICE_DEMAND_START;
		DEBUG(" start type is 'demand'")
			break;
	case 'x':
		GSvcConfig.dwStartType = SERVICE_DISABLED;
		DEBUG(" start type is 'disabled'")
			break;
	default:; /* keep the compiler happy */
	}

	/* set default error control */
	switch (GSvcConfig.dwErrorControl)
	{
	case SERVICE_ERROR_IGNORE:   default_reply[0] = 'i'; break;
	case SERVICE_ERROR_NORMAL:   default_reply[0] = 'n'; break;
	case SERVICE_ERROR_SEVERE:   default_reply[0] = 's'; break;
	case SERVICE_ERROR_CRITICAL: default_reply[0] = 'c'; break;
	default:                     default_reply[0] = 'i';
	}
	default_reply[1] = EOS;

	/* get error control */
	printf("\n");
	util_readline("Enter error control", default_reply, "insc", Gerror_control_help,
		ERROR_CONTROL_HELP_LINES, BUFFER_SIZE, reply, FLAG_LOWER);
	switch (reply[0])
	{
	case 'i':
		GSvcConfig.dwErrorControl = SERVICE_ERROR_IGNORE;
		DEBUG(" error control is 'ignore'")
			break;
	case 'n':
		GSvcConfig.dwErrorControl = SERVICE_ERROR_NORMAL;
		DEBUG(" error control is 'normal'")
			break;
	case 's':
		GSvcConfig.dwErrorControl = SERVICE_ERROR_SEVERE;
		DEBUG(" error control is 'severe'")
			break;
	case 'c':
		GSvcConfig.dwErrorControl = SERVICE_ERROR_CRITICAL;
		DEBUG(" error control is 'critical'")
			break;
	default:; /* keep the compiler happy */
	}

	printf("\n");
	while (TRUE)
	{
		/* get binary path name */
		util_readline("Enter binary path name", GSvcConfig.BinaryPathName, "", Gbinary_path_name_help,
			BINARY_PATH_NAME_HELP_LINES, BUFFER_SIZE, reply, FLAG_NONE);
		strcpy_s(GSvcConfig.BinaryPathName, reply);
		if (!util_is_valid_path(GSvcConfig.BinaryPathName, filename))
		{
			sprintf_s(msgtxt, "WARNING: file '%s' does not seem to exist, or is not executable. OK?",
				filename);
			util_readline(msgtxt, "n", "yn", NULL, 0, BUFFER_SIZE, reply, FLAG_LOWER);
			if (reply[0] == 'n') { continue; }
		}
		break;
	}
	DEBUG1("binary path name is '%s'", GSvcConfig.BinaryPathName)

		/* is the service currently using LocalSystem? */
		if (GSvcConfig.ServiceStartName[0] == EOS) { strcpy_s(GSvcConfig.ServiceStartName, LOCALSYSTEM); }
	default_reply[0] = (!strcmp(GSvcConfig.ServiceStartName, LOCALSYSTEM) ? 'y' : 'n');
	default_reply[1] = EOS;

	/* get service start name */
	printf("\n");
	util_readline("Start service using 'LocalSystem' account?", default_reply, "yn", Gstart_localsystem_help,
		LOCALSYSTEM_HELP_LINES, BUFFER_SIZE, reply, FLAG_LOWER);
	if (reply[0] == 'y')
	{
		strcpy_s(GSvcConfig.ServiceStartName, LOCALSYSTEM);
		printf(" (User '%s' does not require a password).\n", LOCALSYSTEM);
		GSvcConfig.Password[0] = EOS;
	}
	else
	{
		printf("\n");
		while (TRUE)
		{
			util_readline("Enter service start name", GSvcConfig.ServiceStartName, "", Gservice_start_name_help,
				SERVICE_START_NAME_HELP_LINES, BUFFER_SIZE, reply, FLAG_NONE);
			if ((strcmp(reply, LOCALSYSTEM)) && (strchr(reply, BSL) == NULL))
			{
				printf("ERROR: name '%s' is not in the form 'domain\\account' or '.\\account'.\n",
					reply);
				continue;
			}
			break;
		}
		strcpy_s(GSvcConfig.ServiceStartName, reply);
		/* get password - NB none is needed for LocalSystem */
		if (!strcmp(GSvcConfig.ServiceStartName, LOCALSYSTEM))
		{
			printf(" (User '%s' does not require a password).\n", LOCALSYSTEM);
			GSvcConfig.Password[0] = EOS;
		}
		else
		{
			/* informational message */
			printf("WARNING: the user '%s' must have the user right\n", GSvcConfig.ServiceStartName);
			printf(" to'logon as a service'. To manage user rights,\n");
			printf(" start User Manager, select Policies | User Rights\n");
			printf(" and click the 'Show Advanced User Rights' checkbox.\n");
			/* get password */
			while (TRUE)
			{
				printf("\n");
				sprintf_s(msgtxt, "Enter service start password for user '%s'",
					GSvcConfig.ServiceStartName);
				util_readline(msgtxt, "", "", NULL, 0, BUFFER_SIZE, reply, FLAG_CLEAR);
				strcpy_s(password, reply);
				util_readline("Re-enter password to confirm", "", "", NULL, 0, BUFFER_SIZE, reply, FLAG_CLEAR);
				if (!strcmp(password, reply)) { break; }
				else { printf("ERROR: non-matching passwords entered: try again.\n"); }
			}
			strcpy_s(GSvcConfig.Password, password);
		}
	}
	DEBUG1("service start name is '%s'", GSvcConfig.ServiceStartName)
		DEBUG1("service start password is '%s'", GSvcConfig.Password)

		/* does the service currently have dependencies? */
		default_reply[0] = GSvcConfig.nDependentServices == 0 ? 'n' : 'y';
	default_reply[1] = EOS;

	/* get service dependencies */
	printf("\n");
	util_readline("Does this service have dependencies?", default_reply, "yn", Ghas_dependencies_help,
		HAS_DEPENDENCIES_HELP_LINES, BUFFER_SIZE, reply, FLAG_LOWER);
	if (reply[0] == 'n')
	{
		/* the service does not have any dependents */
		GSvcConfig.nDependentServices = 0;
		GSvcConfig.nDependentServiceNameLength = 0;
	}
	else
	{
		reply[0] = EOS;
		if (GSvcConfig.nDependentServices != 0)
		{
			while (TRUE)
			{
				sprintf_s(msgtxt, "Use the current list of %d dependencies (l for list)?",
					GSvcConfig.nDependentServices);
				util_readline(msgtxt, "y", "ynl", Guse_current_dependencies_help,
					USE_CURRENT_DEPENDENCIES_HELP_LINES, BUFFER_SIZE, reply, FLAG_LOWER);
				if (reply[0] == 'l')
				{
					for (i = 0; i < GSvcConfig.nDependentServices; i++)
					{
						printf(" %d: %s\n", i, GSvcConfig.DependentServiceList[i]);
					}
				}
				else { break; }
			}
		}
		if ((reply[0] == 'n') || (reply[0] == EOS))
		{
			/* prompt the user for the modified list of dependencies */
			printf("\nEnter the short name of each dependent service, one per line.\n");
			printf("Enter an entirely blank line on its own to finish.\n");
			printf("If you want to enter the name of a group,\n");
			printf(" precede it with the character '%c' (no quotes)\n", SC_GROUP_IDENTIFIER);
			printf("Note: service names are not validated at this point.\n\n");

			/* initialise the list of dependencies */
			GSvcConfig.nDependentServices = 0;
			GSvcConfig.nDependentServiceNameLength = 0;

			while (TRUE)
			{
				/* read the dependent service name */
				sprintf_s(msgtxt, "Short name of dependent service %d (blank to finish)",
					GSvcConfig.nDependentServices + 1);
				util_readline(msgtxt, "", "", NULL, 0, BUFFER_SIZE, reply, FLAG_NONE);

				if (reply[0] == EOS)
				{
					break;
				}
				else
				{
					/* copy this dependent service name into the array */
					strcpy_s(GSvcConfig.DependentServiceList[GSvcConfig.nDependentServices],
						reply);
					DEBUG2("dependent service %d: '%s'", GSvcConfig.nDependentServices,
						GSvcConfig.DependentServiceList[GSvcConfig.nDependentServices])

						/* increment the count and total length */
						GSvcConfig.nDependentServices++;
					GSvcConfig.nDependentServiceNameLength += static_cast<int>(strlen(reply) + 1);
				}
			}
			printf("You entered %d dependent services.\n", GSvcConfig.nDependentServices);
			DEBUG2("%d services, name length %d", GSvcConfig.nDependentServices,
				GSvcConfig.nDependentServiceNameLength)
		}
	}

}

/******************************************************************************
**
** FUNCTION    : read_service_name
**
** DESCRIPTION : prompt the user for the service name, and validate it
**
** ARGUMENTS   : action                     install / modify / remove
**               hServiceControlManager     handle to SCM Database
**
** RETURNS     : n/a
**
******************************************************************************/
void read_service_name
(
	char         action,
	SC_HANDLE    hServiceControlManager
)
{
	char      reply[BUFFER_SIZE];
	char      CurrentServiceName[BUFFER_SIZE];

	/* initialise the SvcConfig structure */
	strcpy_s(CurrentServiceName, GSvcConfig.ServiceName);
	memset(&GSvcConfig, 0, sizeof(GSvcConfig));

	/* prompt for the service name */
	while (TRUE)
	{
		util_readline("Enter the service name", CurrentServiceName, "", Gservice_name_help,
			SERVICE_NAME_HELP_LINES, BUFFER_SIZE, reply, FLAG_NONE);
		if (reply[0] == ASTERISK)
		{
			list_services(hServiceControlManager);
		}
		else
		{
			/* copy the service name */
			strcpy_s(GSvcConfig.ServiceName, reply);

			/* try and obtain a handle to the service */
			GSvcConfig.hService = OpenService(hServiceControlManager,
				GSvcConfig.ServiceName,
				SERVICE_ALL_ACCESS);

			/* did we obtain a handle? */
			if ((GSvcConfig.hService) == NULL)
			{
				if (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST)
				{
					util_print_error_and_exit((const char*)"Unable to obtain service handle");
				}
				/* the service does not exist */
				if (action == INSTALL_ACTION)
				{
					/*
					** we are trying to install a service,
					**  and it does not exist - this is OK
					*/
					break;
				}
				else
				{
					/*
					** we are trying to display, modify or remove a service,
					**  and it does not exist - this is an error
					*/
					printf("ERROR: service '%s' does not exist. You cannot display, modify or remove it.\n",
						GSvcConfig.ServiceName);
				}
			}
			else
			{
				/* the service does exist */
				if (action != INSTALL_ACTION)
				{
					/* we are trying to modify or remove a service, and it exists - this is OK */
					break;
				}
				else
				{
					/* we are trying to install a service, and it already exists - this is an error */
					printf("ERROR: service '%s' already exists. You cannot install it.\n",
						GSvcConfig.ServiceName);
				}
			}
		}
	}

}

/******************************************************************************
**
** FUNCTION    : remove_service
**
** DESCRIPTION : remove the service from the SCM Database (confirm first)
**
** ARGUMENTS   : none (uses global GSvcConfig)
**
** RETURNS     : n/a
**
******************************************************************************/
void remove_service()
{
	BOOL rc;
	char msgtxt[BUFFER_SIZE];

	/* confirm the user really wants to remove */
	if (!util_confirm_action(REMOVE_ACTION, GSvcConfig.ServiceName))
	{
		printf("Service '%s' was not removed\n.", GSvcConfig.ServiceName);
		return;
	}

	/* remove the service */
	printf("Deleting service '%s' ...", GSvcConfig.ServiceName); fflush(stdout);
	rc = DeleteService(GSvcConfig.hService);
	printf(" Complete.\n");

	/* check the return status */
	if (rc == 0)
	{
		sprintf_s(msgtxt, "Service '%s' was not removed", GSvcConfig.ServiceName);
		util_print_error_and_exit(msgtxt);
	}
	else
	{
		printf("Service '%s' was successfully marked for deletion.\n", GSvcConfig.ServiceName);
		printf("It will not be removed until you exit this program.\n");
	}

}


/******************************************************************************
**
** FUNCTION    : util_confirm_action
**
** DESCRIPTION : confirm that the user really wants to perform the selected action
**
** ARGUMENTS   : action
**               ServiceName
**
** RETURNS     : TRUE if action confirmed, else FALSE
**
******************************************************************************/
BOOL util_confirm_action
(
	char  action,
	char  ServiceName[]
)
{
#define	AREYOUSURE	"Are you sure you want to %s service '%s'?"

	char reply[BUFFER_SIZE];
	char msgtxt[BUFFER_SIZE];

	printf("\n *** WARNING: modifying service configuration can make Windows NT unuseable.\n");
	printf(" *** You should back up the Windows NT Registry before running this command.\n\n");
	switch (action)
	{
	case INSTALL_ACTION: sprintf_s(msgtxt, AREYOUSURE, "install", ServiceName); break;
	case MODIFY_ACTION:  sprintf_s(msgtxt, AREYOUSURE, "modify", ServiceName); break;
	case REMOVE_ACTION:  sprintf_s(msgtxt, AREYOUSURE, "remove", ServiceName); break;
	default:; /* keep the compiler happy */
	}
	util_readline(msgtxt, "n", "yn", NULL, 0, BUFFER_SIZE, reply, FLAG_LOWER);

	return reply[0] == 'n' ? FALSE : TRUE;

}

/******************************************************************************
**
** FUNCTION    : util_is_valid_path
**
** DESCRIPTION : given a supplied command line, extract the file name from it,
**               and see if it exists and is executable
**
** ARGUMENTS   : BinaryPathName     given command line
**               filename (OUTPUT)  extracted filename
**
** RETURNS     : TRUE if valid, FALSE otherwise
**
******************************************************************************/
BOOL util_is_valid_path
(
	char BinaryPathName[],
	char filename[]
)
{
	char* path_ptr = BinaryPathName;
	char* file_ptr = filename;
	short int  quote_count = 0;
	DWORD      dwBinaryType;

	while ((*path_ptr) != EOS)
	{
		if (((*path_ptr) == SPACE) && (quote_count == 0))
		{
			/* we have got to the end of the file name */
			break;
		}
		if ((*path_ptr) == QUOTE) { quote_count = ~quote_count; }
		(*file_ptr++) = (*path_ptr++);
	}

	/* add an end-of-string marker */
	(*file_ptr) = EOS;
	DEBUG1("file is '%s'", filename)

		/* is the file executable? */
		return GetBinaryType(filename, &dwBinaryType);

}

/******************************************************************************
**
** FUNCTION    : util_make_dependencies_array
**
** DESCRIPTION : take the array of dependent service names, and string it together
**               in a format suitable for the Win32 API, namely a single character
**               array in the form name1 EOS name2 EOS ... namen EOS EOS.
**
** ARGUMENTS   : none (uses global GSvcConfig)
**
** RETURNS     : pointer to allocated storage
**
******************************************************************************/
char* util_make_dependencies_array()
{
	char* lpDependencies;
	char* lpSource, * lpDest;
	int   i;

	/* allocate the storage required for the array */
	lpDependencies = (char*)malloc(GSvcConfig.nDependentServiceNameLength++);

	/* copy the names (if any) one at a time */
	lpDest = lpDependencies;
	for (i = 0; i < GSvcConfig.nDependentServices; i++)
	{
		DEBUG2("copying name %d '%s'", i, GSvcConfig.DependentServiceList[i])
			lpSource = GSvcConfig.DependentServiceList[i];
		/* copy the name */
		while (*lpSource != EOS) { (*lpDest++) = (*lpSource++); }
		/* copy the EOS */
		(*lpDest++) = (*lpSource++);
	}

	/* insert the final EOS */
	lpDest[GSvcConfig.nDependentServiceNameLength] = EOS;

	/* check it worked ... */
	DEBUG2("length %d inserted %d", GSvcConfig.nDependentServiceNameLength,
		(int)(lpDest - lpDependencies))

		return lpDependencies;
}

/******************************************************************************
**
** FUNCTION    : util_print_error_and_exit
**
** DESCRIPTION : call GetLastError to print error information, and then exit
**               the process
**
** ARGUMENTS   : message
**
** RETURNS     : n/a
**
******************************************************************************/
void util_print_error_and_exit
(
	const char* message
)
{
	LPVOID lpMsgBuf;
	DWORD LastError;

	/* get the last error code */
	LastError = GetLastError();
	printf("ERROR %d - %s\n", LastError, message);

	/* format the corresponding error message */
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, LastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);

	printf("Error message is: '%s'\n", (char*)lpMsgBuf);
	switch (LastError)
	{
	case ERROR_INVALID_PARAMETER:
		printf("Suggestion: check the validity of all parameters\n");
		printf(" that you supplied, particularly service names and pathnames.\n");
		break;

	case ERROR_INVALID_SERVICE_ACCOUNT:
		printf("Suggestion: check that the login name you supplied exists.\n");
		break;

	case ERROR_SERVICE_MARKED_FOR_DELETE:
		printf("Suggestion: you cannot modify a service which has been marked for deletion.\n");
		break;

	case ERROR_ACCESS_DENIED:
		printf("Suggestion: you do not have permission to carry out these tasks.\n");
		break;

	default: break;
	}

	printf("You can run this program in debug mode using the '-d' flag.\n");

	/* free the buffer allocated by FormatMessage */
	LocalFree(lpMsgBuf);

	/* exit the process */
	ExitProcess(EXIT_FAILURE);
}

/******************************************************************************
**
** FUNCTION    : util_readline
**
** DESCRIPTION : read a line of input from stdin, ignoring lines beginning with
**               a comment character #
**
** ARGUMENTS   : prompt_ptr         prompt text (displayed first)
**               default_reply_ptr  default reply ("" for none)
**               domain_ptr         string which lists all valid single-character
**                                   reponses ("" for no checking)
**               helptext_ptr       pointer to character array of help text (NULL
**                                   for no help)
**               helplines          number of elements in help text array
**               linelength         maximum characters to read in reply
**               line_ptr (OUTPUT)  buffer where reply will be stored (null-
**                                   terminated)
**               flags              I/O control, one or more of the following bit values:
**                                   FLAG_NONE	no special control
**                                   FLAG_CLEAR	entered text is not displayed
**                                   FLAG_LOWER	entered text is converted to lower-case
**                                   FLAG_UPPER	entered text is converted to upper-case
**
** RETURNS     : n/a
**
******************************************************************************/
void util_readline
(
	const char* prompt_ptr,
	const char* default_reply_ptr,
	const char* domain_ptr,
	const char* helptext_ptr[],
	short int  helplines,
	short int  linelength,
	char* line_ptr,
	int        flags
)
{
	char* buffer_ptr = (char*)malloc(linelength * sizeof(char));
	char* get_ptr;
	int   get_char, char_count;
	char* in_ptr, * out_ptr;
	int   i;
	BOOL  last_char;

	while (TRUE)
	{
		/* initialise pointers */
		in_ptr = buffer_ptr;
		out_ptr = line_ptr;

		/* display the prompt */
		if (helplines > 0)
		{
			if (default_reply_ptr[0] == EOS) { printf("%s (? for help): ", prompt_ptr); }
			else { printf("%s (? for help) [%s]: ", prompt_ptr, default_reply_ptr); }
		}
		else
		{
			if (default_reply_ptr[0] == EOS) { printf("%s: ", prompt_ptr); }
			else { printf("%s [%s]: ", prompt_ptr, default_reply_ptr); }
		}

		/* print a newline if the default is very long */
		if (strlen(default_reply_ptr) > INSERT_LINEFEED) { printf("\n"); }

		/* read the reply */
		/* fgets(buffer_ptr,linelength,stdin); */
		fflush(stdout);
		get_ptr = buffer_ptr;
		char_count = 0;
		while (TRUE)
		{
			if ((flags & FLAG_CLEAR) != 0)
			{
				/* get hidden text */
				get_char = _getch();
				last_char = (get_char == RETURN);
				if (last_char) { printf("\n"); fflush(stdout); }
			}
			else
			{
				/* get plain text */
				get_char = getc(stdin);
				last_char = (get_char == NEWLINE);
			}
			if (last_char) { (*get_ptr) = EOS; break; }
			if ((flags & FLAG_LOWER) != 0) { get_char = tolower(get_char); }
			if ((flags & FLAG_UPPER) != 0) { get_char = toupper(get_char); }
			(*get_ptr++) = (char)get_char;
			if ((char_count++) >= linelength - 1) { (*get_ptr) = EOS; break; }
		}

		/* is the first character a comment character? */
		while ((*in_ptr) == SPACE) { in_ptr++; }
		if ((*in_ptr) == COMMENT) { continue; }

		/* is the first character a question mark? */
		if ((*in_ptr) == QUESTION)
		{
			/* display help, if any */
			if (helplines == 0)
			{
				printf("No help is available for this prompt.\n");
			}
			else
			{
				for (i = 0; i < helplines; i++) { printf(i == 0 ? " HELP: %s\n" : " %s\n", helptext_ptr[i]); }
			}
			printf("\n"); continue;
		}

		/* copy the rest of the line into the output */
		while ((*in_ptr) != EOS)
		{
			if ((*in_ptr) != NEWLINE)
			{
				/* do not copy the newline character (if any) */
				(*out_ptr++) = (*in_ptr);
			}
			in_ptr++;
		}
		/* append a string termination character */
		(*out_ptr) = EOS;

		/* is the string empty? */
		if ((*line_ptr) == EOS)
		{
			/* copy the default value into the reply */
			strcpy_s(line_ptr, strlen(default_reply_ptr), default_reply_ptr);
		}

		/* check to make sure the reply is within the domain, if supplied */
		if ((*domain_ptr) != EOS)
		{
			if ((strchr(domain_ptr, (*line_ptr)) == NULL) || (strlen(line_ptr) != 1))
			{
				printf("Try again - reply must be one of [%s]\n", domain_ptr);
				continue;
			}
			else
			{
				/* reply is within domain */
				break;
			}
		}
		else
		{
			/* no domain supplied - exit the while loop */
			break;
		}
	}

	/* free the allocated buffer */
	free(buffer_ptr);
}