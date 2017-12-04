#pragma once
void Auth_OnDisconnect(int clientNum);

typedef struct
{
	netadr_t adr;
	int challenge;
	int time1;
	int pingTime;
	int time2;
	int pad;
	int pad2;
	char guid[20];
} challengeState_t;

struct ClientAuth
{
	const char* name;
	int clientNum;
	int prestige;
	netadr_t adr;
	NPID steamid;
	challengeState_t* steamAuth;
};