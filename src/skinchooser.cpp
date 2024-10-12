
#include "skinchooser.h"
#include "gamesystem.h"
#include "commands.h"
#include "playermanager.h"
#include "ctimer.h"
#include <fstream>

#undef snprintf
#include "vendor/nlohmann/json.hpp"

using ordered_json = nlohmann::ordered_json;

struct PlayerModelEntry
{
	std::string szName;
	std::string szModelPath;
	std::string szSteamId;
};

CUtlVector<PlayerModelEntry*> g_vecModelEntries;
void LoadSkinsData();

bool g_bSkinChooserEnable = true;
FAKE_BOOL_CVAR(cs2f_skinchooser_enable, "Allow skinchooser to players", g_bSkinChooserEnable, true, false)

CON_COMMAND_F(cs2f_skinchooser_reload, "Reload Skinchooser", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY | FCVAR_PROTECTED)
{
	LoadSkinsData();
	Message("Reloaded Skinchooser config.\n");
}

bool SetPlayerSkin(CCSPlayerController* pController, const char* pszSkin)
{
	FOR_EACH_VEC(g_vecModelEntries, i)
	{
		if (V_stricmp(g_vecModelEntries[i]->szName.c_str(), pszSkin) == 0)
		{
			ZEPlayer* pZEPlayer = pController->GetZEPlayer();
			if (!g_vecModelEntries[i]->szSteamId.empty() &&
				(g_vecModelEntries[i]->szSteamId.find(std::to_string(pZEPlayer->GetSteamId64())) == std::string::npos && !pZEPlayer->IsAdminFlagSet(ADMFLAG_GENERIC)))
			{
				ClientPrint(pController, HUD_PRINTTALK, SKINCHOOSER_PREFIX "You can not use private skin!");
				return false;
			}

			ClientPrint(pController, HUD_PRINTTALK, SKINCHOOSER_PREFIX "Changed to %s", g_vecModelEntries[i]->szName.c_str());
			pController->GetPlayerPawn()->SetModel(g_vecModelEntries[i]->szModelPath.c_str());
			pZEPlayer->SetSkinPreference(g_vecModelEntries[i]->szName.c_str());
			return true;
		}
	}

	return false;
}

void SkinChooser_Init()
{

}

void SkinChooser_Precache(IEntityResourceManifest* pResourceManifest)
{
	LoadSkinsData();

	FOR_EACH_VEC(g_vecModelEntries, i)
	{
		pResourceManifest->AddResource(g_vecModelEntries[i]->szModelPath.c_str());
	}
}

void SkinChooser_OnPlayerSpawn(CCSPlayerController* pController)
{
	if (!g_bSkinChooserEnable)
		return;

	CHandle<CCSPlayerController> handle = pController->GetHandle();
	new CTimer(0.05f, false, false, [handle]()
	{
		CCSPlayerController* pController = (CCSPlayerController*)handle.Get();
		if (!pController)
			return -1.0f;

		if (!pController->IsAlive())
			return -1.0f;

		const char* pszSkinPreference = pController->GetZEPlayer()->GetSkinPreference();
		if (pszSkinPreference[0] != '\0')
			SetPlayerSkin(pController, pszSkinPreference);

		return -1.0f;
	});
}

void LoadSkinsData()
{
	g_vecModelEntries.PurgeAndDeleteElements();

	const char* pszJsonPath = "addons/cs2fixes/configs/PlayerSkins.json";
	char szPath[MAX_PATH];
	V_snprintf(szPath, sizeof(szPath), "%s%s%s", Plat_GetGameDirectory(), "/csgo/", pszJsonPath);
	std::ifstream jsoncFile(szPath);

	if (!jsoncFile.is_open())
	{
		Message("Failed to open %s\n", pszJsonPath);
		return;
	}

	std::set<std::string> setClassNames;
	ordered_json jsonPlayerClasses = ordered_json::parse(jsoncFile, nullptr, true, true);

	for (auto& [szSkins, jsonSkinClasses] : jsonPlayerClasses.items())
	{
		for (auto& [szClassName, jsonClass] : jsonSkinClasses.items())
		{
			PlayerModelEntry *entry = new PlayerModelEntry();
			entry->szName = jsonClass.value("name", "");
			entry->szModelPath = jsonClass.value("ModelCT", "");
			entry->szSteamId = jsonClass.value("steamid", "");

			g_vecModelEntries.AddToTail(entry);
		}
	}
}

CON_COMMAND_CHAT(skin, "- change player skin")
{
	if (!player)
		return;

	if (!g_bSkinChooserEnable)
		return;

	if (SetPlayerSkin(player, args[1]))
		player->GetZEPlayer()->SetSkinPreference(args[1]);
	else
		ClientPrint(player, HUD_PRINTTALK, SKINCHOOSER_PREFIX "Failed to set skin %s", args[1]);
}
