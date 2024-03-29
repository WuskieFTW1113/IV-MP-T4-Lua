/*
This is an open source project developed by the IV:MP team (www.iv-mp.eu)
*/

#include <exporting.h>
#include "apiLua.h"

#if linux
#include <unistd.h>
#define mySleep2(x) (usleep(x * 1000))
#else
#include <Windows.h>
#define mySleep2(x) (Sleep(x))
#endif

#include "mainNetwork.h"
#include "events.h"
#include "apiPlayerEvents.h"
#include <time.h>
#include <fstream>
#include "apiParamHelper.h"
#include "easylogging++.h"
#include "apiWorld.h"
#include "apiNpc.h"
#include "apiPlayerInputs.h"
#include <iostream>
#include <map>
#include <string>
#include "apiDebugTask.h"

struct configFile
{
	std::string fName;
	bool loaded;
	std::map<std::string, std::string> v;

	configFile()
	{
		fName = "config.cfg";
		v["name"] = "Unamed server", v["port"] = "8888", v["masterlist"] = "0", v["refreshrate"] = "100",
			v["location"] = "", v["website"] = "", v["EFLC"] = "0", v["auto_blip_tags"] = "1", v["gen_crash_dump"] = "1";
		loaded = false;
	}
	void open()
	{
		std::ifstream fileOpen(fName);
		if(fileOpen.is_open())
		{
			mlog("Parsing %s", fName.c_str());
			std::string line;
			while(std::getline(fileOpen, line))
			{
				size_t find = line.find("=");
				if(find != std::string::npos)
				{
					v[line.substr(0, find)] = line.substr(find + 1, line.size());
				}
			}
			loaded = true;
		}
		fileOpen.close();
	}
};

void runLuaServer()
{
	configFile* c = new configFile();
	c->open();
	if(!c->loaded)
	{
		mlog("Error: config.cfg did not open");
		return;
	}

	unsigned int refresh = apiParamHelper::isUnsignedInt(c->v["refreshrate"].c_str(), false);
	if(refresh < 50) refresh = 50;
	else if(refresh > 300) refresh = 300;

	//LINFO << "Starting " << c->v["name"] << " on port " << apiParamHelper::isUnsignedInt(c->v["port"].c_str(), false);
	mlog("Starting %s on port %u", c->v["name"].c_str(), apiParamHelper::isUnsignedInt(c->v["port"].c_str(), false));

#if linux
#else
	SetErrorMode(GetErrorMode () | SEM_NOGPFAULTERRORBOX);
	if(c->v["gen_crash_dump"] == std::string("1")) apiDebugTask::setDump();
#endif

	if(initRaknet(apiParamHelper::isUnsignedInt(c->v["port"].c_str(), false), c->v["name"].c_str(), c->v["location"].c_str(),
		c->v["website"].c_str(), apiParamHelper::isInt(c->v["masterlist"].c_str()) == 1, apiParamHelper::isInt(c->v["EFLC"].c_str()) != 1)) {

		apiPlayer::setDefaultServerBlips(apiParamHelper::isInt(c->v["auto_blip_tags"].c_str()) == 1);

		delete c;
		apiLua::createVm();

#if linux
#else
		LoadLibraryA("Lua Dll.dll");
		HMODULE hModLua = GetModuleHandleA("Lua Dll.dll");
		if(hModLua != NULL)
		{
			printf("Module found\n");
			typedef void(*mInit)(lua_State* state);
			mInit fn = (mInit)GetProcAddress(hModLua, "Plugin_Init");
			if(fn != NULL)
			{
				printf("Calling\n");
				fn(apiLua::getVm());
			}
			printf("Done\n");
		}
#endif
		apiWorld::createWorld(1, 1, 12, 0, 2000);

		apiPlayerEvents::registerEnteredVehicle(events::onPlayerEnteredVehicle);
		apiPlayerEvents::registerEnteringVehicle(events::onPlayerRequestVehicleEntry);
		apiPlayerEvents::registerLeftVehicle(events::onPlayerExitVehicle);

		apiPlayerEvents::registerPlayerAfk(events::onPlayerAfk);
		apiPlayerEvents::registerPlayerJoin(events::onPlayerConnect);
		apiPlayerEvents::registerPlayerLeft(events::onPlayerLeft);
		apiPlayerEvents::registerPlayerCredentials(events::onPlayerCredentials);

		apiPlayerEvents::registerPlayerDeath(events::onPlayerDeath);
		apiPlayerEvents::registerPlayerRespawn(events::onPlayerSpawn);
		apiPlayerEvents::registerPlayerArmor(events::onPlayerArmourChange);
		apiPlayerEvents::registerPlayerHpChange(events::onPlayerHpChange);

		apiPlayerEvents::registerPlayerEnterCP(events::onPlayerEnterCheckPoint);
		apiPlayerEvents::registerPlayerExitCP(events::onPlayerExitCheckPoint);

		apiPlayerEvents::registerPlayerChat(events::onPlayerChat);
		apiPlayerEvents::registerPlayerChatUpdated(events::onChatUpdate);

		apiPlayerEvents::registerPlayerWeaponChanged(events::onPlayerSwapWeapons);
		apiPlayerEvents::registerPlayerWeaponsArrived(events::onPlayerWeaponsArrived);

		apiPlayerEvents::registerPlayerDialogListResponse(events::onPlayerDialogResponse);

		apiPlayerEvents::registerPlayerCheckSum(events::onPlayerCheckSumArrive);
		apiPlayerEvents::registerPlayerHackCheck(events::onPlayerHacks);
		apiPlayerInputs::registerKeyInputs(events::onKeyPress);
		apiPlayerEvents::registerTeamSpeak(events::onTeamSpeak);
		apiNpc::registerTasksFinished(events::npcStopped);
		apiPlayerEvents::registerPlayerSpawnEntity(events::onPlayerSpawnEntity);

		clock_t cTime = 0;
		while(true)
		{
			pulseServer();

			cTime = clock();
			apiLua::pulseVm(cTime);

			mySleep2(refresh);
		}
	}
}

int main(int argc, char *argv[])
{
	log_set_fp();
	runLuaServer();
	return 1;
}

