#pragma once

class CPlayerSlot;
class ZEPlayer;

constexpr char sqlite_players_create[] = R"(
    CREATE TABLE IF NOT EXISTS Players ( 
        SteamID64 INTEGER NOT NULL, 
        SaySound INTEGER NOT NULL DEFAULT '1',
        Skin Text,
        Created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, 
        CONSTRAINT PK_Player PRIMARY KEY (SteamID64))
)";

constexpr char sqlite_players_insert[] = R"(
    INSERT OR IGNORE INTO Players (SaySound, Skin, SteamID64) 
        VALUES (%d, '%s', %lld)
)";

constexpr char sqlite_players_update[] = R"(
    UPDATE OR IGNORE Players 
        SET SaySound=%d, Skin='%s'
        WHERE SteamID64=%lld
)";

constexpr char sql_players_get_infos[] = R"(
    SELECT SaySound, Skin
        FROM Players 
        WHERE SteamID64=%lld
)";

void Database_Setup();
void Database_OnPlayerAuthenticated(ZEPlayer* pPlayer);
void Database_SavePlayer(CPlayerSlot slot);