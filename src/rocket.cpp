
#include "rocket.h"

#include "entity/ccsplayerpawn.h"
#include "commands.h"
#include "ctimer.h"
#include "utils/entity.h"
#include "playermanager.h"
#include "adminsystem.h"

#define ROCKET_LAUNCHSOUND "C4.ExplodeWarning"
#define ROCKET_EXPLODESOUND "c4.explode"
#define ROCKET_SURVIVEDSOUND "c4.explode"

#define ROCKET_STEAMPARTICLE "particles/burning_fx/gas_cannister_trail_smoke.vpcf"
#define ROCKET_EXPLODEPARTICLE "particles/explosions_fx/explosion_basic.vpcf"

bool g_bRocketMeEnabled = false;
float g_flRocketExplodeTime = 3.0f;
float g_flRocketGravity = 0.1f;
int g_iRocketExplodeProbability = 50;
float g_flRocketExemptTime = 4.0f;
FAKE_BOOL_CVAR(lp_rocketme_enabled, "Allow players to suicide as a rocket", g_bRocketMeEnabled, false, false)
FAKE_FLOAT_CVAR(lp_rocket_explode_time, "How long to detonate in seconds after launch", g_flRocketExplodeTime, 3.0f, false)
FAKE_FLOAT_CVAR(lp_rocket_gravity, "How long to detonate in seconds after launch", g_flRocketGravity, 0.1f, false)
FAKE_INT_CVAR(lp_rocket_explode_probability, "The probability of detonate chance", g_iRocketExplodeProbability, 50, false)
FAKE_FLOAT_CVAR(lp_rocket_exempt_time, "How long to detonate in seconds after launch", g_flRocketExemptTime, 4.0f, false)

extern CGlobalVars* gpGlobals;

void AttachFlame(CCSPlayerPawn* pPawn)
{
	CParticleSystem* pEffect = CreateEntityByName<CParticleSystem>("info_particle_system");

	Vector vecOrigin = pPawn->GetAbsOrigin();
	vecOrigin.z += 30.0f;

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", ROCKET_STEAMPARTICLE);
	pKeyValues->SetVector("origin", vecOrigin);
	pKeyValues->SetBool("start_active", true);

	// SetParent, FollowEntity causes crash, idk why
	//pEffect->AcceptInput("SetParent", "!activator", pPawn);
	pEffect->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(pEffect, "DestroyImmediately", nullptr, nullptr, "", g_flRocketExplodeTime);
	UTIL_AddEntityIOEvent(pEffect, "Kill", nullptr, nullptr, "", g_flRocketExplodeTime + 0.02f);
}

void MakeExplosionEffect(CCSPlayerPawn *pPawn)
{
	CParticleSystem* pEffect = CreateEntityByName<CParticleSystem>("info_particle_system");

	Vector vecOrigin = pPawn->GetAbsOrigin();
	vecOrigin.z += 10;

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", ROCKET_EXPLODEPARTICLE);
	pKeyValues->SetVector("origin", vecOrigin);
	pKeyValues->SetBool("start_active", true);

	pEffect->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(pEffect, "Kill", nullptr, nullptr, "", 2.0f);
}

void Rocket_Precache(IEntityResourceManifest* pResourceManifest)
{
	pResourceManifest->AddResource(ROCKET_LAUNCHSOUND);
	pResourceManifest->AddResource(ROCKET_EXPLODESOUND);
	pResourceManifest->AddResource(ROCKET_SURVIVEDSOUND);
	pResourceManifest->AddResource(ROCKET_STEAMPARTICLE);
	pResourceManifest->AddResource(ROCKET_EXPLODEPARTICLE);
}

void Rocket_OnRoundStart()
{
	for (int i = 0; i < gpGlobals->maxClients; i++)
	{
		CCSPlayerController* pController = CCSPlayerController::FromSlot(i);
		if (!pController || !pController->IsConnected())
			continue;

		ZEPlayer* pPlayer = pController->GetZEPlayer();
		if (!pPlayer)
			continue;

		pPlayer->SetInRocket(false);
		pPlayer->SetRocketFallDamageIgnore(false);
	}
}

bool Rocket_Detour_OnTakeDamageOld(CBaseEntity* pThis, CTakeDamageInfo* inputInfo)
{
	if (!pThis->IsPawn())
		return true;

	CCSPlayerPawn* pPawn = (CCSPlayerPawn*)pThis;
	CCSPlayerController* pController = pPawn->GetOriginalController();
	if (!pController)
		return true;

	if (inputInfo->m_bitsDamageType & DMG_FALL && pController->GetZEPlayer()->IsRocketFallDamageIgnore())
		return false;

	return true;
}

void Rocket_Perform(CCSPlayerPawn* pPawn)
{
	if (!pPawn)
		return;

	// Todo : steam effect on rocket launch
	//AttachFlame(pPawn);
	pPawn->EmitSound(ROCKET_LAUNCHSOUND);

	Vector vel = Vector(0, 0, 800);
	pPawn->Teleport(nullptr, nullptr, &vel);
	pPawn->m_flGravityScale = g_flRocketGravity;

	pPawn->GetOriginalController()->GetZEPlayer()->SetInRocket(true);

	CHandle<CCSPlayerPawn> hPawnHandle = pPawn->GetHandle();
	new CTimer(g_flRocketExplodeTime, false, false, [hPawnHandle]()
	{
		CCSPlayerPawn* pPawn = hPawnHandle.Get();
		if (!pPawn || !pPawn->IsAlive())
			return -1.0f;

		pPawn->m_flGravityScale = 1.0f;
		pPawn->GetOriginalController()->GetZEPlayer()->SetInRocket(false);

		if (rand() % 100 < g_iRocketExplodeProbability)
		{
			pPawn->EmitSound(ROCKET_SURVIVEDSOUND);
			pPawn->GetOriginalController()->GetZEPlayer()->SetRocketFallDamageIgnore(true);

			new CTimer(g_flRocketExemptTime, false, false, [hPawnHandle]()
			{
				CCSPlayerPawn* pPawn = hPawnHandle.Get();
				if (!pPawn)
					return -1.0f;

				pPawn->GetOriginalController()->GetZEPlayer()->SetRocketFallDamageIgnore(false);
				return -1.0f;
			});

			return -1.0f;
		}

		pPawn->CommitSuicide(false, true);
		pPawn->EmitSound(ROCKET_EXPLODESOUND);
		MakeExplosionEffect(pPawn);

		return -1.0f;
	});
}

void Command_Rocket(CCSPlayerController* pController)
{
	if (!pController)
	{
		ClientPrint(pController, HUD_PRINTCONSOLE, CHAT_PREFIX "You cannot use this command from the server console.");
		return;
	}

	ZEPlayer* pPlayer = pController->GetZEPlayer();
	if (pPlayer->IsInRocket())
	{
		ClientPrint(pController, HUD_PRINTCONSOLE, CHAT_PREFIX "You are in rocket already.");
		return;
	}

	Rocket_Perform(pController->GetPlayerPawn());
}

CON_COMMAND_CHAT(rocket, "- a fun way to suicide")
{
	if (!g_bRocketMeEnabled)
	{
		return;
	}

	Command_Rocket(player);
}

CON_COMMAND_CHAT(rocketme, "- a fun way to suicide")
{
	if (!g_bRocketMeEnabled)
	{
		return;
	}

	Command_Rocket(player);
}

CON_COMMAND_CHAT_FLAGS(evilrocket, "<name>", ADMFLAG_SLAY)
{
	if (args.ArgC() < 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Usage: !evilrocket <name>");
		return;
	}

	int iCommandPlayer = player ? player->GetPlayerSlot() : -1;
	int iNumClients = 0;
	int pSlots[MAXPLAYERS];

	ETargetType nType = g_playerManager->TargetPlayerString(iCommandPlayer, args[1], iNumClients, pSlots);

	if (!iNumClients)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "Target not found.");
		return;
	}

	if (nType == ETargetType::PLAYER && iNumClients > 1)
	{
		ClientPrint(player, HUD_PRINTTALK, CHAT_PREFIX "More than one client matched.");
		return;
	}

	const char* pszCommandPlayerName = player ? player->GetPlayerName() : "Console";

	for (int i = 0; i < iNumClients; i++)
	{
		CCSPlayerController* pTarget = CCSPlayerController::FromSlot(pSlots[i]);
		if (!pTarget)
			continue;

		if (pTarget->GetZEPlayer()->IsInRocket())
			continue;

		Rocket_Perform(pTarget->GetPlayerPawn());

		if (nType < ETargetType::ALL)
			PrintSingleAdminAction(pszCommandPlayerName, pTarget->GetPlayerName(), "rocketed", "", CHAT_PREFIX);
	}

	PrintMultiAdminAction(nType, pszCommandPlayerName, "rocketed", "", CHAT_PREFIX);
}