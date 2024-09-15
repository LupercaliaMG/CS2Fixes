#pragma once

class IEntityResourceManifest;
class CCSPlayerPawn;
class CBaseEntity;
class CTakeDamageInfo;

void Rocket_OnRoundStart();
void Rocket_Precache(IEntityResourceManifest* pResourceManifest);
void Rocket_Perform(CCSPlayerPawn* pPawn);
bool Rocket_Detour_OnTakeDamageOld(CBaseEntity* pThis, CTakeDamageInfo* inputInfo);