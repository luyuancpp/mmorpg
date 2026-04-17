package main

import (
	"context"
	"database/sql"
	"encoding/base64"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"

	"data_service/cmd/debugutil"
	"data_service/internal/config"
	"data_service/internal/logic"

	databasepb "proto/common/database"
	rollbackpb "proto/common/rollback"

	"google.golang.org/protobuf/encoding/protojson"
	gproto "google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
)

type renderedValue struct {
	Kind        string `json:"kind"`
	Size        int    `json:"size"`
	RawBase64   string `json:"raw_base64,omitempty"`
	Text        string `json:"text,omitempty"`
	JSONValue   any    `json:"json,omitempty"`
	ProtoType   string `json:"proto_type,omitempty"`
	ProtoJSON   any    `json:"proto_json,omitempty"`
	DecodeError string `json:"decode_error,omitempty"`
}

var (
	outputPath    string
	protoTypeHint string
)

func main() {
	var (
		configPath     = flag.String("config", "etc/data_service.yaml", "path to data_service config")
		dbConfigPath   = flag.String("db-config", "../db/etc/db.yaml", "path to db config for SQL mode")
		mode           = flag.String("mode", "player", "debug mode: player | zone | server | snapshot | sql | player-db")
		playerID       = flag.Uint64("player", 0, "player id")
		zoneID         = flag.Uint("zone", 0, "zone/server id")
		fieldsCSV      = flag.String("fields", "", "comma-separated player fields; empty means all")
		limit          = flag.Int("limit", 20, "max rows or player ids to print")
		sqlQuery       = flag.String("sql", "", "read-only SQL query for sql mode")
		protoType      = flag.String("proto-type", "", "optional protobuf message type for decoding binary payloads")
		outPath        = flag.String("out", "", "optional output file path; default stdout")
		timeoutSeconds = flag.Int("timeout-seconds", 10, "request timeout in seconds")
	)
	flag.Parse()

	if *limit <= 0 {
		*limit = 20
	}
	if *timeoutSeconds <= 0 {
		*timeoutSeconds = 10
	}

	outputPath = strings.TrimSpace(*outPath)
	protoTypeHint = strings.TrimSpace(*protoType)

	ctx, cancel := context.WithTimeout(context.Background(), time.Duration(*timeoutSeconds)*time.Second)
	defer cancel()

	var err error
	switch resolveModeName(*mode) {
	case "player":
		err = runPlayerMode(ctx, *configPath, *playerID, parseFields(*fieldsCSV))
	case "zone":
		err = runZoneMode(ctx, *configPath, uint32(*zoneID), *limit)
	case "snapshot":
		err = runSnapshotMode(ctx, *configPath, *playerID, *limit)
	case "sql":
		err = runSQLMode(ctx, *dbConfigPath, uint32(*zoneID), *sqlQuery, *limit)
	case "playerdb":
		err = runPlayerDBMode(ctx, *dbConfigPath, uint32(*zoneID), *playerID, *limit)
	default:
		err = fmt.Errorf("unsupported mode %q", *mode)
	}

	if err != nil {
		fmt.Fprintf(os.Stderr, "debug_fetch failed: %v\n", err)
		os.Exit(1)
	}
}

func runPlayerMode(ctx context.Context, configPath string, playerID uint64, fields []string) error {
	if playerID == 0 {
		return errors.New("-player is required for player mode")
	}

	svcCtx, cleanup, err := debugutil.NewServiceContextWithSnapshot(configPath)
	if err != nil {
		return err
	}
	defer cleanup()

	zoneID, zoneErr := svcCtx.Router.GetPlayerHomeZone(ctx, playerID)
	if zoneErr != nil {
		return fmt.Errorf("resolve player zone: %w", zoneErr)
	}

	resp, err := logic.LoadPlayerData(ctx, svcCtx, &logic.LoadPlayerDataReq{
		PlayerID: playerID,
		Fields:   fields,
	})
	if err != nil {
		return fmt.Errorf("load player data from redis: %w", err)
	}

	fieldNames := make([]string, 0, len(resp.Data))
	decoded := make(map[string]renderedValue, len(resp.Data))
	for name, value := range resp.Data {
		fieldNames = append(fieldNames, name)
		decoded[name] = renderValueWithHints(name, value, protoTypeHint)
	}
	sort.Strings(fieldNames)

	return writeJSON(map[string]any{
		"mode":         "player",
		"player_id":    playerID,
		"home_zone_id": zoneID,
		"version":      resp.Version,
		"field_count":  len(resp.Data),
		"field_names":  fieldNames,
		"fields":       decoded,
	})
}

func runZoneMode(ctx context.Context, configPath string, zoneID uint32, limit int) error {
	if zoneID == 0 {
		return errors.New("-zone is required for zone mode")
	}

	svcCtx, cleanup, err := debugutil.NewServiceContextWithSnapshot(configPath)
	if err != nil {
		return err
	}
	defer cleanup()

	playerIDs, err := svcCtx.Router.GetAllPlayerIDsInZone(ctx, zoneID)
	if err != nil {
		return fmt.Errorf("load zone player ids from redis: %w", err)
	}
	sort.Slice(playerIDs, func(i, j int) bool { return playerIDs[i] < playerIDs[j] })

	shown := playerIDs
	if len(shown) > limit {
		shown = shown[:limit]
	}

	info := buildZoneDebugInfo(svcCtx.Config, zoneID)
	info["mode"] = "zone"
	info["zone_id"] = zoneID
	info["players_total"] = len(playerIDs)
	info["players_returned"] = len(shown)
	info["player_ids"] = shown

	return writeJSON(info)
}

func runSnapshotMode(ctx context.Context, configPath string, playerID uint64, limit int) error {
	if playerID == 0 {
		return errors.New("-player is required for snapshot mode")
	}

	svcCtx, cleanup, err := debugutil.NewServiceContextWithSnapshot(configPath)
	if err != nil {
		return err
	}
	defer cleanup()

	if svcCtx.SnapshotStore == nil {
		return errors.New("snapshot mysql is not configured or unavailable")
	}

	resp, err := logic.ListPlayerSnapshots(ctx, svcCtx, &logic.ListSnapshotsReq{
		PlayerID: playerID,
		Limit:    uint32(limit),
	})
	if err != nil {
		return fmt.Errorf("list player snapshots from mysql: %w", err)
	}

	return writeJSON(map[string]any{
		"mode":      "snapshot",
		"player_id": playerID,
		"count":     len(resp.Snapshots),
		"snapshots": resp.Snapshots,
	})
}

func buildZoneDebugInfo(c config.Config, zoneID uint32) map[string]any {
	info := map[string]any{}

	if strings.TrimSpace(c.DevRedis.Host) != "" {
		info["routing_mode"] = "dev-single-redis"
		info["redis_addr"] = c.DevRedis.Host
		info["redis_db"] = c.DevRedis.DB
		return info
	}

	for _, region := range c.Regions {
		for _, candidateZone := range region.Zones {
			if candidateZone == zoneID {
				info["routing_mode"] = "region-redis-cluster"
				info["region_id"] = region.Id
				info["redis_addrs"] = region.Redis.Addrs
				return info
			}
		}
	}

	info["routing_mode"] = "unknown"
	return info
}

func runSQLMode(ctx context.Context, dbConfigPath string, zoneID uint32, query string, limit int) error {
	if err := ensureReadOnlyQuery(query); err != nil {
		return err
	}

	db, resolvedZoneID, dbName, err := debugutil.OpenZoneDB(ctx, dbConfigPath, zoneID)
	if err != nil {
		return err
	}
	defer db.Close()

	cols, resultRows, err := queryRows(ctx, db, query, limit)
	if err != nil {
		return fmt.Errorf("execute sql query: %w", err)
	}

	return writeJSON(map[string]any{
		"mode":          "sql",
		"zone_id":       resolvedZoneID,
		"database":      dbName,
		"columns":       cols,
		"rows_returned": len(resultRows),
		"rows":          resultRows,
	})
}

func runPlayerDBMode(ctx context.Context, dbConfigPath string, zoneID uint32, playerID uint64, limit int) error {
	if playerID == 0 {
		return errors.New("-player is required for player-db mode")
	}

	db, resolvedZoneID, dbName, err := debugutil.OpenZoneDB(ctx, dbConfigPath, zoneID)
	if err != nil {
		return err
	}
	defer db.Close()

	const discoverySQL = `
SELECT table_name, column_name
FROM information_schema.columns
WHERE table_schema = ?
  AND LOWER(column_name) IN ('player_id', 'playerid', 'role_id', 'roleid', 'uid')
ORDER BY table_name, column_name`

	rows, err := db.QueryContext(ctx, discoverySQL, dbName)
	if err != nil {
		return fmt.Errorf("discover player tables: %w", err)
	}
	defer rows.Close()

	matches := make([]map[string]any, 0)
	tablesScanned := 0
	for rows.Next() {
		var tableName string
		var columnName string
		if err := rows.Scan(&tableName, &columnName); err != nil {
			return fmt.Errorf("scan discovered table metadata: %w", err)
		}
		tablesScanned++

		safeTable, err := debugutil.QuoteIdentifier(tableName)
		if err != nil {
			continue
		}
		safeColumn, err := debugutil.QuoteIdentifier(columnName)
		if err != nil {
			continue
		}

		query := fmt.Sprintf("SELECT * FROM %s WHERE %s = ? LIMIT %d", safeTable, safeColumn, limit)
		_, tableRows, err := queryRows(ctx, db, query, limit, playerID)
		if err != nil {
			continue
		}
		if len(tableRows) == 0 {
			continue
		}

		matches = append(matches, map[string]any{
			"table":         tableName,
			"match_column":  columnName,
			"rows_returned": len(tableRows),
			"rows":          tableRows,
		})
	}
	if err := rows.Err(); err != nil {
		return fmt.Errorf("iterate discovered tables: %w", err)
	}

	return writeJSON(map[string]any{
		"mode":           "playerdb",
		"player_id":      playerID,
		"zone_id":        resolvedZoneID,
		"database":       dbName,
		"tables_scanned": tablesScanned,
		"matched_tables": len(matches),
		"matches":        matches,
	})
}

func queryRows(ctx context.Context, db *sql.DB, query string, limit int, args ...any) ([]string, []map[string]any, error) {
	rows, err := db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, nil, err
	}
	defer rows.Close()

	cols, err := rows.Columns()
	if err != nil {
		return nil, nil, fmt.Errorf("read columns: %w", err)
	}

	resultRows := make([]map[string]any, 0, limit)
	for rows.Next() {
		values := make([]any, len(cols))
		scans := make([]any, len(cols))
		for i := range values {
			scans[i] = &values[i]
		}
		if err := rows.Scan(scans...); err != nil {
			return nil, nil, fmt.Errorf("scan row: %w", err)
		}

		rowMap := make(map[string]any, len(cols))
		for i, col := range cols {
			rowMap[col] = normalizeSQLValue(col, values[i])
		}
		resultRows = append(resultRows, rowMap)
		if len(resultRows) >= limit {
			break
		}
	}
	if err := rows.Err(); err != nil {
		return nil, nil, fmt.Errorf("iterate rows: %w", err)
	}

	return cols, resultRows, nil
}

func resolveModeName(mode string) string {
	switch strings.ToLower(strings.TrimSpace(mode)) {
	case "server":
		return "zone"
	case "player-db", "player_db":
		return "playerdb"
	default:
		return strings.ToLower(strings.TrimSpace(mode))
	}
}

func parseFields(csv string) []string {
	csv = strings.TrimSpace(csv)
	if csv == "" {
		return nil
	}
	parts := strings.Split(csv, ",")
	result := make([]string, 0, len(parts))
	for _, part := range parts {
		part = strings.TrimSpace(part)
		if part != "" {
			result = append(result, part)
		}
	}
	return result
}

func renderValue(data []byte) renderedValue {
	return renderValueWithHints("", data, "")
}

func renderValueWithHints(fieldName string, data []byte, explicitType string) renderedValue {
	decoded := renderedValue{Size: len(data)}
	if len(data) == 0 {
		decoded.Kind = "text"
		decoded.Text = ""
		return decoded
	}

	// Always include raw bytes so debug_import can restore them.
	decoded.RawBase64 = base64.StdEncoding.EncodeToString(data)

	if protoType := chooseProtoTypeHint(fieldName, explicitType); protoType != "" {
		protoValue, resolvedType, err := decodeProtoPayload(data, protoType)
		if err == nil {
			decoded.Kind = "protobuf"
			decoded.ProtoType = resolvedType
			decoded.ProtoJSON = protoValue
			return decoded
		}
		decoded.DecodeError = err.Error()
	}

	if json.Valid(data) {
		var value any
		if err := json.Unmarshal(data, &value); err == nil {
			decoded.Kind = "json"
			decoded.JSONValue = value
			return decoded
		}
	}

	if utf8.Valid(data) {
		decoded.Kind = "text"
		decoded.Text = string(data)
		return decoded
	}

	decoded.Kind = "binary"
	return decoded
}

func chooseProtoTypeHint(fieldName string, explicitType string) string {
	if strings.TrimSpace(explicitType) != "" {
		return strings.TrimSpace(explicitType)
	}

	normalized := normalizeToken(fieldName)
	switch {
	case strings.Contains(normalized, "playeralldata"):
		return "PlayerAllData"
	case strings.Contains(normalized, "playerdatabase1"):
		return "PlayerDatabase_1"
	case strings.Contains(normalized, "playerdatabase"):
		return "PlayerDatabase"
	case strings.Contains(normalized, "playercentredatabase"), strings.Contains(normalized, "playercenterdatabase"):
		return "PlayerCentreDatabase"
	case strings.Contains(normalized, "playersnapshotentry") || strings.Contains(normalized, "snapshotblob"):
		return "PlayerSnapshotEntry"
	default:
		return ""
	}
}

func decodeProtoPayload(data []byte, typeName string) (any, string, error) {
	msg, resolvedType, err := newProtoMessageByTypeName(typeName)
	if err != nil {
		return nil, "", err
	}
	if err := gproto.Unmarshal(data, msg); err != nil {
		return nil, resolvedType, fmt.Errorf("unmarshal %s: %w", resolvedType, err)
	}

	jsonBytes, err := protojson.MarshalOptions{
		Multiline:       true,
		Indent:          "  ",
		EmitUnpopulated: true,
		UseProtoNames:   true,
	}.Marshal(msg)
	if err != nil {
		return nil, resolvedType, fmt.Errorf("marshal %s to json: %w", resolvedType, err)
	}

	var value any
	if err := json.Unmarshal(jsonBytes, &value); err != nil {
		return string(jsonBytes), resolvedType, nil
	}
	return value, resolvedType, nil
}

func newProtoMessageByTypeName(typeName string) (gproto.Message, string, error) {
	normalized := normalizeToken(typeName)
	if normalized == "" {
		return nil, "", errors.New("protobuf type name must not be empty")
	}

	switch normalized {
	case "playeralldata":
		return &databasepb.PlayerAllData{}, "PlayerAllData", nil
	case "playerdatabase", "playerdatabasedata":
		return &databasepb.PlayerDatabase{}, "PlayerDatabase", nil
	case "playerdatabase1", "playerdatabase1data":
		return &databasepb.PlayerDatabase_1{}, "PlayerDatabase_1", nil
	case "playercentredatabase", "playercenterdatabase":
		return &databasepb.PlayerCentreDatabase{}, "PlayerCentreDatabase", nil
	case "playersnapshotentry":
		return &rollbackpb.PlayerSnapshotEntry{}, "PlayerSnapshotEntry", nil
	}

	var found gproto.Message
	var resolvedType string
	protoregistry.GlobalTypes.RangeMessages(func(mt protoreflect.MessageType) bool {
		fullName := string(mt.Descriptor().FullName())
		shortName := string(mt.Descriptor().Name())
		if normalizeToken(fullName) == normalized || normalizeToken(shortName) == normalized {
			found = mt.New().Interface()
			resolvedType = shortName
			return false
		}
		return true
	})
	if found != nil {
		return found, resolvedType, nil
	}

	return nil, "", fmt.Errorf("unsupported protobuf type %q", typeName)
}

func normalizeToken(value string) string {
	var b strings.Builder
	for _, r := range strings.ToLower(strings.TrimSpace(value)) {
		if unicode.IsLetter(r) || unicode.IsDigit(r) {
			b.WriteRune(r)
		}
	}
	return b.String()
}

func ensureReadOnlyQuery(query string) error {
	q := strings.TrimSpace(query)
	if q == "" {
		return errors.New("-sql must not be empty")
	}

	if strings.Count(q, ";") > 1 {
		return errors.New("only a single read-only SQL statement is allowed")
	}
	if idx := strings.Index(q, ";"); idx >= 0 && idx != len(q)-1 {
		return errors.New("multiple SQL statements are not allowed")
	}
	q = strings.TrimSuffix(q, ";")
	q = strings.TrimSpace(q)

	firstWord := strings.ToLower(strings.Fields(q)[0])
	switch firstWord {
	case "select", "show", "describe", "desc", "explain":
		return nil
	default:
		return fmt.Errorf("only read-only SELECT/SHOW/DESCRIBE/EXPLAIN queries are allowed, got %q", firstWord)
	}
}

func normalizeSQLValue(columnName string, value any) any {
	switch v := value.(type) {
	case nil:
		return nil
	case []byte:
		if protoHint := chooseProtoTypeHint(columnName, protoTypeHint); protoHint != "" {
			return renderValueWithHints(columnName, v, protoHint)
		}
		if utf8.Valid(v) {
			return string(v)
		}
		return renderValueWithHints(columnName, v, protoTypeHint)
	case time.Time:
		return v.Format(time.RFC3339)
	default:
		return v
	}
}

func writeJSON(v any) error {
	if outputPath == "" {
		encoder := json.NewEncoder(os.Stdout)
		encoder.SetIndent("", "  ")
		return encoder.Encode(v)
	}

	resolvedPath, err := filepath.Abs(outputPath)
	if err != nil {
		return fmt.Errorf("resolve output path: %w", err)
	}
	if err := os.MkdirAll(filepath.Dir(resolvedPath), 0o755); err != nil {
		return fmt.Errorf("create output directory: %w", err)
	}

	file, err := os.Create(resolvedPath)
	if err != nil {
		return fmt.Errorf("create output file: %w", err)
	}
	defer file.Close()

	encoder := json.NewEncoder(file)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(v); err != nil {
		return err
	}

	fmt.Fprintf(os.Stderr, "debug_fetch wrote result to %s\n", resolvedPath)
	return nil
}
