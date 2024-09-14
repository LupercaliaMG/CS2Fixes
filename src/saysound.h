#pragma once

class CCSPlayerController;

void SaySound_Init();
void SaySound_Precache(IEntityResourceManifest* pResourceManifest);
bool SaySound_OnChat(CCSPlayerController* pController, const char* pMessage);
