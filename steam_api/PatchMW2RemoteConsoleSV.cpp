// ==========================================================
// IW4M project
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: Modern Warfare 2 patches: remote console support
//          (server-side component)
//
// Initial author: NTAuthority
// Started: 2011-05-21 (copied from PatchMW2Status.cpp)
// ==========================================================

#include "StdInc.h"
#include <algorithm>

dvar_t* sv_rconPassword;
netadr_t redirectAddress;

void SV_FlushRedirect(char *outputbuf) {
	NET_OutOfBandPrint(1, redirectAddress, "print\n%s", outputbuf);
}

#define SV_OUTPUTBUF_LENGTH ( 16384 - 16 )
char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

extern DWORD* cmd_id_sv;
extern dvar_t* rcon_ip;
std::map<std::string, int> record;
bool warningGiven = false;

void SVC_RemoteCommand(netadr_t from, void* msg)
{
	bool valid = false;
	unsigned int time = 0;
	char remaining[1024] = { 0 };
	size_t current = 0;
	static unsigned int invalidLastTime = 0;
	static unsigned int validLastTime = 0;
	netadr_t _from = from;
	netadr_t rcon_ip_addr;
	_from.port = 0;
	std::string host = NET_AdrToString(_from);

	//check ip is the same as our sv_rconIp
	if (strlen(rcon_ip->current.string)) {

		NET_StringToAdr(rcon_ip->current.string, &rcon_ip_addr);
		rcon_ip_addr.port = 0;
		
		if (!NET_CompareAdr(_from, rcon_ip_addr)) {
			if (!warningGiven) {
				Com_Printf(1, "Bad rcon: \"%s\" may not issue rcon commands. rcon_ip in a effect.\n%s\n", NET_AdrToString(from), Cmd_Argv(2));
				warningGiven = true;
			}
			return;
		}
	
	}

	//check if this host doesn't have a record of bad account higher than > 10
	if (record[host] >= 10)
		return;

	record[host]++;

	if (!strlen(sv_rconPassword->current.string)) {
		Com_Printf(1, "The server must set 'rcon_password' for clients to use 'rcon'.\n");
		return;
	}

	if (strlen(sv_rconPassword->current.string) && !strcmp(Cmd_Argv(1), sv_rconPassword->current.string)) {

		Com_Printf(1, "Rcon from %s:\n%s\n", NET_AdrToString(from), Cmd_Argv(2));
		if (record[host] > 0)
			record[host]--;
	
	} else {

		//Max 1 invalid commands per second
		time = Com_Milliseconds();
		if (time < (invalidLastTime + 10))
			return;
		invalidLastTime = time;
		Com_Printf(1, "Bad rcon from %s:\n%s\n", NET_AdrToString(from), Cmd_Argv(2));
		return;
	
	}

	// start redirecting all print outputs to the packet
	redirectAddress = from;
	Com_BeginRedirect(sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

	remaining[0] = 0;

	if (Cmd_Argc() > 2)
	{
		for (int i = 2; i < Cmd_Argc(); i++)
		{
			current = Com_AddToString(Cmd_Argv(i), remaining, current, sizeof(remaining), true);
			current = Com_AddToString(" ", remaining, current, sizeof(remaining), false);
		}
	}
	else
	{
		memset(remaining, 0, sizeof(remaining));
		strncpy(remaining, Cmd_Argv(2), sizeof(remaining)-1);
	}

	Cmd_ExecuteSingleCommand(0, 0, remaining);

	Com_EndRedirect();

	if (strlen(remaining) > 0)
	{
		Com_Printf(0, "handled rcon: %s\n", remaining);
	}
}