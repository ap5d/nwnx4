// vaultster.cpp : Defines the entry point for the DLL application.
//
//#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "nwnxvaultster.h"
#include "vaultster.h"

CNWNXVaultster vaultster;

VAULTSTER_API CNWNXBase* GetClassObject ()
{
	return &vaultster;
}

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
   return TRUE;
}