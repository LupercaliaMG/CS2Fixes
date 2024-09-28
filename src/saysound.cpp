
#include "entity/ccsplayercontroller.h"
#include "recipientfilters.h"
#include "commands.h"
#include "playermanager.h"
#include "saysound.h"
#include <regex>

KeyValues *g_pSaySoundsKV;

extern CGlobalVars* gpGlobals;

void LoadSaySoundsKV();

bool g_bSaysoundsEnable = true;
bool g_bExtraParams = true;
FAKE_BOOL_CVAR(cs2f_saysound_enable, "Allow players to play saysound", g_bSaysoundsEnable, false, false)
FAKE_BOOL_CVAR(cs2f_saysound_extra_params, "Allow extra parameters of saysounds", g_bExtraParams, false, false)

CON_COMMAND_F(cs2f_saysound_reload, "Reload Saysound", FCVAR_LINKED_CONCOMMAND | FCVAR_SPONLY | FCVAR_PROTECTED)
{
	LoadSaySoundsKV();
	Message("Reloaded Saysounds config.\n");
}

CON_COMMAND_CHAT(ss, "- toggle saysound")
{
	if (!g_bSaysoundsEnable)
		return;

	if (!player)
	{
		ClientPrint(player, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ZEPlayer* pPlayer = player->GetZEPlayer();
	bool bEnable = !pPlayer->IsSaySoundEnable();
	pPlayer->SetSaysoundEnable(bEnable);

	ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "You have %s saysounds.", bEnable ? "enabled" : "disabled");
}

void EmitSaySound(const char *pszSound, int pitch, float duration)
{
	char szSoundScape[128];
	if (pitch != 100)
		V_snprintf(szSoundScape, sizeof(szSoundScape), "%s.p%d", pszSound, pitch);
	else
		V_strncpy(szSoundScape, pszSound, sizeof(szSoundScape));

	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController || !pController->IsConnected() || pController->IsBot() || !pController->GetZEPlayer()->IsSaySoundEnable())
			continue;

		CSingleRecipientFilter filter(pController->GetPlayerSlot());
		pController->StopSound(szSoundScape);
		pController->EmitSoundFilter(filter, szSoundScape);
	}
}

void SaySound_Init()
{
	LoadSaySoundsKV();
}

void LoadSaySoundsKV()
{
	// KeyValues::AutoDelete might more better, but that causes reads values wrong, idk why
	if (g_pSaySoundsKV)
		delete g_pSaySoundsKV;

	g_pSaySoundsKV = new KeyValues("SaySounds");
	const char* pszPath = "addons/cs2fixes/configs/saysounds.cfg";

	if (!g_pSaySoundsKV->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load saysound file %s\n", pszPath);
		return;
	}
}

void SaySound_Precache(IEntityResourceManifest* pResourceManifest)
{
	pResourceManifest->AddResource("soundevents/soundevents_saysounds.vsndevts");
}

int RoundToMultiple(int toRound, int multiple)
{
	const auto ratio = static_cast<double>(toRound) / multiple;
	const auto iratio = std::lround(ratio);
	return iratio * multiple;
}

bool SaySound_OnChat(CCSPlayerController* pController, const char* pMessage)
{
	if (!g_pSaySoundsKV || !g_bSaysoundsEnable)
		return false;
	
	int pitch = 100;
	float duration = 0.0f;
	CCommand args;
	args.Tokenize(pMessage);
	if (g_bExtraParams)
	{
		for (int i = 1; i < args.ArgC(); i++)
		{
			if (*args[i] == '@')
			{
				pitch = MIN(200, MAX(50, V_StringToUint32(args[i] + 1, 100)));
				pitch = RoundToMultiple(pitch, 10);
			}
			else if (*args[i] == '#')
				duration = MAX(0.0f, V_StringToFloat32(args[i] + 1, 0.0f));
			else
				return false;
		}
	}

	std::string fixedName = args[0];
	fixedName = std::regex_replace(fixedName, std::regex("\\!"), "EXCL");
	fixedName = std::regex_replace(fixedName, std::regex("\\~"), "tilda");
	fixedName = std::regex_replace(fixedName, std::regex("\\-"), "_");
	fixedName = std::regex_replace(fixedName, std::regex("\\^"), "CARET");

	for (KeyValues* pKey = g_pSaySoundsKV->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey())
	{
		const char* pszName = pKey->GetName();
		if (V_strcmp(pszName, "#Settings") == 0)
			continue;

		if (V_stricmp(pszName, fixedName.c_str()) == 0)
		{
			ClientPrintAll(HUD_PRINTTALK, " \x03%s \x01played \x03%s", pController->GetPlayerName(), args[0]);

			EmitSaySound(pKey->GetString("sound_trigger"), pitch, duration);
			return true;
		}
	}

	return false;
}