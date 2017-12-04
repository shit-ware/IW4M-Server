     // ==========================================================
	 // IW4M project
	 //
	 // Component: clientdll
	 // Sub-component: steam_api
	 // Purpose: support code for the main menu news ticker
	 //
	 // Initial author: NTAuthority
	 // Started: 2012-04-19
	 // ==========================================================

#include "StdInc.h"

#define CURL_STATICLIB
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

	 CallHook newsTickerGetTextHook;
DWORD newsTickerGetTextHookLoc = 0x6388C1;
size_t AuthDataReceived(void *ptr, size_t size, size_t nmemb, void *data);

char* Auth_GetUsername();
char ticker[8192];


const char* NewsTicker_GetText(const char* garbage)
{
	if (!ticker[0])
	{
		char buf[8192] = { 0 };

		curl_global_init(CURL_GLOBAL_ALL);
		CURL* curl = curl_easy_init();

		curl_easy_setopt(curl, CURLOPT_URL, "http://repz.eu/news/news.txt");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AuthDataReceived);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buf);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "IW4M");
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);

		CURLcode code = curl_easy_perform(curl);

		curl_easy_cleanup(curl);
		curl_global_cleanup();

		if (code == CURLE_OK)
			strcpy(ticker, va("%s", buf));
		else
			strcpy(ticker, va("^1Failed to retrieve news! Error: %s", code));
	}
	return ticker;
}

void PatchMW2_NewsTicker()
{
	// hook for getting the news ticker string
	*(WORD*)0x6388BB = 0x9090; // skip the "if (item->text[0] == '@')" localize check

	// replace the localize function
	newsTickerGetTextHook.initialize(newsTickerGetTextHookLoc, NewsTicker_GetText);
	newsTickerGetTextHook.installHook();

	// make newsfeed (ticker) menu items not cut off based on safe area
	memset((void*)0x63892D, 0x90, 5);
}