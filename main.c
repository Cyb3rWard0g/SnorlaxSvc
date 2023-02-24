/*
* Code Reference:
* - Code from the Dark Vortex Offensive Tool Development (OTD) workshop taught by Chetan Nayak (@ninjaparanoid)
* - Added a few comments to understand the code and a simple while condition :) 
*/
#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#define SERVICE_NAME TEXT("SnorlaxSvc")

// Global definitions
SERVICE_STATUS gSvcStatus;
SERVICE_STATUS_HANDLE gSvcStatusHandle;
HANDLE ghSvcStopEvent = NULL;

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode) {
		case SERVICE_CONTROL_STOP:

			if (gSvcStatus.dwCurrentState != SERVICE_RUNNING) {
				break;
			}

			gSvcStatus.dwControlsAccepted = gSvcStatus.dwWin32ExitCode = 0;
			gSvcStatus.dwCurrentState = SERVICE_STOP_PENDING;
			gSvcStatus.dwCheckPoint = 4;

			if (SetServiceStatus(gSvcStatusHandle, &gSvcStatus) == FALSE) {
				return;
			}

			// This will signal the worker thread to start shutting down
			SetEvent(ghSvcStopEvent);
			break;

		default:
			break;
	}
}

/*

Notes
-----
- The ServiceMain function (SvcMain in our case) has access to the command-line arguments for the service in the way that the main function of a console application does.
- The arguments are passed from the initial StartService call to the SCM which launched the process in the first place.
- The first argument passed is always the name of the service name. Therefore, there will always be at least one argument.
*/
VOID WINAPI SvcMain(
	DWORD argc, // The number of arguments being passed to the service in the second parameter
	LPTSTR* argv // Pointer to an array of string pointers -> Arguments
)
{
	/*
	Fills a block of memory with zeros.
	void ZeroMemory(
	[in] PVOID  Destination,
	[in] SIZE_T Length
	);
	Reference: https://docs.microsoft.com/en-us/previous-versions/windows/desktop/legacy/aa366920(v=vs.85)
	*/
	ZeroMemory(&gSvcStatus, sizeof(gSvcStatus));

	// Fill in the SERVICE_STATUS structure to send it to the SCM
	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	gSvcStatus.dwWaitHint = 500;
	gSvcStatus.dwCurrentState = SERVICE_START_PENDING;
	gSvcStatus.dwControlsAccepted = 0;
	gSvcStatus.dwWin32ExitCode = NO_ERROR;
	gSvcStatus.dwServiceSpecificExitCode = NO_ERROR;
	gSvcStatus.dwCheckPoint = 0;

	/*
	Notes
	-----
	* SvcMain first calls the RegisterServiceCtrlHandler function to get the service's SERVICE_STATUS_HANDLE.
	* SvcMain then calls the SetServiceStatus function to notify the service control manager that its status is SERVICE_START_PENDING.

	SERVICE_STATUS_HANDLE RegisterServiceCtrlHandler(
	  [in] LPCSTR             lpServiceName,
	  [in] LPHANDLER_FUNCTION lpHandlerProc
	);
	* Reference: https://docs.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-registerservicectrlhandlera
	*/
	gSvcStatusHandle = RegisterServiceCtrlHandler(
		SERVICE_NAME,		// The name of the service run by the calling thread. This is the service name that the service control program specified in the CreateService function when creating the service.
		ServiceCtrlHandler	// A pointer to the handler function to be registered
	);
	if (!gSvcStatusHandle) {
		return;
	}

	/*
	Updates the service control manager's status information for the calling service.
	BOOL SetServiceStatus(
	  [in] SERVICE_STATUS_HANDLE hServiceStatus,
	  [in] LPSERVICE_STATUS      lpServiceStatus
	);
	Reference: https://docs.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-setservicestatus
	*/
	if (SetServiceStatus(gSvcStatusHandle, &gSvcStatus) == FALSE) {
		return;
	}

	ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ghSvcStopEvent == NULL) {
		gSvcStatus.dwControlsAccepted = 0;
		gSvcStatus.dwCurrentState = SERVICE_STOPPED;
		gSvcStatus.dwWin32ExitCode = GetLastError();
		gSvcStatus.dwCheckPoint = 1;

		if (SetServiceStatus(gSvcStatusHandle, &gSvcStatus) == FALSE) {
			return;
		}
		return;
	}

	gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	gSvcStatus.dwCurrentState = SERVICE_RUNNING;
	gSvcStatus.dwWin32ExitCode = gSvcStatus.dwCheckPoint = 0;

	if (SetServiceStatus(gSvcStatusHandle, &gSvcStatus) == FALSE) {
		return;
	}

	// Waits until the specified object is in the signaled state.
	// https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
	while (WaitForSingleObject(ghSvcStopEvent, 0) != WAIT_OBJECT_0) {
		Sleep(30);
	}

	gSvcStatus.dwControlsAccepted = gSvcStatus.dwWin32ExitCode = 0;
	gSvcStatus.dwCurrentState = SERVICE_STOPPED;
	gSvcStatus.dwCheckPoint = 3;

	if (SetServiceStatus(gSvcStatusHandle, &gSvcStatus) == FALSE) {
		return;
	}
}

/*
Notes
-----
* The main function of a service program calls the StartServiceCtrlDispatcher function to connect to the service control manager (SCM) and start the control dispatcher thread.
* The dispatcher thread loops, waiting for incoming control requests for the services specified in the dispatch table.
* Reference: https://docs.microsoft.com/en-us/windows/win32/services/writing-a-service-program-s-main-function
*/
int main(int argc, char* argv[]) {
	/*
	Notes
	-----
	* Specifies the ServiceMain function for a service that can run in the calling process.
	* It is used by the StartServiceCtrlDispatcher function.
	* Reference: https://docs.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_table_entrya
	*/
	SERVICE_TABLE_ENTRY DispatchTable[] = {
		{
			(LPTSTR)SERVICE_NAME, // The name of a service to be run in this service process
			(LPSERVICE_MAIN_FUNCTION)SvcMain // A pointer to a ServiceMain function
		},
		{ NULL, NULL }
	};
	/*
	Notes
	-----
	* When the SCM starts a service program, it waits for it to call the StartServiceCtrlDispatcher function
	* The StartServiceCtrlDispatcher function takes a SERVICE_TABLE_ENTRY structure for each service contained in the process
	* Reference: https://docs.microsoft.com/en-us/windows/win32/services/service-entry-point
	*/
	if (!StartServiceCtrlDispatcher(DispatchTable))
	{
		return 0;
	}
	return 1;
}