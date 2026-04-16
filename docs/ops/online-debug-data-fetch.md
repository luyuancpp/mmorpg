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

### 3) List players currently mapped to one zone/server

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode zone -ZoneId 1 -Limit 50
```

### 4) List snapshot records from MySQL for one player

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode snapshot -PlayerId 10001 -Limit 10
```

### 5) Run a read-only SQL query against a zone DB

```powershell
pwsh -File tools/scripts/debug_fetch_data.ps1 -Mode sql -ZoneId 1 -Sql "SELECT * FROM player LIMIT 20"
```

## Production usage notes

- Point `-ConfigPath` at the real `data_service.yaml` used by the running environment.
- Point `-DbConfigPath` at the real `db.yaml` when using SQL mode.
- SQL mode only allows `SELECT`, `SHOW`, `DESCRIBE`, and `EXPLAIN`.
- Binary Redis values are returned as base64 so they can still be inspected safely.
- Text or JSON values are printed directly in formatted JSON for easier debugging.
