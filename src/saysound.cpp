
#include "entity/ccsplayercontroller.h"
#include "recipientfilters.h"
#include "commands.h"
#include "saysound.h"
#include <regex>

KeyValues *g_pSaySoundsKV;

extern CGlobalVars* gpGlobals;

// I know this is workaround and annoying, but IEngineSound doesn't implemented yet on HL2SDK
void EmitSoundToAll(const char* pszSound)
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController || !pController->IsConnected() || pController->IsBot())
			continue;

		CSingleRecipientFilter filter(pController->GetPlayerSlot());
		pController->StopSound(pszSound);
		pController->EmitSoundFilter(filter, pszSound, 1.0f);
	}
}

void SaySound_Init()
{
	// KeyValues::AutoDelete might more better, but that causes reads values wrong, idk why
	if (g_pSaySoundsKV)
		delete g_pSaySoundsKV;

	g_pSaySoundsKV = new KeyValues("SaySounds");

	// Path is defined in convar on before CS# plugin, but I hardcoded this and don't think so path might be changed.
	const char* pszPath = "addons/cs2fixes/configs/lupercalia/saysounds.txt";

	if (!g_pSaySoundsKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load saysound file %s\n", pszPath);
		return;
	}
}

void SaySound_Precache(IEntityResourceManifest* pResourceManifest)
{
	// Actually path is in config #Settings key, but I don't think path might be changed
	pResourceManifest->AddResource("soundevents/soundevents_saysounds.vsndevts");
}

bool SaySound_OnChat(CCSPlayerController* pController, const char* pMessage)
{
	if (!g_pSaySoundsKV)
		return false;

	std::string bufStr = pMessage;
	bufStr = std::regex_replace(bufStr, std::regex("\\!"), "EXCL");
	bufStr = std::regex_replace(bufStr, std::regex("\\~"), "tilda");
	bufStr = std::regex_replace(bufStr, std::regex("\\-"), "_");
	bufStr = std::regex_replace(bufStr, std::regex("\\^"), "CARET");

	for (KeyValues* pKey = g_pSaySoundsKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char* pszName = pKey->GetName();
		if (V_strcmp(pszName, "#Settings") == 0)
			continue;

		if (V_stricmp(pszName, bufStr.c_str()) == 0)
		{
			ClientPrintAll(HUD_PRINTTALK, " \x03%s \x01played \x03%s", pController->GetPlayerName(), pMessage);
			EmitSoundToAll(pKey->GetString("sound_trigger"));
			return true;
		}
	}

	return false;
}