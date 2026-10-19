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

// renderedValue 与导出包共用 debugutil.Value 的形状(JSON 键名是 debug_import 的输入契约)。
type renderedValue = debugutil.Value

var (
	outputPath    string
	protoTypeHint string
)

// player-bundle 要连三到四个实例、读几十把键,端口转发下 10 秒常常不够。
const (
	defaultTimeoutSeconds       = 10
	defaultBundleTimeoutSeconds = 60
)

func main() {
	var (
		configPath     = flag.String("config", "etc/data_service.yaml", "path to data_service config (player-bundle: global DB = SnapshotMySQL)")
		dbConfigPath   = flag.String("db-config", "../db/etc/db.yaml", "path to db config (SQL modes; player-bundle: zone MySQL + core/mapping Redis)")
		mode           = flag.String("mode", "player", "debug mode: player-bundle | player | zone | server | snapshot | sql | player-db")
		playerID       = flag.Uint64("player", 0, "player id")
		zoneID         = flag.Uint("zone", 0, "zone/server id (player-bundle: fallback zone when player:zone is missing)")
		database       = flag.String("database", "", "explicit database name (sql / player-db / player-bundle); overrides zone_{zone}_db and placement")
		fieldsCSV      = flag.String("fields", "", "comma-separated player fields; empty means all")
		limit          = flag.Int("limit", 20, "max rows or player ids to print")
		sqlQuery       = flag.String("sql", "", "read-only SQL query for sql mode")
		protoType      = flag.String("proto-type", "", "optional protobuf message type for decoding binary payloads")
		outPath        = flag.String("out", "", "optional output file path; default stdout")
		timeoutSeconds = flag.Int("timeout-seconds", 0, "request timeout in seconds (0 = 10, player-bundle 60)")

		reason           = flag.String("reason", "", "player-bundle: ticket / feedback id, recorded in the bundle (required)")
		operator         = flag.String("operator", "", "player-bundle: who exported it (default: OS user)")
		serverRevision   = flag.String("server-revision", "", "player-bundle: git commit of the live server image, recorded for decoding")
		redisAddr        = flag.String("redis-addr", "", "player-bundle: override core Redis host:port (e.g. a port-forward)")
		mappingRedisAddr = flag.String("mapping-redis-addr", "", "player-bundle: override mapping Redis host:port (default follows core when Placement.Redis is unset)")
		mysqlAddr        = flag.String("mysql-addr", "", "player-bundle: override zone MySQL host:port")
		globalMySQLAddr  = flag.String("global-mysql-addr", "", "player-bundle: override global MySQL (SnapshotMySQL) host:port")
		redisPassEnv     = flag.String("redis-password-env", "", "player-bundle: read Redis password from this env var instead of the yaml")
		mysqlPassEnv     = flag.String("mysql-password-env", "", "player-bundle: read MySQL password (zone + global) from this env var instead of the yaml")
		skipGlobal       = flag.Bool("skip-global", false, "player-bundle: do not read the global DB")
		skipAccount      = flag.Bool("skip-account", false, "player-bundle: do not read the account role directory")
	)
	flag.Parse()

	if *limit <= 0 {
		*limit = 20
	}
	resolvedMode := resolveModeName(*mode)
	if *timeoutSeconds <= 0 {
		*timeoutSeconds = defaultTimeoutSeconds
		if resolvedMode == "playerbundle" {
			*timeoutSeconds = defaultBundleTimeoutSeconds
		}
	}

	outputPath = strings.TrimSpace(*outPath)
	protoTypeHint = strings.TrimSpace(*protoType)

	ctx, cancel := context.WithTimeout(context.Background(), time.Duration(*timeoutSeconds)*time.Second)
	defer cancel()

	var err error
	switch resolvedMode {
	case "playerbundle":
		op := strings.TrimSpace(*operator)
		if op == "" {
			op = defaultOperator()
		}
		err = runPlayerBundleMode(ctx, bundleOptions{
			dbConfigPath: *dbConfigPath, configPath: *configPath,
			playerID: *playerID, fallbackZone: uint32(*zoneID), databaseOverride: *database, limit: *limit,
			reason: *reason, operator: op, serverRevision: *serverRevision,
			redisAddr: strings.TrimSpace(*redisAddr), mappingRedisAddr: strings.TrimSpace(*mappingRedisAddr),
			mysqlAddr: strings.TrimSpace(*mysqlAddr), globalMySQLAddr: strings.TrimSpace(*globalMySQLAddr),
			redisPasswordEnv: *redisPassEnv, mysqlPasswordEnv: *mysqlPassEnv,
			skipGlobal: *skipGlobal, skipAccount: *skipAccount, now: time.Now,
		})
	case "player":
		fmt.Fprintln(os.Stderr, "注意:player 模式只读 data_service 的 player:{id}:<field> 键族,不是玩家实时数据;排查玩家请用 -mode player-bundle")
		err = runPlayerMode(ctx, *configPath, *playerID, parseFields(*fieldsCSV))
	case "zone":
		err = runZoneMode(ctx, *configPath, uint32(*zoneID), *limit)
	case "snapshot":
		err = runSnapshotMode(ctx, *configPath, *playerID, *limit)
	case "sql":
		err = runSQLMode(ctx, *dbConfigPath, uint32(*zoneID), *database, *sqlQuery, *limit)
	case "playerdb":
		err = runPlayerDBMode(ctx, *dbConfigPath, uint32(*zoneID), *database, *playerID, *limit)
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

func runSQLMode(ctx context.Context, dbConfigPath string, zoneID uint32, dbOverride, query string, limit int) error {
	if err := ensureReadOnlyQuery(query); err != nil {
		return err
	}

	db, resolvedZoneID, dbName, err := debugutil.OpenZoneDBNamed(ctx, dbConfigPath, zoneID, dbOverride)
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

func runPlayerDBMode(ctx context.Context, dbConfigPath string, zoneID uint32, dbOverride string, playerID uint64, limit int) error {
	if playerID == 0 {
		return errors.New("-player is required for player-db mode")
	}

	db, resolvedZoneID, dbName, err := debugutil.OpenZoneDBNamed(ctx, dbConfigPath, zoneID, dbOverride)
	if err != nil {
		return err
	}
	defer db.Close()

	// 先把发现结果全部读完并关闭游标,再逐表查询。连接池只有一条连接(OpenZoneDB),
	// 游标开着时再发查询会一直等那条连接,直到整次读取超时。
	pairs, err := discoverPlayerTables(ctx, db, dbName)
	if err != nil {
		return err
	}

	matches := make([]map[string]any, 0)
	tableErrors := make([]map[string]string, 0)
	scannedTables := map[string]struct{}{}
	for _, pair := range pairs {
		scannedTables[pair.table] = struct{}{}
		safeTable, err := debugutil.QuoteIdentifier(pair.table)
		if err != nil {
			tableErrors = append(tableErrors, map[string]string{"table": pair.table, "error": err.Error()})
			continue
		}
		safeColumn, err := debugutil.QuoteIdentifier(pair.column)
		if err != nil {
			tableErrors = append(tableErrors, map[string]string{"table": pair.table, "error": err.Error()})
			continue
		}

		query := fmt.Sprintf("SELECT * FROM %s WHERE %s = ? LIMIT %d", safeTable, safeColumn, limit+1)
		_, tableRows, err := queryRows(ctx, db, query, limit+1, playerID)
		if err != nil {
			// 不能静默跳过:权限不足 / 超时会被误读成「这张表里没有该玩家」。
			tableErrors = append(tableErrors, map[string]string{"table": pair.table, "error": err.Error()})
			continue
		}
		if len(tableRows) == 0 {
			continue
		}
		truncated := len(tableRows) > limit
		if truncated {
			tableRows = tableRows[:limit]
		}

		matches = append(matches, map[string]any{
			"table":         pair.table,
			"match_column":  pair.column,
			"rows_returned": len(tableRows),
			"truncated":     truncated,
			"rows":          tableRows,
		})
	}

	return writeJSON(map[string]any{
		"mode":           "playerdb",
		"player_id":      playerID,
		"zone_id":        resolvedZoneID,
		"database":       dbName,
		"tables_scanned": len(scannedTables),
		"matched_tables": len(matches),
		"matches":        matches,
		"errors":         tableErrors,
	})
}

type tableColumn struct {
	table  string
	column string
}

func discoverPlayerTables(ctx context.Context, db *sql.DB, dbName string) ([]tableColumn, error) {
	const discoverySQL = `
SELECT table_name, column_name
FROM information_schema.columns
WHERE table_schema = ?
  AND LOWER(column_name) IN ('player_id', 'playerid', 'role_id', 'roleid', 'uid')
ORDER BY table_name, column_name`

	rows, err := db.QueryContext(ctx, discoverySQL, dbName)
	if err != nil {
		return nil, fmt.Errorf("discover player tables: %w", err)
	}
	defer rows.Close()

	var pairs []tableColumn
	for rows.Next() {
		var pair tableColumn
		if err := rows.Scan(&pair.table, &pair.column); err != nil {
			return nil, fmt.Errorf("scan discovered table metadata: %w", err)
		}
		pairs = append(pairs, pair)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate discovered tables: %w", err)
	}
	return pairs, nil
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
	case "player-bundle", "player_bundle", "bundle":
		return "playerbundle"
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

	words := strings.Fields(strings.ToLower(q))
	firstWord := words[0]
	switch firstWord {
	case "select", "show", "describe", "desc", "explain":
	default:
		return fmt.Errorf("only read-only SELECT/SHOW/DESCRIBE/EXPLAIN queries are allowed, got %q", firstWord)
	}
	// 首词检查拦不住带副作用的读语句:EXPLAIN ANALYZE 会真正执行被解释的 DML,INTO OUTFILE 写服务器文件,
	// 锁定读与用户锁会在线上主库上持锁。连接用的是业务写账号,这里按词组再拦一道;根治仍是只读账号。
	normalized := " " + strings.Join(words, " ") + " "
	for _, banned := range readOnlyBannedPhrases {
		if strings.Contains(normalized, banned) {
			return fmt.Errorf("query contains %q, which is not allowed in read-only mode", strings.TrimSpace(banned))
		}
	}
	return nil
}

var readOnlyBannedPhrases = []string{
	" explain analyze ", " analyze ", " into outfile ", " into dumpfile ", " into @",
	" for update", " for share", " lock in share mode",
	"get_lock(", "release_lock(", "sleep(", "benchmark(",
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

	// 导出的是线上玩家数据:文件只给本人读写(Windows 上权限位不生效,靠存放目录的 ACL)。
	file, err := os.OpenFile(resolvedPath, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0o600)
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
