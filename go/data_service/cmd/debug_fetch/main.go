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
	"sort"
	"strings"
	"time"
	"unicode/utf8"

	"data_service/internal/config"
	"data_service/internal/logic"
	"data_service/internal/routing"
	"data_service/internal/store"
	"data_service/internal/svc"

	_ "github.com/go-sql-driver/mysql"
	"github.com/zeromicro/go-zero/core/conf"
)

type renderedValue struct {
	Kind      string `json:"kind"`
	Size      int    `json:"size"`
	Text      string `json:"text,omitempty"`
	JSONValue any    `json:"json,omitempty"`
	Base64    string `json:"base64,omitempty"`
}

type dbConfigFile struct {
	ZoneId       uint32 `json:"ZoneId"`
	ServerConfig struct {
		Database struct {
			Hosts       string `json:"Hosts"`
			User        string `json:"User"`
			Passwd      string `json:"Passwd"`
			DBName      string `json:"DBName,optional"`
			MaxOpenConn int    `json:"MaxOpenConn"`
			MaxIdleConn int    `json:"MaxIdleConn"`
			Net         string `json:"Net"`
		} `json:"Database"`
	} `json:"ServerConfig"`
}

func main() {
	var (
		configPath     = flag.String("config", "etc/data_service.yaml", "path to data_service config")
		dbConfigPath   = flag.String("db-config", "../db/etc/db.yaml", "path to db config for SQL mode")
		mode           = flag.String("mode", "player", "debug mode: player | zone | snapshot | sql")
		playerID       = flag.Uint64("player", 0, "player id")
		zoneID         = flag.Uint("zone", 0, "zone/server id")
		fieldsCSV      = flag.String("fields", "", "comma-separated player fields; empty means all")
		limit          = flag.Int("limit", 20, "max rows or player ids to print")
		sqlQuery       = flag.String("sql", "", "read-only SQL query for sql mode")
		timeoutSeconds = flag.Int("timeout-seconds", 10, "request timeout in seconds")
	)
	flag.Parse()

	if *limit <= 0 {
		*limit = 20
	}
	if *timeoutSeconds <= 0 {
		*timeoutSeconds = 10
	}

	ctx, cancel := context.WithTimeout(context.Background(), time.Duration(*timeoutSeconds)*time.Second)
	defer cancel()

	var err error
	switch strings.ToLower(strings.TrimSpace(*mode)) {
	case "player":
		err = runPlayerMode(ctx, *configPath, *playerID, parseFields(*fieldsCSV))
	case "zone":
		err = runZoneMode(ctx, *configPath, uint32(*zoneID), *limit)
	case "snapshot":
		err = runSnapshotMode(ctx, *configPath, *playerID, *limit)
	case "sql":
		err = runSQLMode(ctx, *dbConfigPath, uint32(*zoneID), *sqlQuery, *limit)
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

	svcCtx, cleanup, err := newDebugServiceContext(configPath)
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
		decoded[name] = renderValue(value)
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

	svcCtx, cleanup, err := newDebugServiceContext(configPath)
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

	return writeJSON(map[string]any{
		"mode":             "zone",
		"zone_id":          zoneID,
		"players_total":    len(playerIDs),
		"players_returned": len(shown),
		"player_ids":       shown,
	})
}

func runSnapshotMode(ctx context.Context, configPath string, playerID uint64, limit int) error {
	if playerID == 0 {
		return errors.New("-player is required for snapshot mode")
	}

	svcCtx, cleanup, err := newDebugServiceContext(configPath)
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

func runSQLMode(ctx context.Context, dbConfigPath string, zoneID uint32, query string, limit int) error {
	if err := ensureReadOnlyQuery(query); err != nil {
		return err
	}

	cfg, err := loadDBConfig(dbConfigPath)
	if err != nil {
		return err
	}

	if zoneID == 0 {
		zoneID = cfg.ZoneId
	}
	if zoneID == 0 {
		return errors.New("-zone is required for sql mode when db config does not include ZoneId")
	}

	dbName := strings.TrimSpace(cfg.ServerConfig.Database.DBName)
	if dbName == "" {
		dbName = fmt.Sprintf("zone_%d_db", zoneID)
	}

	network := strings.TrimSpace(cfg.ServerConfig.Database.Net)
	if network == "" {
		network = "tcp"
	}

	dsn := fmt.Sprintf("%s:%s@%s(%s)/%s?charset=utf8mb4&parseTime=true&loc=Local",
		cfg.ServerConfig.Database.User,
		cfg.ServerConfig.Database.Passwd,
		network,
		cfg.ServerConfig.Database.Hosts,
		dbName,
	)

	db, err := sql.Open("mysql", dsn)
	if err != nil {
		return fmt.Errorf("open mysql connection: %w", err)
	}
	defer db.Close()

	db.SetMaxOpenConns(1)
	db.SetMaxIdleConns(1)

	if err := db.PingContext(ctx); err != nil {
		return fmt.Errorf("ping mysql: %w", err)
	}

	rows, err := db.QueryContext(ctx, query)
	if err != nil {
		return fmt.Errorf("execute sql query: %w", err)
	}
	defer rows.Close()

	cols, err := rows.Columns()
	if err != nil {
		return fmt.Errorf("read columns: %w", err)
	}

	resultRows := make([]map[string]any, 0, limit)
	for rows.Next() {
		values := make([]any, len(cols))
		scans := make([]any, len(cols))
		for i := range values {
			scans[i] = &values[i]
		}
		if err := rows.Scan(scans...); err != nil {
			return fmt.Errorf("scan row: %w", err)
		}

		rowMap := make(map[string]any, len(cols))
		for i, col := range cols {
			rowMap[col] = normalizeSQLValue(values[i])
		}
		resultRows = append(resultRows, rowMap)
		if len(resultRows) >= limit {
			break
		}
	}
	if err := rows.Err(); err != nil {
		return fmt.Errorf("iterate rows: %w", err)
	}

	return writeJSON(map[string]any{
		"mode":          "sql",
		"zone_id":       zoneID,
		"database":      dbName,
		"columns":       cols,
		"rows_returned": len(resultRows),
		"rows":          resultRows,
	})
}

func newDebugServiceContext(configPath string) (*svc.ServiceContext, func(), error) {
	var c config.Config
	if err := mustLoadConfig(configPath, &c); err != nil {
		return nil, nil, fmt.Errorf("load data_service config: %w", err)
	}

	svcCtx := &svc.ServiceContext{
		Config: c,
		Router: routing.NewRouter(c),
	}

	if strings.TrimSpace(c.SnapshotMySQL.Host) != "" {
		ss, err := store.NewSnapshotStore(store.MySQLConfig{
			Host:        c.SnapshotMySQL.Host,
			User:        c.SnapshotMySQL.User,
			Password:    c.SnapshotMySQL.Password,
			DBName:      c.SnapshotMySQL.DBName,
			MaxOpenConn: c.SnapshotMySQL.MaxOpenConn,
			MaxIdleConn: c.SnapshotMySQL.MaxIdleConn,
		})
		if err == nil {
			svcCtx.SnapshotStore = ss
		}
	}

	cleanup := func() {
		if svcCtx.Router != nil {
			svcCtx.Router.Close()
		}
		if svcCtx.SnapshotStore != nil {
			svcCtx.SnapshotStore.Close()
		}
	}

	return svcCtx, cleanup, nil
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
	decoded := renderedValue{Size: len(data)}
	if len(data) == 0 {
		decoded.Kind = "text"
		decoded.Text = ""
		return decoded
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

	decoded.Kind = "base64"
	decoded.Base64 = base64.StdEncoding.EncodeToString(data)
	return decoded
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

func normalizeSQLValue(value any) any {
	switch v := value.(type) {
	case nil:
		return nil
	case []byte:
		if utf8.Valid(v) {
			return string(v)
		}
		return base64.StdEncoding.EncodeToString(v)
	case time.Time:
		return v.Format(time.RFC3339)
	default:
		return v
	}
}

func loadDBConfig(path string) (*dbConfigFile, error) {
	var cfg dbConfigFile
	if err := mustLoadConfig(path, &cfg); err != nil {
		return nil, fmt.Errorf("load db config: %w", err)
	}
	return &cfg, nil
}

func mustLoadConfig(path string, target any) (err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("%v", r)
		}
	}()
	conf.MustLoad(path, target)
	return nil
}

func writeJSON(v any) error {
	encoder := json.NewEncoder(os.Stdout)
	encoder.SetIndent("", "  ")
	return encoder.Encode(v)
}
