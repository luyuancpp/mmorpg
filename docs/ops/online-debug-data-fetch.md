# Online Debug Data Fetch

This repo now includes a small read-only debug tool for pulling player or zone data during investigation.

## Entry points

- PowerShell wrapper: `tools/scripts/debug_fetch_data.ps1`
- Go tool: `go/data_service/cmd/debug_fetch`

## Common examples

### 1) Pull one player's live Redis data

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode player -PlayerId 10001
```

### 2) Pull specific player fields only

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode player -PlayerId 10001 -Fields "PlayerDatabaseData,bag,currency"
```

### 2.1) Decode protobuf binary payloads into readable JSON

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode player -PlayerId 10001 -Fields "PlayerDatabaseData" -ProtoType "PlayerDatabase"
```

### 3) List players currently mapped to one zone/server

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode server -ZoneId 1 -Limit 50
```

### 4) Sweep the zone database for one player's rows

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode player-db -ZoneId 1 -PlayerId 10001 -Limit 20
```

### 5) List snapshot records from MySQL for one player

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode snapshot -PlayerId 10001 -Limit 10
```

### 6) Run a read-only SQL query against a zone DB

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode sql -ZoneId 1 -Sql "SELECT * FROM player LIMIT 20"
```

### 6.1) Decode PB blob columns from SQL results

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode sql -ZoneId 1 -Sql "SELECT player_database_blob FROM player_snapshot_entry LIMIT 1" -ProtoType "PlayerDatabase"
```

### 7) Save the result to a JSON file for sharing

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode player -PlayerId 10001 -OutFile "bin/debug/player-10001.json"
```

## Production usage notes

- Point `-ConfigPath` at the real `data_service.yaml` used by the running environment.
- Point `-DbConfigPath` at the real `db.yaml` when using SQL mode.
- SQL mode only allows `SELECT`, `SHOW`, `DESCRIBE`, and `EXPLAIN`.
- `server` is an alias of the zone inspection mode and includes Redis routing details.
- `player-db` will scan common player ID columns across the zone database to find matching rows quickly.
- The tool now auto-detects common PB payload names such as PlayerDatabase, PlayerDatabase_1, PlayerAllData, and PlayerCentreDatabase.
- If auto-detection is not enough, pass `-ProtoType` explicitly to force PB decoding.
- Binary Redis values are returned as base64 only when the tool cannot decode them as protobuf, JSON, or text.
- Text or JSON values are printed directly in formatted JSON for easier debugging.
