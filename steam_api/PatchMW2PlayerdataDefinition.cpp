// ==========================================================
// IW4M project
// 
// Component: clientdll
// Sub-component: steam_api
// Purpose: Modify player data
//
// Initial author: momo5502
// Started: 2014-02-11
// ==========================================================

#include "StdInc.h"

struct newWeapon
{
	char* name;
	int securityIndex;
};

std::map<int, newWeapon*> newWeaps;
structuredDataEnumIndex_t* newIndices;
int newIndexCount;

void addWeapon(char* weaponName, int securityIndexOffset)
{
	newWeapon* newWeap = (newWeapon*)malloc(sizeof(newWeapon));
	newWeap->name = weaponName;
	newWeap->securityIndex = securityIndexOffset;
	newWeaps[newWeaps.size()] = newWeap;
}

char* strToLower(char* str)
{
	int len = strlen(str);
	char* newStr = (char*)malloc(len + 1);
	memset(newStr, 0, len + 1);

	for (int i = 0; i < len; i++)
	{
		newStr[i] = tolower(str[i]);
	}

	return newStr;
}

// Tells if first entry is earlier in alphabet or not
bool checkAlphabeticalOrder(char* entry1, char* entry2)
{
	char* e1_l = strToLower(entry1);
	char* e2_l = strToLower(entry2);

	int len = min(strlen(e1_l), strlen(e2_l));

	for (int i = 0; i < len; i++)
	{
		if (*(e1_l + i) > *(e2_l + i))
			return false;

		else if (*(e1_l + i) < *(e2_l + i))
			return true;
	}

	return (strlen(e1_l) <= strlen(e2_l));
}

void patchPlayerData(structuredDataDef_t* data)
{
	structuredDataEnum_t* weaponEnum = &data->data->enums[1];

	// Check if new enum has already been built
	if (newIndices)
	{
		weaponEnum->numIndices = newIndexCount;
		weaponEnum->indices = newIndices;
		return;
	}

	// Define new amount of indices
	newIndexCount = weaponEnum->numIndices + newWeaps.size();

	// Find last cac index
	int lastCacIndex = 0;
	for (int i = 0; i < weaponEnum->numIndices; i++)
	{
		if (weaponEnum->indices[i].index > lastCacIndex)
			lastCacIndex = weaponEnum->indices[i].index;
	}

	// Create new enum
	newIndices = (structuredDataEnumIndex_t*)malloc(sizeof(structuredDataEnumIndex_t)* (weaponEnum->numIndices + newWeaps.size()));
	memset(newIndices, 0, sizeof(structuredDataEnumIndex_t)* (weaponEnum->numIndices + newWeaps.size()));
	memcpy(newIndices, weaponEnum->indices, sizeof(structuredDataEnumIndex_t)* weaponEnum->numIndices);

	// Apply new weapons to enum
	for (int i = 0; i < newWeaps.size(); i++)
	{
		int weaponPos = 0;

		for (; weaponPos < weaponEnum->numIndices + i; weaponPos++)
		{
			// Search weapon position
			if (checkAlphabeticalOrder(newWeaps[i]->name, (char*)newIndices[weaponPos].key))
				break;
		}

		for (int j = weaponEnum->numIndices + i; j > weaponPos; j--)
		{
			newIndices[j] = newIndices[j - 1];
		}

		newIndices[weaponPos].index = newWeaps[i]->securityIndex + lastCacIndex;
		newIndices[weaponPos].key = newWeaps[i]->name;
	}

	// Apply stuff to current player data
	weaponEnum->numIndices = newIndexCount;
	weaponEnum->indices = newIndices;
}

structuredDataDef_t* FindStructuredDataHookFunc(assetType_t type, const char* name)
{
	structuredDataDef_t* data = (structuredDataDef_t*)DB_FindXAssetHeader(type, name);

	if (!strcmp(name, "mp/playerdata.def"))
	{
		patchPlayerData(data);
	}

	return data;
}


void PatchMW2_PlayerdataDefinition()
{

    // Note that the number (second param) is just for security of index preservation
	// Increment it every time you add a new weapon
	// e.g. next one could be 'addWeapon("peacekeeper", 2);'
	// Otherwise it will fuck up player stats
	//When we add more weapons crash's allways client


	addWeapon("g36c", 1);
	addWeapon("peacekeeper", 2);
	addWeapon("m40a3", 3);
	addWeapon("remington700", 4);
	addWeapon("skorpion", 5);
	addWeapon("dragunov", 6);
	addWeapon("w1200", 7);
	addWeapon("thompson", 8);
	
	call(0x6464E7, FindStructuredDataHookFunc, PATCH_CALL);
}