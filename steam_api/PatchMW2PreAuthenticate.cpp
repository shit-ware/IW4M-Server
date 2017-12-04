// ==========================================================
// IW4M project
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: Patches to perform NP authentication at challenge
//          time.
//
// Initial author: NTAuthority
// Started: 2012-12-26
// ==========================================================

#include "StdInc.h"
#include "Hooking.h"
#include "PatchMW2PreAuthenticate.h"

std::vector<challengeState_t*> steamAuthed;


challengeState_t* challengeStates = (challengeState_t*)0x62B68D4;
extern int* svs_time;

typedef void(__cdecl * SV_SendSteamAuthorize_t)(netadr_t adr, NPID id);
SV_SendSteamAuthorize_t SV_SendSteamAuthorize = (SV_SendSteamAuthorize_t)0x437CE0;

std::map<NPID, __int64> steamAuth; // Idiots could spam the auth, making the server spam the np... Thats a big nono in my book

void SV_SendAuthorize(const char* guidString, challengeState_t* state)
{
	strcpy_s(state->guid, sizeof(state->guid), guidString);

	NPID serverID;
	NP_GetNPID(&serverID);
	SV_SendSteamAuthorize(state->adr, serverID);
	//NET_OutOfBandPrint(NS_SERVER, state->adr, "steamauthReq");	
}

void HandleSteamAuthResult(NPAsync<NPValidateUserTicketResult>* async)
{
	NPValidateUserTicketResult* result = async->GetResult();
	challengeState_t* state = (challengeState_t*)async->GetUserData();

	NPID guid = _strtoi64(state->guid, NULL, 16);

	steamAuthed.push_back(state);

	if (result->id != guid)
	{
		NET_OutOfBandPrint(NS_SERVER, state->adr, "error\n%s", "Auth ticket result NPID does not match challenge NPID.");

		memset(state, 0, sizeof(*state));
		return;
	}

	if (result->result == ValidateUserTicketResultOK)
	{
		state->pingTime = *svs_time;
		NET_OutOfBandPrint(NS_SERVER, state->adr, "challengeResponse %i", state->challenge);
		steamAuth[guid] = 0;
	}
	else
	{
		NET_OutOfBandPrint(NS_SERVER, state->adr, "error\n%s", "Ticket verification failed. Redownload the client from http://repziw4.de/ and try again.");

		memset(state, 0, sizeof(*state));
	}
}

typedef bool(__cdecl * NET_IsLanAddress_t)(netadr_t adr);
NET_IsLanAddress_t NET_IsLanAddress = (NET_IsLanAddress_t)0x43D6C0;

CallHook processSteamAuthHook;
DWORD processSteamAuthHookLoc = 0x6266B5;

void ProcessSteamAuthHookFunc(netadr_t adr, msg_t* msg)
{
	char ticketData[2048];
	int i;
	challengeState_t* state;

	for (i = 0; i < 1024; i++)
	{
		if (NET_CompareAdr(challengeStates[i].adr, adr))
		{
			state = &challengeStates[i];
			break;
		}
	}

	if (i == 1024)
	{
		NET_OutOfBandPrint(NS_SERVER, adr, "error\n%s", "Unknown IP for challenge.");
		return;
	}
	

	//where was the function ?
	NPID guid = _strtoi64(state->guid, NULL, 16);
	NPID npID = MSG_ReadInt64(msg);

	if (steamAuth[guid] != 0 && (Com_Milliseconds() - steamAuth[guid]) < (5 * 1000)) {
		return;
	}

	steamAuth[guid] = Com_Milliseconds();

	if (npID != guid)
	{
		memset(state, 0, sizeof(*state));

		NET_OutOfBandPrint(NS_SERVER, adr, "error\n%s", "Auth ticket NPID does not match challenge NPID.");
		return;
	}

	short ticketLength = MSG_ReadShort(msg);
	MSG_ReadData(msg, ticketData, ticketLength); // muahaha

	//Com_Printf(0, "msg: %s", msg);
	//Com_Printf(0, "ticketData: %s", ticketData);

	DWORD clientIP = (adr.ip[0] << 24) | (adr.ip[1] << 16) | (adr.ip[2] << 8) | adr.ip[3];

	if (NET_IsLanAddress(adr))
	{
		clientIP = 0;
	}

	NPAsync<NPValidateUserTicketResult>* async = NP_ValidateUserTicket(ticketData, ticketLength, clientIP, npID, "NULL");
	async->SetCallback(HandleSteamAuthResult, state);
}

StompHook getChallengeHook;
DWORD getChallengeHookLoc = 0x4B95B1;

void __declspec(naked) GetChallengeHookStub()
{
	__asm
	{
		push esi
			push edx

			call SV_SendAuthorize

			add esp, 8h

			pop edi
			pop esi
			pop ebp
			pop ebx
			add esp, 0Ch
			retn
	}
}



//Second auth layer, by iain17
//At stats receive we'll do another validation of the user. This time we have the username and other information at our disposal.
//We'll ask the np to send us the user data. Just before we do that, we set the time (Com_Milliseconds())
//We keep track of the users using the authed int array.
//Once the np replies we'll set the authed tp 1.
//Default value for each user in authed is 0 or the time we asked for the userData.
//If the np gets spammed or doesn't reply inside the time window of AUTH_TIMEOUT we'll assume something went horribly wrong.
//Because each AUTH_VALIDATE we'll go through each client and check if he went through our auth fase. Either the auth for that client number should be 
//0 = not connected
//1 = authed
//if when we auth validate: (Com_Milliseconds() - Com_Milliseconds() at stats received) > AUTH_TIMEOUT we can kick him. He missed our validate window.
//This way we can issue more control on who joins our servers. ProcessSteamAuthHookFunc wasn't enough since we didn't have the user data

#define	AUTH_TIMEOUT	15*1000
#define	AUTH_VALIDATE	15*1000
static __int64 authed[MAX_CLIENTS];
ClientAuth* authedData[MAX_CLIENTS];
static int lastCheck = 0;

//Np responds with the userData.
void authResult(NPAsync<NPValidateUserTicketResult>* async) {

	NPValidateUserTicketResult* result = async->GetResult();
	ClientAuth* authingClient = (ClientAuth*)async->GetUserData();

	if (result->result == ValidateUserTicketResultInvalid || result->id != authingClient->steamid) {

		authed[authingClient->clientNum] = 0;
		char buff[2048];
		NET_OutOfBandPrint(NS_SERVER, authingClient->adr, "Ticket verification failed (1). Redownload the client from http://repziw4.de/ and try again.");
		sprintf(buff, "clientkick %i '%s'", authingClient->clientNum, "Ticket verification failed (1). Redownload the client from http://repziw4.de/ and try again.");
		Cmd_ExecuteSingleCommand(0, 0, buff);
		return;

	}

	//Check if prestige is just the same as the connectioninfostring
	if (authingClient->prestige != result->Presige) {
		
		authed[authingClient->clientNum] = 0;
		char buff[2048];
		NET_OutOfBandPrint(NS_SERVER, authingClient->adr, "Ticket verification failed (8). Redownload the client from http://repziw4.de/ and try again.");
		sprintf(buff, "clientkick %i '%s'", authingClient->clientNum, "Ticket verification failed (8). Redownload the client from http://repziw4.de/ and try again.");
		Cmd_ExecuteSingleCommand(0, 0, buff);
		return;
	}
	
	//Passed all checks, lets set this user to authed.
	Com_Printf(0, "[RepZ Auth] %s:%d (G:%d) (P:%d) authenticated.\n", authingClient->name, result->id, result->groupID, result->Presige);
	authed[authingClient->clientNum] = 1;
	authingClient->prestige = result->Presige;
	authedData[authingClient->clientNum] = authingClient;

}

//Stats received. Issue the seconds auth request.
void hookStatsReceived() {

	BYTE* clientAddress = (BYTE*)svs_clients;

	for (int i = 0; i < *svs_numclients; i++) {

		int clientNum = CLIENTNUM(clientAddress);

		if (svs_clients[i].state == 3) {
			
			DWORD clientIP = (svs_clients[i].adr.ip[0] << 24) | (svs_clients[i].adr.ip[1] << 16) | (svs_clients[i].adr.ip[2] << 8) | svs_clients[i].adr.ip[3];

			if (NET_IsLanAddress(svs_clients[i].adr))
			{
				clientIP = 0;
			}

			if (authed[clientNum] == 0) {

				authed[clientNum] = Com_Milliseconds();

				ClientAuth* authingClient = new ClientAuth();
				authingClient->clientNum = clientNum;
				authingClient->name = svs_clients[i].name;
				authingClient->prestige = atoi(Info_ValueForKey(svs_clients[i].connectInfoString, "prestige"));//We'll be validating this at np ticket result
				authingClient->adr = svs_clients[i].adr;
				authingClient->steamid = svs_clients[i].steamid;

				//Check if user passed steam auth
				if (steamAuthed.empty() == false) {
					for (int i = steamAuthed.size() - 1; i >= 0; i--)
					{
						if (_strtoi64(steamAuthed.at(i)->guid, NULL, 16) == authingClient->steamid) {
							authingClient->steamAuth = steamAuthed.at(i);
							steamAuthed.erase(steamAuthed.begin()+i);
							break;
						}
					}
				}

				if (authingClient->steamAuth == NULL) {
					char buff[2048];
					sprintf(buff, "clientkick %i '%s'", authingClient->clientNum, "Ticket verification failed (0). Redownload the client from http://repziw4.de/ and try again.");
					Cmd_ExecuteSingleCommand(0, 0, buff);
					authed[authingClient->clientNum] = 0;
					continue;
				}

				//Request second auth layer
				NPAsync<NPValidateUserTicketResult>* async = NP_ValidateUserTicket("nothing here whatsoever", 2048, clientIP, svs_clients[i].steamid, svs_clients[i].name);
				async->SetCallback(authResult, authingClient);

			}
			else {

				if ((Com_Milliseconds() - lastCheck) > AUTH_VALIDATE)
				{
					if (*clientAddress >= 5) {

						//So the user is connected, not authed and passed the auth window. Only happens if the np doesn't reply for some reason.
						if (svs_clients[i].steamid != 0 && authed[clientNum] != 1 && (Com_Milliseconds() - authed[clientNum]) > AUTH_TIMEOUT) {

							char buff[2048];
							sprintf(buff, "clientkick %i '%s'", clientNum, "Ticket verification failed (4). Redownload the client from http://repziw4.de/ and try again.");
							Cmd_ExecuteSingleCommand(0, 0, buff);
							authed[clientNum] = 0;
							continue;
						}

						//Snapnum keeps track how longer the dude has been connected. If it appears he's been connected long enough and we still don't have confirmation kick him
						if (svs_clients[i].steamid != 0 && svs_clients[0].snapNum > 153599 && authed[clientNum] != 1) {
							char buff[2048];
							sprintf(buff, "clientkick %i '%s'", clientNum, "Ticket verification failed (3). Redownload the client from http://repziw4.de/ and try again.");
							Cmd_ExecuteSingleCommand(0, 0, buff);
							authed[clientNum] = 0;
							continue;
						}

						//Check the users details.
						if (authed[clientNum] == 1 || svs_clients[0].snapNum > 153599) {

							//Username mismatch
							if (authedData[clientNum]->name != svs_clients[i].name) {
								char buff[2048];
								sprintf(buff, "clientkick %i '%s'", clientNum, "Ticket verification failed (5). Redownload the client from http://repziw4.de/ and try again.");
								Cmd_ExecuteSingleCommand(0, 0, buff);
								authed[clientNum] = 0;
								continue;
							}

							//Presige mismatch
							if (authedData[clientNum]->prestige != atoi(Info_ValueForKey(svs_clients[i].connectInfoString, "prestige"))) {
								char buff[2048];
								sprintf(buff, "clientkick %i '%s'", clientNum, "Ticket verification failed (6). Redownload the client from http://repziw4.de/ and try again.");
								Cmd_ExecuteSingleCommand(0, 0, buff);
								authed[clientNum] = 0;
								continue;
							}

							//NPID (steamid) mismatch
							if (authedData[clientNum]->steamid != (NPID)(svs_clients[i].steamid)) {
								char buff[2048];
								sprintf(buff, "clientkick %i '%s'", clientNum, "Ticket verification failed (7). Redownload the client from http://repziw4.de/ and try again.");
								Cmd_ExecuteSingleCommand(0, 0, buff);
								authed[clientNum] = 0;
								continue;
							}
						}

					}
				}

			}

		}

		clientAddress += 681872;
	}

	if ((Com_Milliseconds() - lastCheck) > AUTH_VALIDATE)
		lastCheck = Com_Milliseconds();

}

//Client disconnected. Set auth to 0.
void Auth_OnDisconnect(int clientNum)
{
	authed[clientNum] = 0;
	//Com_Printf(0, "Auth_OnDisconnect: %i", clientNum);
}

void PatchMW2_PreAuthenticate()
{
	//Hook on player stats received. Every player must send his stats and at that point we'll have all the userData we might want to validate in the second auth layer.
	DWORD call_bytes;
	call_bytes = (DWORD)(&hookStatsReceived) - (0x4320B8 + 5);

	*(BYTE*)0x4320A0 = 0xEB; //jmp
	*(BYTE*)0x4320A1 = 0x15; //short iw4m...
	*(BYTE*)0x4320B7 = 0x5B; //POP EBX
	*(BYTE*)0x4320B8 = 0xE8; //CALL
	*(DWORD*)0x4320B9 = call_bytes; ///part of add lets see if the bytes are correct now
	*(BYTE*)0x4320BD = 0xC3; //RETN

	processSteamAuthHook.initialize(processSteamAuthHookLoc, ProcessSteamAuthHookFunc);
	processSteamAuthHook.installHook();

	getChallengeHook.initialize(getChallengeHookLoc, GetChallengeHookStub);
	getChallengeHook.installHook();

	// disable Steam auth
	*(BYTE*)0x4663A8 = 0xEB;
}