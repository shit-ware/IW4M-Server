// ==========================================================
// IW4M project
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: Automatically update a server
//
// Initial author: iain17
// Started: 2013-12-08
// ==========================================================

#include "StdInc.h"

#define CURL_STATICLIB
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <windows.h>
#include <Shlwapi.h>
#include <shellapi.h>

static int lastUpdateCheck;
static int timeTillShutdown;
static bool warningGiven;

size_t VersionReceived(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t rsize = (size * nmemb);
	char* text = (char*)ptr;
	int version = atoi(text);

	if (version > BUILDNUMBER)
	{
		if ((Com_Milliseconds() - timeTillShutdown) > 1800000)
		{
			// First, kick all of the players.
			for (int i = 0; i < *svs_numclients; i++)
			{
				client_t* client = &svs_clients[i];

				if (client->state < 3)
				{
					continue;
				}

				Cmd_ExecuteSingleCommand(0, 0, va("clientkick %i Server will update", client));
			}

			remove("cachesSxs.xml");
			ShellExecute(NULL, "open", "dus.exe", 0, 0, SW_SHOWNORMAL);
			ExitProcess(1);
		}


		Com_Printf(1, va("A new server version is available, please run dus.exe"));

		if (!warningGiven)
			MessageBoxA(NULL, "A new server version is now available. Please shutdown this server and run dus.exe\nThe server will automatically update in 30 minutes should you not update.", "Update available", MB_OK | MB_ICONINFORMATION);
		warningGiven = true;
		timeTillShutdown = Com_Milliseconds();

		SV_GameSendServerCommand(-1, 0, va("%c \"This server version (%d) of RepZIW4 has expired.\"", 104, BUILDNUMBER));
		SV_GameSendServerCommand(-1, 0, va("%c \"In exactly 30 minutes the updater will update this server, you'll be kicked in the process. v(%d)\"", 104, version));
	}

	return rsize;
}

void PatchMW2_CheckUpdate()
{
	if (IsDebuggerPresent())
		return;

	if ((Com_Milliseconds() - lastUpdateCheck) > 120000)
	{
		curl_global_init(CURL_GLOBAL_ALL);

		CURL* curl = curl_easy_init();

		if (curl)
		{
			char url[255];
			_snprintf(url, sizeof(url), "http://www.repziw4.net/content/dediiw4/version.txt");

			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, VersionReceived);
			curl_easy_setopt(curl, CURLOPT_USERAGENT, VERSIONSTRING);
			curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);

			CURLcode code = curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			curl_global_cleanup();

			if (code != CURLE_OK) {
				Com_Printf(1, "Could not reach the RepZ server.\n");
			}
		}

		curl_global_cleanup();

		lastUpdateCheck = Com_Milliseconds();
	}
	return;
}