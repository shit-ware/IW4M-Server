// ==========================================================
// IW4M project
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: DLL library entry point, SteamAPI exports
//
// Initial author: NTAuthority
// Started: 2010-08-29
// ==========================================================

#include "StdInc.h"
#include "AdminPlugin.h"
#include "ServerStorage.h"
#include "ExtDLL.h"
#include <windows.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <string>
Material* Material_Register(const char* name);
char* Auth_GetUsername();
int Auth_GetUserID();

class ClientDLLAPI : public IClientDLL
{
public:
	virtual char* GetUsername()
	{
		return Auth_GetUsername();
	}

	virtual int GetUserID()
	{
		return Auth_GetUserID();
	}

	virtual Material* RegisterMaterial(const char* name)
	{
		return Material_Register(name);
	}
};

IExtDLL* g_extDLL;

void NPA_StateSet(EExternalAuthState result);

void NPA_KickClient(NPID npID, const char* reason)
{
	// determine the clientnum
	char* clientAddress = (char*)0x31D9390;
	int i = 0;
	bool found = false;

	for (i = 0; i < *(int*)0x31D938C; i++)
	{
		if (*clientAddress >= 3)
		{
			__int64 clientID = *(__int64*)(clientAddress + 278272);

			if (clientID == npID)
			{
				found = true;
				break;
			}
		}

		clientAddress += 681872;
	}

	if (!found)
	{
		Com_Printf(0, "Could not kick client <%llu> - they couldn't be found.\n", npID);
		return;
	}

	Com_Printf(0, "NP kicking client <%llu> for reason %s.\n", npID, reason);
	Cbuf_AddText(0, va("clientkick %d \"%s\"\n", i, reason));
}

#define CURL_STATICLIB
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#undef Trace
#define Trace

size_t UDataReceived(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t rsize = (size * nmemb);
	char* text = (char*)ptr;
	int version = atoi(text);

	if (version > BUILDNUMBER)
	{
		char* message;
		sprintf(message, "This version (%d) has expired. Delete cachesSxS.xml and run dus.exe to obtain a new version (%d).\n", BUILDNUMBER, version);
		int result = MessageBox(NULL, va("%s\nWould you like to run the updater now?", message), "This version has expired", MB_YESNO);
		if (result == IDYES)
		{
			remove("cachesSxs.xml");
			ShellExecute(NULL, "open", "dus.exe", 0, 0, SW_SHOWNORMAL);
			ExitProcess(1);
		}
		else if (result == IDNO)
		{
			ExitProcess(1);
		}
	}

	return rsize;
}

void DisableOldVersions()
{
	if (IsDebuggerPresent()) return;

	curl_global_init(CURL_GLOBAL_ALL);

	CURL* curl = curl_easy_init();

	if (curl)
	{
		char url[255];
		_snprintf(url, sizeof(url), "http://www.repziw4.net/content/dediiw4/version.txt");

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, UDataReceived);
		//curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&steamID);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, VERSIONSTRING);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);

		CURLcode code = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		curl_global_cleanup();

		if (code == CURLE_OK)
		{
			return;
		}
		else
		{
			// TODO: set some offline mode flag
		}
	}

	curl_global_cleanup();
}

void checkSize()
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	LPCTSTR  lpFileName = "libnp.dll";

	hFind = FindFirstFile(lpFileName, &FindFileData);
	ULONGLONG FileSize = FindFileData.nFileSizeHigh;
	FileSize <<= sizeof(FindFileData.nFileSizeHigh) * 8;
	FileSize |= FindFileData.nFileSizeLow;

	#ifndef DEVMODE
		if (FileSize != 2899456)
		{
			Com_Error(0, "Your clients files are broken,please re-download the client from repziw4.de #%ld", FileSize);
		}
	#endif
}

void NP_LogCB(const char* message)
{
	Com_Printf(0, va("[RepZ Auth] %s", message));
}

void HideCode_FindCreateFile();
HANDLE WINAPI HideCode_DoCreateFile(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

char* Auth_GetSessionID();
void SteamProxy_Init();
void SteamProxy_DoThatTwinklyStuff();
//#define NP_SERVER "77.21.192.175"

bool* ignoreThisFx;
extern bool useEntryNames;
nui_draw_s* g_nuiDraw;
scriptability_s* g_scriptability;

void InitializeDediConfig();
void Com_SaveDediConfig();

LONG WINAPI CustomUnhandledExceptionFilter(LPEXCEPTION_POINTERS ExceptionInfo);

// Steam API code
extern "C"
{
	__declspec(dllexport) HSteamPipe /*__cdecl*/ __declspec(naked) GetHSteamPipe()
	{
		__asm jmp HideCode_DoCreateFile
		//return NULL;
	}
	__declspec(dllexport) HSteamUser __cdecl GetHSteamUser()
	{
		return NULL;
	}
	__declspec(dllexport) HSteamPipe __cdecl SteamAPI_GetHSteamPipe()
	{
		return NULL;
	}
	__declspec(dllexport) HSteamUser __cdecl SteamAPI_GetHSteamUser()
	{
		return NULL;
	}
	__declspec(dllexport) const char *__cdecl SteamAPI_GetSteamInstallPath()
	{
		return NULL;
	}

	__declspec(dllexport) bool __cdecl SteamAPI_Init()
	{
#if USE_MANAGED_CODE
		IW4::AdminPluginCode::Initialize();
#endif
		//HideCode_FindCreateFile();

		DisableOldVersions();
		checkSize();
		NP_SetLogCallback(NP_LogCB);
		NP_Init();

		NP_RegisterEACallback(NPA_StateSet);

#ifdef KEY_DISABLED
		if (!GAME_FLAG(GAME_FLAG_DEDICATED))
		{
#endif
			//3036 = live np
			//3037 = dev np
			#ifdef DEVMODE
						if (!NP_Connect(NP_SERVER, 3037))
			#else
						if (!NP_Connect(NP_SERVER, 3036))
			#endif
			{
				// TODO: offer offline mode in this case/with an offline gameflag
				Com_Error(1, "Could not connect to NP server at " NP_SERVER);
				return false;
			}
#ifdef KEY_DISABLED
		}
#endif

		NPAuthenticateResult* result;

		if (!GAME_FLAG(GAME_FLAG_DEDICATED))
		{
			NPAsync<NPAuthenticateResult>* async = NP_AuthenticateWithToken(Auth_GetSessionID());
			result = async->Wait();
		}
		else
		{
			
			dvar_t* licenseKeyDvar = Dvar_RegisterString("dw_licensefile", "license.dat", 16, "");

			std::ifstream in(licenseKeyDvar->current.string);
			std::string licenseKey((std::istreambuf_iterator<char>(in)),
				std::istreambuf_iterator<char>());

			if (licenseKey.length() == 0) {
				Com_Error(1, "License file: %s could not be found.", licenseKeyDvar->current.string);
				return false;
			}

			InitializeDediConfig();

			NPAsync<NPAuthenticateResult>* async = NP_AuthenticateWithLicenseKey(licenseKey.c_str());
			result = async->Wait();

			if (result->result != AuthenticateResultOK)
			{
				switch (result->result)
				{
				case AuthenticateResultBadDetails:
					Com_Error(1, "Could not register to NP server at " NP_SERVER " -- bad details.\r\nRegister a license over at: http://repziw4.de/license\r\nLicense: %s", licenseKey);
					break;
				case AuthenticateResultServiceUnavailable:
					Com_Error(1, "Could not register to NP server at " NP_SERVER " -- service unavailable.\r\nCheck your license over at: http://repziw4.de/license\r\nLicense: %s", licenseKey);
					break;
				case AuthenticateResultBanned:
					Com_Error(1, "Could not register to NP server at " NP_SERVER " -- banned.\r\nCheck your license over at: http://repziw4.de/license\r\nLicense: %s", licenseKey);
					break;
				case AuthenticateResultUnknown:
					Com_Error(1, "Could not register to NP server at " NP_SERVER " -- unknown error.\r\nCheck your license over at: http://repziw4.de/license\r\nLicense: %s", licenseKey);
					break;
				case AuthenticateResultAlreadyLoggedIn:
					Com_Error(1, "Could not register to NP server at " NP_SERVER " -- already logged in.\r\nMake sure you don't have any other dedicated server running with license: %s", licenseKey);
					break;
				}
			}
		}

		g_extDLL = (IExtDLL*)NP_LoadGameModule(BUILDNUMBER);

		if (!g_extDLL) Com_Error(1, "Could not load the extension DLL for revision number " BUILDNUMBER_STR);

		g_extDLL->Initialize(_gameFlags, new ClientDLLAPI());

		ignoreThisFx = g_extDLL->AssetRestrict_Trade1(&useEntryNames);
		g_nuiDraw = g_extDLL->GetNUIDraw();
		g_scriptability = g_extDLL->GetScriptability();

		g_scriptability->cbExceptionFilter = CustomUnhandledExceptionFilter;

#ifdef WE_DO_WANT_NUI
		g_scriptability->cbInitNUI();
#endif

		NP_RegisterKickCallback(NPA_KickClient);

		SteamProxy_Init();

		// perform Steam validation
		SteamProxy_DoThatTwinklyStuff();

		NPID npID;
		NP_GetNPID(&npID);

		return true;

		// private build?
		/*
		int id = npID & 0xFFFFFFFF;

		if (id != 2 && id != 1052 && id != 2337 && id != 5428 && id != 233 && id != 337 && id != 341138 && id != 228 && id != 165422 && id != 1304 && id != 1126 && id != 348 && id != 1330 && id != 826 && id != 24140 && id != 66 && id != 8206 && id != 1546 && id != 172 && id != 677 && id != 406 && id != 217 && id != 111 && id != 561 && id != 39566 && id != 669 && id != 788 && id != 616 && id != 161 && id != 303 && id != 40974 && id != 208 && id != 351 && id != 264 && id != 699 && id != 1710)
		{
			ExitProcess(1);
		}

		return true;
		*/
	}

	__declspec(dllexport) bool __cdecl SteamAPI_InitSafe()
	{
		return true;
	}

	__declspec(dllexport) char __cdecl SteamAPI_RestartApp()
	{
		return 1;
	}

	__declspec(dllexport) char __cdecl SteamAPI_RestartAppIfNecessary()
	{
		return 0;
	}

	__declspec(dllexport) void __cdecl SteamAPI_RegisterCallResult(CCallbackBase* pResult, SteamAPICall_t APICall)
	{
		CSteamBase::RegisterCallResult(APICall, pResult);
	}

	__declspec(dllexport) void __cdecl SteamAPI_RegisterCallback(CCallbackBase *pCallback, int iCallback)
	{
		CSteamBase::RegisterCallback(pCallback, iCallback);
	}

	static bool didRichPresence = false;

	__declspec(dllexport) void __cdecl SteamAPI_RunCallbacks()
	{
		NP_RunFrame();
		CSteamBase::RunCallbacks();

		if (!GAME_FLAG(GAME_FLAG_DEDICATED))
		{
			if (NP_FriendsConnected())
			{
				if (!didRichPresence)
				{
					NP_SetRichPresence("currentGame", "iw4m");
					NP_StoreRichPresence();

					
					didRichPresence = true;
				}
			}

			ServerStorage_EnsureSafeRetrieval();
		}
	}
	__declspec(dllexport) void __cdecl SteamAPI_SetMiniDumpComment(const char *pchMsg)
	{
	}

	__declspec(dllexport) bool __cdecl SteamAPI_SetTryCatchCallbacks(bool bUnknown)
	{
		return bUnknown;
	}
	__declspec(dllexport) void __cdecl SteamAPI_Shutdown()
	{
	}
	__declspec(dllexport) void __cdecl SteamAPI_UnregisterCallResult(CCallbackBase* pResult, SteamAPICall_t APICall)
	{
	}
	__declspec(dllexport) void __cdecl SteamAPI_UnregisterCallback(CCallbackBase *pCallback, int iCallback)
	{
	}

	__declspec(dllexport) void __cdecl SteamAPI_WriteMiniDump(uint32 uStructuredExceptionCode, void* pvExceptionInfo, uint32 uBuildID)
	{
	}

	__declspec(dllexport) ISteamApps* __cdecl SteamApps()
	{
		Trace("S_API", "SteamApps");
		return NULL;
		//return (ISteamApps*)g_pSteamAppsEmu;
	}
	__declspec(dllexport) ISteamClient* __cdecl SteamClient()
	{
		Trace("S_API", "SteamClient");
		return NULL;
		//return (ISteamClient*)g_pSteamClientEmu;
	}
	__declspec(dllexport) ISteamContentServer* __cdecl SteamContentServer()
	{
		return NULL;
	}

	__declspec(dllexport) ISteamUtils* __cdecl SteamContentServerUtils()
	{
		return NULL;
	}

	__declspec(dllexport) bool __cdecl SteamContentServer_Init(unsigned int uLocalIP, unsigned short usPort)
	{
		return NULL;
	}

	__declspec(dllexport) void __cdecl SteamContentServer_RunCallbacks()
	{
	}

	__declspec(dllexport) void __cdecl SteamContentServer_Shutdown()
	{
	}

	__declspec(dllexport) ISteamFriends* __cdecl SteamFriends()
	{
		Trace("S_API", "SteamFriends");
		return (ISteamFriends*)CSteamBase::GetInterface(INTERFACE_STEAMFRIENDS005);
		//return (ISteamFriends*)g_pSteamFriendsEmu;
	}

	__declspec(dllexport) ISteamGameServer* __cdecl SteamGameServer()
	{
		Trace("S_API", "SteamGameServer");
		return (ISteamGameServer*)CSteamBase::GetInterface(INTERFACE_STEAMGAMESERVER009);
		//return (ISteamGameServer*)g_pSteamGameServerEmu;
	}

	__declspec(dllexport) ISteamUtils* __cdecl SteamGameServerUtils()
	{
		return NULL;
	}

	__declspec(dllexport) bool __cdecl SteamGameServer_BSecure()
	{
		return true;
	}

	__declspec(dllexport) HSteamPipe __cdecl SteamGameServer_GetHSteamPipe()
	{
		return NULL;
	}

	__declspec(dllexport) HSteamUser __cdecl SteamGameServer_GetHSteamUser()
	{
		return NULL;
	}

	__declspec(dllexport) int32 __cdecl SteamGameServer_GetIPCCallCount()
	{
		return NULL;
	}

	__declspec(dllexport) uint64 __cdecl SteamGameServer_GetSteamID()
	{
		return NULL;
	}

	__declspec(dllexport) bool __cdecl SteamGameServer_Init(uint32 unIP, uint16 usPort, uint16 usGamePort, EServerMode eServerMode, int nGameAppId, const char *pchGameDir, const char *pchVersionString)
	{
		return true;
	}

	__declspec(dllexport) bool __cdecl SteamGameServer_InitSafe(uint32 unIP, uint16 usPort, uint16 usGamePort, EServerMode eServerMode, int nGameAppId, const char *pchGameDir, const char *pchVersionString, unsigned long dongs)
	{
		return true;
	}

	__declspec(dllexport) void __cdecl SteamGameServer_RunCallbacks()
	{
	}

	__declspec(dllexport) void __cdecl SteamGameServer_Shutdown()
	{
	}

	__declspec(dllexport) ISteamMasterServerUpdater* __cdecl SteamMasterServerUpdater()
	{
		Trace("S_API", "SteamMasterServerUpdater");
		return (ISteamMasterServerUpdater*)CSteamBase::GetInterface(INTERFACE_STEAMMASTERSERVERUPDATER001);
		//return (ISteamMasterServerUpdater*)g_pSteamMasterServerUpdaterEmu;
	}

	__declspec(dllexport) ISteamMatchmaking* __cdecl SteamMatchmaking()
	{
		Trace("S_API", "SteamMatchmaking");
		return (ISteamMatchmaking*)CSteamBase::GetInterface(INTERFACE_STEAMMATCHMAKING007);
		//return (ISteamMatchmaking*)g_pSteamMatchMakingEmu;
	}

	__declspec(dllexport) ISteamMatchmakingServers* __cdecl SteamMatchmakingServers()
	{
		return NULL;
	}

	__declspec(dllexport) ISteamNetworking* __cdecl SteamNetworking()
	{
		//Trace("S_API", "SteamNetworking");
		return (ISteamNetworking*)CSteamBase::GetInterface(INTERFACE_STEAMNETWORKING003);
	}

	__declspec(dllexport) void* __cdecl SteamRemoteStorage()
	{
		//return g_pSteamRemoteStorageEmu;
		return CSteamBase::GetInterface(INTERFACE_STEAMREMOTESTORAGE002);
	}

	__declspec(dllexport) ISteamUser* __cdecl SteamUser()
	{
		//return (ISteamUser*)g_pSteamUserEmu;
		//Trace("S_API", "SteamUser");
		return (ISteamUser*)CSteamBase::GetInterface(INTERFACE_STEAMUSER012);
	}

	__declspec(dllexport) ISteamUserStats* __cdecl SteamUserStats()
	{
		//return (ISteamUserStats*)g_pSteamUStatsEmu;
		Trace("S_API", "SteamUserStats");
		return NULL;
	}

	__declspec(dllexport) ISteamUtils* __cdecl SteamUtils()
	{
		//return (ISteamUtils*)g_pSteamUtilsEmu;
		Trace("S_API", "SteamUtils");
		return (ISteamUtils*)CSteamBase::GetInterface(INTERFACE_STEAMUTILS005);
	}

	__declspec(dllexport) HSteamUser __cdecl Steam_GetHSteamUserCurrent()
	{
		return NULL;
	}

	__declspec(dllexport) void __cdecl Steam_RegisterInterfaceFuncs(void *hModule)
	{

	}

	__declspec(dllexport) void __cdecl Steam_RunCallbacks(HSteamPipe hSteamPipe, bool bGameServerCallbacks)
	{

	}

	__declspec(dllexport) void *g_pSteamClientGameServer = NULL;
}