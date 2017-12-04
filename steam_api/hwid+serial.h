#include <string>
#include <sstream>
#include "md5.h"
using namespace std;

string get_hwid()
{
	HW_PROFILE_INFOA hwProfileInfo;

	if (GetCurrentHwProfile(&hwProfileInfo) != NULL)
	{
		return hwProfileInfo.szHwProfileGuid;
	}

	return 0;
}

DWORD get_volume_serial()
{
	DWORD serial = 0;
	GetVolumeInformationA("C:\\", 0, 0, &serial, 0, 0, 0, 0);
	return serial;
}