// ==========================================================
// secretSchemes
// 
// Component: steam_api
// Sub-component: clientdll
// Purpose: various random detections
//
// Initial author: Terminator
// Started: 2014-11-16
// ==========================================================

#include "stdinc.h"
#include <tchar.h>

void CI_SendStatus(int acID);

// blargh, skiddie detections follow
void DebuggerCheck()
{
	// TODO: Get rid of this skiddie method, it's easily bypassable.
	// will have to do for the moment, though.
	if (IsDebuggerPresent())
	{
		CI_SendStatus(CI_ID_IW4M_DEBUGGER_PRESENT);
	}
}

bool scanAdd(const char* canBe, DWORD Address, int numBytes = 1)
{
	if (memcmp(canBe, (void*)Address, numBytes) != 0)
	{
		return true;
	}

	return false;
}

bool CheckProc(LPCWSTR WindowName)
{
	HWND proc = FindWindow(NULL, (LPSTR)WindowName);

	if (proc != NULL)
	{
		return true;
	}

	return false;
}

void CI_CheckProcesses()
{
	if (CheckProc(L"Crosshair Overlay") || (CheckProc(L"ESPBox") || (CheckProc(L"HashChanger") || CheckProc(L"Extreme Injector v2 by master131") || CheckProc(L"Injector")) //bypass : rename .exe lol yes but it will catch at least some of the fuckers
	{
		CI_SendStatus(CI_ID_IW4M_WINDOW);
	}
}

void CI_Norecoil()
{
	if (scanAdd("\x68\xc4\x25\xa8\x00", 0xB35980, 3) ||
		scanAdd("\x68\xc4\x25\xa8\x00", 0xB35980, 4))
	{
		CI_SendStatus(CI_ID_IW4M_NORECOIL);
	}
}

// CI_ID_DVARUNLOCKER: 3005
/*
void CI_DvarUnlocker()
{
	// bypasses for cheatprotected, writeprotected and read-only
	if (scanAdd("\x74\x0A", 0x8BC5F1, 2) ||
		scanAdd("\x74\x03", 0x8BC5E7, 2) ||
		scanAdd("\x74\x0A", 0x8BC5FF, 2) ||
		scanAdd("\x75\xCE", 0x8BC619, 2) ||
		scanAdd("\x74\x05", 0x8BC61E, 2) ||
		scanAdd("\x75\xC4", 0x8BC623, 2))
	{
		CI_SendStatus(CI_ID_T5_DVARUNLOCKER);
	}
}
*/