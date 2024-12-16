
#include "cs2fixes.h"
#include "commands.h"
#include "database.h"

#include "vendor/sql_mm/src/public/sql_mm.h"
#include "vendor/sql_mm/src/public/sqlite_mm.h"
#include "vendor/sql_mm/src/public/mysql_mm.h"

ISQLConnection* g_pConnection;

void Database_RunMigrations();
void OnGenericTxnSuccess(std::vector<ISQLQuery*> queries)
{
	ConMsg("Transaction successful.\n");
}

void OnGenericTxnFailure(std::string error, int failIndex)
{
	ConMsg("Transaction failed at %i (%s).\n", failIndex, error.c_str());
}

void Database_Setup()
{
	ISQLInterface* sqlInterface = (ISQLInterface*)g_SMAPI->MetaFactory(SQLMM_INTERFACE, nullptr, nullptr);
	if (!sqlInterface)
	{
		Message("Database plugin not found. Local database is disabled.\n");
		return;
	}

	SQLiteConnectionInfo info;
	info.database = "addons/cs2fixes/data/user_preferences.sqlite3";
	g_pConnection = sqlInterface->GetSQLiteClient()->CreateSQLiteConnection(info);
	g_pConnection->Connect([](bool connect)
	{
		if (connect)
		{
			Message("Database connected.\n");
			Database_RunMigrations();
		}
		else
		{
			Message("Failed to connect database.\n");
			g_pConnection->Destroy();
			g_pConnection = nullptr;
		}
	});
}

void Database_RunMigrations()
{
	g_pConnection->Query(sqlite_players_create, [](bool success) {});
}

void Database_OnPlayerAuthenticated(ZEPlayer* pPlayer)
{
	char szQuery[1024];
	V_snprintf(szQuery, sizeof(szQuery), sql_players_get_infos, pPlayer->GetSteamId64());

	ZEPlayerHandle handle = pPlayer->GetHandle();
	g_pConnection->Query(szQuery, [handle](ISQLQuery* query) {

		ZEPlayer* pPlayer = handle.Get();
		if (!pPlayer)
			return;

		ISQLResult *results = query->GetResultSet();
		if (results->FetchRow())
		{
			pPlayer->SetSaysoundEnable(results->GetInt(0));
			pPlayer->SetSkinPreference(results->GetString(1));
		}
		else
		{
			pPlayer->SetSaysoundEnable(true);
			pPlayer->SetSkinPreference("");
		}

		pPlayer->SetDBLoaded(true);
	});
}

void Database_SavePlayer(CPlayerSlot slot)
{
	CCSPlayerController *pController = CCSPlayerController::FromSlot(slot);
	if (!pController)
		return;

	ZEPlayer* pPlayer = pController->GetZEPlayer();
	if (!pPlayer)
		return;

	if (!pPlayer->IsAuthenticated() || !pPlayer->IsDBLoaded())
		return;

	Message(pPlayer->GetSkinPreference());

	Transaction txn;
	char szQuery[1024];

	V_snprintf(szQuery, sizeof(szQuery), sqlite_players_update, pPlayer->IsSaySoundEnable(), pPlayer->GetSkinPreference(), pPlayer->GetSteamId64());
	txn.queries.push_back(szQuery);

	V_snprintf(szQuery, sizeof(szQuery), sqlite_players_insert, pPlayer->IsSaySoundEnable(), pPlayer->GetSkinPreference(), pPlayer->GetSteamId64());
	txn.queries.push_back(szQuery);

	g_pConnection->ExecuteTransaction(txn, OnGenericTxnSuccess, OnGenericTxnFailure);
}