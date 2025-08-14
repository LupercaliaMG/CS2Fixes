
#include <filesystem>
#include "gamesystem.h"
#include "cs2_sdk/entity/cbasetoggle.h"
#include "cs2_sdk/entity/cbasemodelentity.h"
#include "utils/entity.h"
#include "ctimer.h"

extern IVEngineServer2* g_pEngineServer2;

const char* DetermineGameMode()
{
	if (UTIL_FindEntityByClassname(nullptr, "info_player_terrorist") == nullptr || 
		UTIL_FindEntityByClassname(nullptr, "info_player_counterterrorist") == nullptr)
		return "course";

	if (UTIL_FindEntityByClassname(nullptr, "func_bomb_target") == nullptr)
		return "multigames";

	CBaseEntity* pBuyZone = nullptr;
	while (nullptr != (pBuyZone = (CBaseEntity*)UTIL_FindEntityByClassname(pBuyZone, "func_buyzone")))
	{
		SpawnPoint* pTSpawn = nullptr;
		while (nullptr != (pTSpawn = (SpawnPoint*)UTIL_FindEntityByClassname(pTSpawn, "info_player_terrorist")))
		{
			Vector spawnPos = pTSpawn->GetAbsOrigin();
			Vector min = pBuyZone->m_pCollision->m_vecMins;
			if (spawnPos.WithinAABox(pBuyZone->m_pCollision->m_vecMins, pBuyZone->m_pCollision->m_vecMaxs))
				return "defusal";
		}
	}

	return "multigames";
}


void MapConfig_Init(const char* pszMapName)
{
	// finding entity doesn't work on level init some reason, delay a tick
	new CTimer(0.02f, true, true, [pszMapName]() {
		Message("MapConfig_Init(%s)\n", pszMapName);
		char szPath[MAX_PATH];
		V_snprintf(szPath, sizeof(szPath), "%s%s%s%s", Plat_GetGameDirectory(), "/csgo/", "cfg/mgcore/map/", pszMapName);
		if (!std::filesystem::exists(szPath))
		{
			const char* pszGameMode = DetermineGameMode();

			char cmd[MAX_PATH];
			V_snprintf(cmd, sizeof(cmd), "exec mgcore/map/%s", pszGameMode);
			g_pEngineServer2->ServerCommand(cmd);

			Message("MapConfig auto load : %s\n", pszGameMode);
		}

		return -1.0f;
	});
}