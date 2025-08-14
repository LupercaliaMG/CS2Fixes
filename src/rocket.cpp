
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

CConVar<bool> g_cvarRocketMeEnabled("cs2f_rocketme_enabled", FCVAR_NONE, "Allow players to suicide as a rocket", false);
CConVar<float> g_cvarRocketExplodeTime("cs2f_rocket_explode_time", FCVAR_NONE, "How long to detonate in seconds after launch", 3.0f);
CConVar<float> g_cvarRocketGravity("cs2f_rocket_gravity", FCVAR_NONE, "Rocket gravity", 0.1f);
CConVar<int> g_cvarRocketExplodeProb("cs2f_rocket_explode_probability", FCVAR_NONE, "The probability of detonate chance", 5);
CConVar<float> g_cvarRocketExemptTime("cs2f_rocket_exempt_time", FCVAR_NONE, "", 4.0f);

void AttachFlame(CCSPlayerPawn* pPawn)
{
	CParticleSystem* pEffect = CreateEntityByName<CParticleSystem>("info_particle_system");

	Vector vecOrigin = pPawn->GetAbsOrigin();
	vecOrigin.z += 30.0f;

	CEntityKeyValues* pKeyValues = new CEntityKeyValues();

	pKeyValues->SetString("effect_name", ROCKET_STEAMPARTICLE);
	pKeyValues->SetVector("origin", vecOrigin);
	pKeyValues->SetBool("start_active", true);

	pEffect->SetParent(pPawn);
	// SetParent, FollowEntity causes crash, idk why
	//pEffect->AcceptInput("SetParent", "!activator", pPawn);
	pEffect->DispatchSpawn(pKeyValues);

	UTIL_AddEntityIOEvent(pEffect, "DestroyImmediately", nullptr, nullptr, "", g_cvarRocketExplodeTime.Get());
	UTIL_AddEntityIOEvent(pEffect, "Kill", nullptr, nullptr, "", g_cvarRocketExplodeTime.Get() + 0.02f);
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
	for (int i = 0; i < GetGlobals()->maxClients; i++)
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
	pPawn->m_flGravityScale = g_cvarRocketGravity.Get();

	pPawn->GetOriginalController()->GetZEPlayer()->SetInRocket(true);

	CHandle<CCSPlayerPawn> hPawnHandle = pPawn->GetHandle();
	new CTimer(g_cvarRocketExplodeTime.Get(), false, false, [hPawnHandle]()
	{
		CCSPlayerPawn* pPawn = hPawnHandle.Get();
		if (!pPawn || !pPawn->IsAlive())
			return -1.0f;

		pPawn->m_flGravityScale = 1.0f;
		pPawn->GetOriginalController()->GetZEPlayer()->SetInRocket(false);

		if (rand() % 100 < g_cvarRocketExplodeProb.Get())
		{
			ClientPrintAll(HUD_PRINTTALK, " \x04[Rocket] \x03%s\x01はロケットが不発だった！", pPawn->GetOriginalController()->GetPlayerName());
			pPawn->EmitSound(ROCKET_SURVIVEDSOUND);
			pPawn->GetOriginalController()->GetZEPlayer()->SetRocketFallDamageIgnore(true);

			new CTimer(g_cvarRocketExemptTime.Get(), false, false, [hPawnHandle]()
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
	if (!g_cvarRocketMeEnabled.Get())
	{
		return;
	}

	Command_Rocket(player);
}

CON_COMMAND_CHAT(rocketme, "- a fun way to suicide")
{
	if (!g_cvarRocketMeEnabled.Get())
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
	ETargetType nType;

	if (!g_playerManager->CanTargetPlayers(player, args[1], iNumClients, pSlots, NO_BOT | NO_IMMUNITY, nType))
		return;

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