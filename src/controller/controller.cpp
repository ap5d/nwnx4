/***************************************************************************
    NWNX Controller - Controls the server process
    Copyright (C) 2006 Ingmar Stieger (Papillon, papillon@nwnx.org)
	Copyright (C) 2008 Skywing (skywing@valhallalegends.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

***************************************************************************/

#include "controller.h"

extern LogNWNX* logger;

static std::string GetNwn2InstallPath()
/*++

Routine Description:

	This routine attempts to auto detect the NWN2 installation path from the
	registry.

Arguments:

	None.

Return Value:

	The routine returns the game installation path if successful.  Otherwise,
	an std::exception is raised.

Environment:

	User mode.

Author:

	Copyright (c) Ken Johnson (Skywing). All rights reserved.
	https://github.com/SkywingvL/nwn2dev-public/blob/master/NWNScriptConsole/AppParams.cpp#L310

--*/
{
	HKEY  Key;
	CHAR  NameBuffer[ MAX_PATH + 1 ];
	DWORD NameBufferSize;
	LONG  Status;

	Status = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Obsidian\\NWN 2\\Neverwinter",
		REG_OPTION_RESERVED,
#ifdef _WIN64
		KEY_QUERY_VALUE | KEY_WOW64_32KEY,
#else
		KEY_QUERY_VALUE,
#endif
		&Key);

	if (Status != NO_ERROR)
	{
		Status = RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			L"SOFTWARE\\GOG.com\\GOGNWN2COMPLETE",
			REG_OPTION_RESERVED,
#ifdef _WIN64
			KEY_QUERY_VALUE | KEY_WOW64_32KEY,
#else
			KEY_QUERY_VALUE,
#endif
			&Key);

		if (Status == NO_ERROR)
		{
			NameBufferSize = sizeof( NameBuffer ) - sizeof( NameBuffer[ 0 ] );

			Status = RegQueryValueExA(
				Key,
				"PATH",
				NULL,
				NULL,
				(LPBYTE) NameBuffer,
				&NameBufferSize);

			RegCloseKey( Key );
			Key = NULL;

			if (Status == NO_ERROR)
			{
				//
				// Strip trailing null byte if it exists.
				//

				if ((NameBufferSize > 0) &&
					(NameBuffer[ NameBufferSize - 1 ] == '\0'))
					NameBufferSize -= 1;

				return std::string( NameBuffer, NameBufferSize );
			}
		}

		throw std::exception( "Unable to open NWN2 registry key" );
	}

	try
	{
		bool                FoundIt;
		static const char * ValueNames[ ] =
		{
			"Path",     // Retail NWN2
			"Location", // Steam NWN2
		};

		for (size_t i = 0; i < _countof( ValueNames ); i += 1)
		{
			NameBufferSize = sizeof( NameBuffer ) - sizeof( NameBuffer[ 0 ] );

			Status = RegQueryValueExA(
				Key,
				ValueNames[ i ],
				NULL,
				NULL,
				(LPBYTE) NameBuffer,
				&NameBufferSize);

			if (Status != NO_ERROR)
				continue;

			//
			// Strip trailing null byte if it exists.
			//

			if ((NameBufferSize > 0) &&
				(NameBuffer[ NameBufferSize - 1 ] == '\0'))
				NameBufferSize -= 1;

			RegCloseKey( Key );
			Key = NULL;

			return std::string( NameBuffer, NameBufferSize );
		}

		throw std::exception( "Unable to read Path from NWN2 registry key" );
	}
	catch (...)
	{
		if (Key != NULL)
			RegCloseKey( Key );
		throw;
	}
}

NWNXController::NWNXController(SimpleIniConfig* config)
{
    this->config = config;

    // Setup temporary directories.
    this->setupTempDirectories();

	// Set nwnx4 dir env variable
	GetCurrentDirectoryA(MAX_PATH, m_nwnx4Dir);
	SetEnvironmentVariableA("NWNX4_DIR", m_nwnx4Dir);

    tick = 0;
	initialized = false;
	shuttingDown = false;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));

	config->Read("restartDelay", &restartDelay, 5L);
	config->Read("processWatchdog", &processWatchdog, true);
	config->Read("gamespyWatchdog", &gamespyWatchdog, true);
	config->Read("gamespyInterval", &gamespyInterval, 30);
	config->Read("gamespyRetries", &gamespyRetries, 0);
	config->Read("gamespyTolerance", &gamespyTolerance, 5);
	config->Read("gamespyDelay", &gamespyDelay, 30L);
	config->Read("gracefulShutdownTimeout", &gracefulShutdownTimeout, 10);
	config->Read("gracefulShutdownMessage", &gracefulShutdownMessage, std::string(""));
	config->Read("gracefulShutdownMessageWait", &gracefulShutdownMessageWait, 5);

	if (!config->Read("parameters", &parameters) )
	{
		logger->Info("Parameter setting not found in nwnx.ini. Starting server with empty commandline.");
	}
	parameters += " ";

	if (gamespyWatchdog)
	{
		config->Read("gamespyPort", &gamespyPort, 5121);
		try
		{
			udp = new CUDP("localhost", gamespyPort);
		}
		catch (std::bad_alloc)
		{
			udp = nullptr;
		}
	}

	if (!config->Read("nwn2", &nwninstalldir))
	{
		try{
			nwninstalldir = GetNwn2InstallPath();
		}
		catch(std::exception){
			logger->Info("* NWN2 installation directory not found. Check your nwnx.ini file.");
			return;
		}
	}
	logger->Trace("NWN2 install dir: %s", nwninstalldir.c_str());
	logger->Trace("NWN2 parameters: %s", parameters.c_str());

	// Create log service (if necessary).
	try {
        logService = new NWNX4::RPC::Log::LogService(".");
        logServiceConnection = logService->StartAsync();
	} catch (const std::exception& exception) {
	    logger->Err(exception.what());
    } catch (const char* exception) {
        logger->Err(exception);
    } catch (...) {
	    logger->Err("Unknown error.");
	}
}

NWNXController::~NWNXController()
{
	killServerProcess();

	if (udp)
		delete udp;

	if (logService != nullptr) {
        delete logService;
    }
}

void NWNXController::setupTempDirectories() {
    std::string tempPath;
    if (config->Read("nwn2temp", &tempPath))
    {
        wchar_t wTempPath[MAX_PATH];
        memset(wTempPath, 0, MAX_PATH);
        mbstowcs(wTempPath, tempPath.c_str(), tempPath.length());

        SetEnvironmentVariable(L"TEMP", wTempPath);
        SetEnvironmentVariable(L"TMP", wTempPath);
    }
}
/***************************************************************************
    NWNServer related functions
***************************************************************************/

void NWNXController::startServerProcess()
{
	killServerProcess(false);

	while (!shuttingDown && !startServerProcessInternal())
	{
		killServerProcess(false);
		logger->Info( "! Error: Failed to start server process, retrying in 5000ms..." );
		Sleep(5000);
	}
}

void NWNXController::notifyServiceShutdown()
{
	shuttingDown = true;
}

bool NWNXController::startServerProcessInternal()
{
    SHARED_MEMORY shmem;
	std::string nwnexe("\\nwn2server.exe");
	auto pszHookDLLPath = "NWNX4_Hook.dll";

	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	auto exePath = nwninstalldir + nwnexe;
	logger->Trace("Starting server executable %s in %s", exePath.c_str(), nwninstalldir.c_str());

	char szDllPath[MAX_PATH];
	LPSTR pszFilePart = nullptr;

    if (!GetFullPathNameA(pszHookDLLPath, arrayof(szDllPath), szDllPath, &pszFilePart))
	{
		logger->Info("Error: %s could not be found.", pszHookDLLPath);
		return false;
	}

	SECURITY_ATTRIBUTES SecurityAttributes;
	SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	SecurityAttributes.bInheritHandle = TRUE;
	SecurityAttributes.lpSecurityDescriptor = 0;

	shmem.ready_event = CreateEvent(&SecurityAttributes, TRUE, FALSE, nullptr);
	if(!shmem.ready_event)
	{
		logger->Info("CreateEvent failed (%d)", GetLastError());
		return false;
	}

	DWORD dwFlags = CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED;
	SetLastError(0);

	char params[MAX_PATH];
	strcpy(params, "nwn2server.exe ");
	strncat(params, parameters.c_str(), MAX_PATH);

	auto envRes = DoEnvironmentSubstA(params, MAX_PATH);
	if(!HIWORD(envRes)){
		logger->Err("Could not substitute environment variables in NWN2Server command line: %s", params);
	}

	logger->Trace("Starting: %s\\%s", nwninstalldir.c_str(), params);
	logger->Trace("with %s", szDllPath);

	if (!DetourCreateProcessWithDllExA(exePath.c_str(), params,
                                    nullptr, nullptr, TRUE, dwFlags, nullptr, nwninstalldir.c_str(),
                                    &si, &pi, szDllPath, nullptr))
	{
		auto err = GetLastError();
		logger->Err("DetourCreateProcessWithDll failed: %d", err);
		if (err == 740) {
			logger->Err("You probably need to run the command as administrator.");
		}
		CloseHandle( shmem.ready_event );
		ZeroMemory( &pi, sizeof( PROCESS_INFORMATION ) );
		return false;
    }

    logger->Trace("Started nwn2server");

	GUID my_guid =
	{ /* d9ab8a40-f4cc-11d1-b6d7-006097b010e3 */
		0xd9ab8a40,
		0xf4cc,
		0x11d1,
		{0xb6, 0xd7, 0x00, 0x60, 0x97, 0xb0, 0x10, 0xe3}
	};

	strcpy(shmem.nwnx_home, m_nwnx4Dir);
	logger->Trace("Injecting NWNX home directory as '%s'", shmem.nwnx_home);

	if (!DetourCopyPayloadToProcess(pi.hProcess, my_guid, &shmem, sizeof(SHARED_MEMORY))) {
	    logger->Err("! Error: Could not copy payload to process.", GetLastError());
	    return false;
	}

	// Start the main thread running and wait for it to signal that it has read
	// configuration data and started up it's end of any IPC mechanisms that we
	// might rely on.

	ResumeThread(pi.hThread);
	switch(WaitForSingleObject(shmem.ready_event, 60000))
	{
		case WAIT_TIMEOUT:
			logger->Info("! Error: Server did not initialize properly (timeout).");
			CloseHandle( shmem.ready_event );
			CloseHandle( pi.hProcess );
			CloseHandle( pi.hThread );
			ZeroMemory( &pi, sizeof( PROCESS_INFORMATION ) );
			return false;
			break;
		case WAIT_FAILED:
			logger->Info("! Error: Server did not initialize properly (wait failed).");
			CloseHandle( shmem.ready_event );
			CloseHandle( pi.hProcess );
			CloseHandle( pi.hThread );
			ZeroMemory( &pi, sizeof( PROCESS_INFORMATION ) );
			return false;
			break;
		case WAIT_OBJECT_0:
			CloseHandle(shmem.ready_event);
			logger->Info("* Success: Server initialized properly.");
			break;
	}

    DWORD dwResult = 0;
    if (!GetExitCodeProcess(pi.hProcess, &dwResult))
	{
		logger->Info("GetExitCodeProcess failed: %d\n", GetLastError());
		return false;
    }

	// Reset the count of ping probes to zero for purposes of initial load time
	// GameSpy ping allowances.
	tick = 0;

	// Reset GameSpy failed response count.
	gamespyRetries = 0;

	logger->Info("* Hook installed and initialized successfully");
	initialized = true;

	return true;
}

void NWNXController::restartServerProcess()
{
	// Kill any leftovers
	if (checkProcessActive())
		killServerProcess(true);

	// Run maintenance command
	std::string restartCmd;
	config->Read("restartCmd", &restartCmd);
	if (restartCmd != "")
	{
		PROCESS_INFORMATION pi;
		STARTUPINFOA si;

		ZeroMemory(&si,sizeof(si));
		si.cb = sizeof(si);
		logger->Info("* Starting maintenance file %s", restartCmd.c_str());
		restartCmd = std::string("cmd.exe /c ") + restartCmd;
		if (CreateProcessA(nullptr, (LPSTR)restartCmd.c_str(), nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi))
		{
			WaitForSingleObject( pi.hProcess, INFINITE );
			CloseHandle( pi.hProcess );
			CloseHandle( pi.hThread );
		}
	}

	// Finally restart the server
	logger->Info("* Waiting %d seconds before restarting the server.", restartDelay);
	Sleep(restartDelay * 1000);
	logger->Info("* Restarting server.");
	startServerProcess();
}

void NWNXController::killServerProcess(bool graceful)
{
	if (!pi.hProcess)
		return;

	// If we are doing a graceful shutdown, then let's try to poke the
	// server GUI closed so that players are cleanly saved and logged out of
	// the server.

	if (graceful)
	{
		logger->Info( "* Telling server to stop itself..." );
		if (!performGracefulShutdown())
			logger->Info( "* WARNING: Failed to gracefully shutdown the server process." );
	}

	// Mark us as not initialized.

	shutdownServerProcess();
	TerminateProcess(pi.hProcess, -1);
	CloseHandle(pi.hProcess);
	if (pi.hThread)
		CloseHandle(pi.hThread);
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
}

void NWNXController::shutdownServerProcess()
{
	initialized = false;
}

BOOL CALLBACK NWNXController::findServerGuiWindowEnumProc(__in HWND hwnd, __in LPARAM lParam)
{
	DWORD pid;
	WCHAR className[256];
	PFIND_SERVER_GUI_WINDOW_PARAM param;

	param = reinterpret_cast< PFIND_SERVER_GUI_WINDOW_PARAM >( lParam );

	// Ignore windows that do not match the right nwn2server process id.
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid != param->processId)
		return TRUE;

	// Ignore windows that are not of the class of the main server window GUI.
	if (GetClassNameW(hwnd, className, 256))
	{
		if (!wcscmp(className, L"Exo - BioWare Corp., (c) 1999 - Generic Blank Application"))
		{
			param->hwnd = hwnd;
			return FALSE;
		}
	}

	return TRUE;
}

HWND NWNXController::findServerGuiWindow(ULONG processId)
{
	FIND_SERVER_GUI_WINDOW_PARAM param;

	param.hwnd = 0;
	param.processId = processId;

	EnumWindows(findServerGuiWindowEnumProc, (LPARAM)&param);

	return param.hwnd;
}

bool NWNXController::performGracefulShutdown()
{
    HWND serverGuiWindow;

	// Can't perform a graceful shutdown without a process ID.
	if (!pi.dwProcessId || !pi.hProcess)
		return false;

	// Try and locate the server's administrative GUI window.
	serverGuiWindow = findServerGuiWindow(pi.dwProcessId);

	if (!serverGuiWindow)
		return false;

	// If we have a graceful shutdown message then transmit it before we
	// initiate shutdown.
	if (gracefulShutdownMessage != "")
	{
		logger->Info( "* Sending shutdown server message and waiting %d seconds.", gracefulShutdownMessageWait);
		broadcastServerMessage(gracefulShutdownMessage.c_str());
		Sleep(gracefulShutdownMessageWait * 1000);
	}

	// Post a WM_CLOSE to the admin interface window, which initiates a clean
	// server shutdown without the blocking confirmation message box (if there
	// were any players present).
	if (!PostMessage(serverGuiWindow, WM_CLOSE, 0, 0))
		return false;

	// Wait for the server process to exit in the timeout interval.
	return (WaitForSingleObject(pi.hProcess, gracefulShutdownTimeout * 1000) == WAIT_OBJECT_0);
}

bool NWNXController::broadcastServerMessage(const char* message)
{
	HWND srvWnd;
	HWND sendMsgEdit;
	HWND sendMsgButton;
	DWORD_PTR result;

	if (!pi.dwProcessId)
		return false;

	srvWnd = findServerGuiWindow(pi.dwProcessId);

	if (!srvWnd)
		return false;

	sendMsgEdit   = GetDlgItem(srvWnd, IDC_SENDMESSAGE_EDIT);
	sendMsgButton = GetDlgItem(srvWnd, IDC_SENDMESSAGE_BUTTON);

	if (!sendMsgEdit)
		return false;

	if (!sendMsgButton)
		return false;

	SendMessageTimeout(
		sendMsgEdit,
		WM_SETTEXT,
		0,
		reinterpret_cast< LPARAM >( message ),
		SMTO_NORMAL,
		1000,
		&result
		);
	SendMessageTimeout(
		sendMsgButton,
		BM_CLICK,
		0,
		0,
		SMTO_NORMAL,
		1000,
		&result
		);
	SendMessageTimeoutA(
		sendMsgEdit,
		WM_SETTEXT,
		0,
		reinterpret_cast< LPARAM >( "" ),
		SMTO_NORMAL,
		1000,
		&result
		);

	return true;
}

/***************************************************************************
    Watchdog related functions
***************************************************************************/

void NWNXController::ping()
{
	if (initialized)
	{
		if (processWatchdog)
			runProcessWatchdog();
		if (gamespyWatchdog && (tick % gamespyInterval == 0) && (tick > (unsigned long) gamespyDelay))
			runGamespyWatchdog();
		tick++;
	}
}

bool NWNXController::checkProcessActive()
{
	if (!pi.hProcess)
		return false;

	return (WaitForSingleObject( pi.hProcess, 0 ) == WAIT_TIMEOUT);
}

void NWNXController::runProcessWatchdog()
{
	if (checkProcessActive() == false)
	{
		logger->Info("* Server process has gone away.");
		restartServerProcess();
	}
}

void NWNXController::runGamespyWatchdog()
{
	int ret;
	char buffer[2048];

	udp->sendMessage("BNLM");
	Sleep(UDP_WAIT);
	ret = udp->getMessage(buffer, 2048);

	if (ret == 0)
	{
		// No reply from server
		logger->Info("* Warning: Server did not answer gamespy query. %d retries left.",
			gamespyTolerance - gamespyRetries);
		gamespyRetries++;
	}
	else
	{
		// Server answered
		gamespyRetries = 0;
	}

	if (gamespyRetries > gamespyTolerance)
	{
		// Restart server
		logger->Warn("* Server did not answer the last %d gamespy queries.", gamespyTolerance);
		gamespyRetries = 0;
		restartServerProcess();
	}
}
