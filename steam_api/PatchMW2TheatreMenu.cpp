// ==========================================================
// IW4M project
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: Menu handler for Theatre Mode menu
//
// Initial author: Apadayo
// Started: 2012-04-30
// ==========================================================

#include "StdInc.h"
#include "PatchMW2UIScripts.h"
#include <stdio.h>
#include <vector>
#include <Windows.h>
#include <sys/stat.h>

extern char* UI_LocalizeMapName(char* mapName);
extern char* UI_LocalizeGameType(char* gameType);

UIFeeder_t TheatreMenu_Feeder;
UIFeeder_t MapList_Feeder;

typedef struct demoInfo_s
{
	char demoName[32];
	char mapName[32];
	char gameType[10];
	char length[5]; //can it be longer???
} demoInfo_t;

typedef struct uiDemoInfo_s
{
	int numDemos;
	int curDemo;
	std::vector<demoInfo_t>demos;
} uiDemoInfo_t;

uiDemoInfo_t uiDemoInfo;

bool fexist( char *filename ) {
	struct stat buffer ;
	if ( stat( filename, &buffer ) ) return true ;
	return false ;
}

const char* milliToString( int milliseconds )
{
	int minutes = (milliseconds % (1000*60*60)) / (1000*60);
    int seconds = ((milliseconds % (1000*60*60)) % (1000*60)) / 1000;
	return va("%d:%02d", minutes, seconds);
}

int TheatreMenu_GetItemCount()
{
	return uiDemoInfo.numDemos;
}

void TheatreMenu_Select(int index)
{
	Cmd_ExecuteSingleCommand(0, 0, va("set ui_demo_mapname %s", uiDemoInfo.demos.at(index).mapName));
	Cmd_ExecuteSingleCommand(0, 0, va("set ui_demo_mapname_localized %s", UI_LocalizeMapName(uiDemoInfo.demos.at(index).mapName)));
	Cmd_ExecuteSingleCommand(0, 0, va("set ui_demo_gametype %s", UI_LocalizeGameType(uiDemoInfo.demos.at(index).gameType)));
	Cmd_ExecuteSingleCommand(0, 0, va("set ui_demo_length %s", uiDemoInfo.demos.at(index).length));
	uiDemoInfo.curDemo = index;
}

char* TheatreMenu_GetItemText(int index, int column)
{
	if( index >= 0 && index < uiDemoInfo.numDemos)
	{
		switch(column)
		{
		case 0:
			return uiDemoInfo.demos.at(index).demoName;
			break;
		case 1:
			return UI_LocalizeMapName(uiDemoInfo.demos.at(index).mapName);
			break;
		}
	}
	return "";
}


void TheatreMenu_UIScript_LoadDemos(char* args)
{
	// clear old stuff
	uiDemoInfo.demos.clear();
	uiDemoInfo.numDemos = 0;

	// load new stuff
	demoInfo_t tmp;
	int filenum = 0;
	char** list = 0;
	list = FS_ListFiles("demos/", "dm_13", 0, &filenum);
	FILE * meta;
	for(int i=0; i < filenum; i++)
	{
		int len = strlen (list[i]);
        if (len >= 6) {
            if (!strstr(list[i],"demofix_")) {
                strncpy(tmp.demoName, list[i], 32);
				tmp.demoName[len - 6] = 0; // null terminate it early to get rid of extension
				meta = fopen(va(".\\players\\demos\\%s.meta", tmp.demoName),"r");
				if(meta != NULL) {
					fscanf(meta,"%s%s%s",tmp.mapName, tmp.gameType, tmp.length); // read meta
					fclose(meta);
					uiDemoInfo.demos.push_back(tmp);
					uiDemoInfo.numDemos++;
					Com_Printf(0, "Added demo of name %s, map %s, gametype %s, and length %s\n", tmp.demoName, tmp.mapName, tmp.gameType, tmp.length);
				} else { Com_Printf(0, va("Couldn't load meta file for %s... not listing in theatre.\n", tmp.demoName)); }
			}
		}
	if(uiDemoInfo.numDemos > 0) TheatreMenu_Select(0); // update ui if there are any entries
	}
}

void TheatreMenu_UIScript_FixDemoServer(char * args)
{
	Cmd_ExecuteSingleCommand(0, 0, "devmap mp_rust");
	Cmd_ExecuteSingleCommand(0, 0, "disconnect");
	Cmd_ExecuteSingleCommand(0, 0, "openmenu pc_theatre_menu");
}

void TheatreMenu_UIScript_LaunchDemo(char * args)
{
	demoInfo_t info = uiDemoInfo.demos.at(uiDemoInfo.curDemo);
	Cmd_ExecuteSingleCommand(0, 0, va("demo demofix_%s", info.mapName));
	Cmd_ExecuteSingleCommand(0, 0, "wait 30");
	Cmd_ExecuteSingleCommand(0, 0, va("demo %s", info.demoName));
	//Cmd_ExecuteSingleCommand(0, 0, "timescale 1.025");
}

cmd_function_t customRecord;
cmd_function_t customStopRecord;

FILE * metaFile;
bool recording = false;
int startTime;

char curDemoName[255];
void customRecordFunc()
{
	dvar_t * map = Dvar_FindVar("mapname");
	memset(curDemoName, 0, 255);
	if(!recording)
	{
		if(strlen(map->current.string) == 0) { Com_Printf(0, "You must be in a level to record.\n"); return; }
		recording = true;

		if(Cmd_Argc() == 2) {
			strncpy(curDemoName, Cmd_Argv(1), 255);
			if(!fexist((char*)va(".\\players\\demos\\%s.dm_13", curDemoName))) { 
				Com_Printf(0, "Demo with that name alreready exists.\n");
				recording = false;
				return; 
			}
		} else {
			for(int i=0; i<9999; i++)
			{
				if(fexist((char*)va(".\\players\\demos\\demo%04d.dm_13", i))) {
					strcpy(curDemoName, va("demo%04d", i));
					break;
				}
			}
		}
		startTime = Com_Milliseconds();
		Cmd_ExecuteSingleCommand(0, 0, va("oldrec %s", curDemoName));
	} else { Com_Printf(0, "Already recording a demo!\n"); }
}

void customStopRecordFunc()
{
	if(recording) {
		Cmd_ExecuteSingleCommand(0, 0, "oldstoprec");
		recording = false;
		dvar_t * map = Dvar_FindVar("mapname");
		dvar_t * gametype = Dvar_FindVar("g_gametype");
		metaFile = fopen(va(".\\players\\demos\\%s.meta", curDemoName),"w");
		if(metaFile == NULL) { 
			Com_Printf(0, "Could not create demo meta file!\n"); 
			return;
		}
		fprintf(metaFile, "%s\n", map->current.string);
		fprintf(metaFile, "%s\n", gametype->current.string);
		fprintf(metaFile, "%s\n", milliToString(Com_Milliseconds()-startTime));
		fclose(metaFile);
		bool copy = CopyFile(va(".\\m2demo\\demos\\%s.dm_13", curDemoName), va(".\\players\\demos\\%s.dm_13", curDemoName), true);
		if(copy > 0)
			DeleteFile(va(".\\m2demo\\demos\\%s.dm_13", curDemoName));
		else
			Com_Printf(1, "Failed to copy demo file to players folder... is it readonly?\n");

	} else { Com_Printf(0, "Not recording a demo.\n"); }
}

// patch function
void PatchMW2_TheatreMenu()
{
	TheatreMenu_Feeder.feeder = 10.0f;
	TheatreMenu_Feeder.GetItemCount = TheatreMenu_GetItemCount;
	TheatreMenu_Feeder.GetItemText = TheatreMenu_GetItemText;
	TheatreMenu_Feeder.Select = TheatreMenu_Select;

	UIFeeders.push_back(TheatreMenu_Feeder);

	AddUIScript("loadDemos", TheatreMenu_UIScript_LoadDemos);
	AddUIScript("FixDemoServer", TheatreMenu_UIScript_FixDemoServer);
	AddUIScript("LaunchDemo", TheatreMenu_UIScript_LaunchDemo);

	strcpy((char*)(0x6FB620), "oldrec");
	strcpy((char*)(0x708238), "oldstoprec"); 

	Cmd_AddCommand("record", customRecordFunc, &customRecord, 0);
	Cmd_AddCommand("stoprecord", customStopRecordFunc, &customStopRecord, 0);
}