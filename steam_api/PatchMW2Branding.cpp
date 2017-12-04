// ==========================================================
// IW4M project
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: branding for IW4M
//
// Initial author: NTAuthority
// Started: 2011-06-09
// ==========================================================

#include "StdInc.h"

typedef void* (__cdecl * R_RegisterFont_t)(const char* asset);
R_RegisterFont_t R_RegisterFont = (R_RegisterFont_t)0x505670;

typedef void (__cdecl * R_AddCmdDrawText_t)(const char* text, int, void* font, float screenX, float screenY, float, float, float rotation, float* color, int);
R_AddCmdDrawText_t R_AddCmdDrawText = (R_AddCmdDrawText_t)0x509D80;

CallHook drawDevStuffHook;
DWORD drawDevStuffHookLoc = 0x5ACB99;

void DrawDemoWarning()
{
#ifdef PRE_RELEASE_DEMO
	void* font = R_RegisterFont("fonts/objectivefont");
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	R_AddCmdDrawText("PRE-RELEASE DEMO", 0x7FFFFFFF, font, 10, 30, 0.7f, 0.7f, 0.0f, color, 0);
#endif
}

void __declspec(naked) DrawDevStuffHookStub()
{
	__asm
	{
		call DrawDemoWarning
		jmp drawDevStuffHook.pOriginal
	}
}

void CL_XStartPrivateMatch_f()
{
	Com_Error(2, "This feature is not available.");
}

/*
StompHook customSearchPathHook;
DWORD customSearchPathHookLoc = 0x44A510;

dvar_t** fs_basepath = (dvar_t**)0x63D0CD4;

void CustomSearchPathStuff()
{
	const char* basePath = (*fs_basepath)->current.string;
	const char* dir = "m2demo";

	__asm
	{
		mov ecx, 642EF0h
		mov ebx, basePath
		mov eax, dir
		call ecx
	}
}
*/

void PatchMW2_Branding()
{
	//drawDevStuffHook.initialize((PBYTE)drawDevStuffHookLoc);
	//drawDevStuffHook.installHook(DrawDevStuffHookStub, false);

	// displayed build tag in UI code
	*(DWORD*)0x43F73B = (DWORD)VERSIONSTRING;

	// console '%s: %s> ' string
	*(DWORD*)0x5A44B4 = (DWORD)(VERSIONSTRING "> ");

	// console version string
	*(DWORD*)0x4B12BB = (DWORD)(VERSIONSTRING " " BUILDHOST " (built " __DATE__ " " __TIME__ ")");

	// version string
	*(DWORD*)0x60BD56 = (DWORD)(VERSIONSTRING " (built " __DATE__ " " __TIME__ ")");

	// add 'm2demo' game folder
	//customSearchPathHook.initialize(5, (PBYTE)customSearchPathHookLoc);
	//customSearchPathHook.installHook(CustomSearchPathStuff, true, false);

	// set fs_basegame to 'm2demo' (will apply before fs_game, unlike the above line)
	*(DWORD*)0x6431D1 = (DWORD)"m2demo";

	// increase font sizes for chat on higher resolutions
	//*(BYTE*)0x5814AA = 0xEB;
	//*(BYTE*)0x5814C4 = 0xEB;
	static float float13 = 13.0f;
	static float float10 = 10.0f;

	*(float**)0x5814AE = &float13;
	*(float**)0x5814C8 = &float10;

	// safe areas; set to 1 (changing the dvar has no effect later on as the ScrPlace viewport is already made)
	//static float float1 = 1.0f;
	//*(float**)0x42E3A8 = &float1;
	//*(float**)0x42E3D9 = &float1;

#ifdef PRE_RELEASE_DEMO
	// disable private parties if demo
	//*(DWORD*)0x40554E = (DWORD)CL_XStartPrivateMatch_f;
	//*(DWORD*)0x5A992E = (DWORD)CL_XStartPrivateMatch_f;
	//*(DWORD*)0x4058B9 = (DWORD)CL_XStartPrivateMatch_f; // xpartygo
	//*(DWORD*)0x4152AA = (DWORD)CL_XStartPrivateMatch_f; // map

	// use M2 playlists
	strcpy((char*)0x6EE7AC, "mp_playlists_m2");
	*(DWORD*)0x4D47FB = (DWORD)"mp_playlists_m2.ff";
#endif
}