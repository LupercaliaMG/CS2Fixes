#pragma once

class CCSPlayerController;
class IEntityResourceManifest;

#define SKINCHOOSER_PREFIX " \x04[SkinChooser]\x01 "

void SkinChooser_Init();
void SkinChooser_Precache(IEntityResourceManifest* pResourceManifest);
void SkinChooser_OnPlayerSpawn(CCSPlayerController* pController);