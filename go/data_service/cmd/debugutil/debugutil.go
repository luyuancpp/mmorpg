// Package debugutil contains shared helpers for debug_fetch / debug_import CLI tools.
package debugutil

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"unicode"

	"data_service/internal/config"
	"data_service/internal/routing"
	"data_service/internal/store"
	"data_service/internal/svc"

	_ "github.com/go-sql-driver/mysql"
	"github.com/zeromicro/go-zero/core/conf"
)

// DBConfigFile mirrors the DB yaml structure shared by debug_fetch and debug_import.
type DBConfigFile struct {
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

// MustLoadConfig loads a YAML config file via go-zero, recovering panics.
func MustLoadConfig(path string, target any) (err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("%v", r)
		}
	}()
	conf.MustLoad(path, target)
	return nil
}

// LoadDBConfig reads and parses a db.yaml style config.
func LoadDBConfig(path string) (*DBConfigFile, error) {
	var cfg DBConfigFile
	if err := MustLoadConfig(path, &cfg); err != nil {
		return nil, fmt.Errorf("load db config: %w", err)
	}
	return &cfg, nil
}

// OpenZoneDB opens a MySQL connection for a specific zone.
func OpenZoneDB(ctx context.Context, dbConfigPath string, zoneID uint32) (*sql.DB, uint32, string, error) {
	cfg, err := LoadDBConfig(dbConfigPath)
	if err != nil {
		return nil, 0, "", err
	}

	if zoneID == 0 {
		zoneID = cfg.ZoneId
	}
	if zoneID == 0 {
		return nil, 0, "", errors.New("-zone is required when db config does not include ZoneId")
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
		return nil, 0, "", fmt.Errorf("open mysql connection: %w", err)
	}

	db.SetMaxOpenConns(1)
	db.SetMaxIdleConns(1)
	if err := db.PingContext(ctx); err != nil {
		db.Close()
		return nil, 0, "", fmt.Errorf("ping mysql: %w", err)
	}

	return db, zoneID, dbName, nil
}

// QuoteIdentifier validates and quotes a SQL identifier.
func QuoteIdentifier(name string) (string, error) {
	if strings.TrimSpace(name) == "" {
		return "", errors.New("sql identifier must not be empty")
	}
	for _, r := range name {
		if !(unicode.IsLetter(r) || unicode.IsDigit(r) || r == '_') {
			return "", fmt.Errorf("unsafe sql identifier %q", name)
		}
	}
	return "`" + name + "`", nil
}

// NewServiceContext builds a lightweight ServiceContext with Router (no snapshot store).
func NewServiceContext(configPath string) (*svc.ServiceContext, func(), error) {
	var c config.Config
	if err := MustLoadConfig(configPath, &c); err != nil {
		return nil, nil, fmt.Errorf("load data_service config: %w", err)
	}

	svcCtx := &svc.ServiceContext{
		Config: c,
		Router: routing.NewRouter(c),
	}

	cleanup := func() {
		if svcCtx.Router != nil {
			svcCtx.Router.Close()
		}
	}

	return svcCtx, cleanup, nil
}

// NewServiceContextWithSnapshot builds a ServiceContext that also initialises SnapshotStore.
func NewServiceContextWithSnapshot(configPath string) (*svc.ServiceContext, func(), error) {
	svcCtx, baseCleanup, err := NewServiceContext(configPath)
	if err != nil {
		return nil, nil, err
	}

	c := svcCtx.Config
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
		if svcCtx.SnapshotStore != nil {
			svcCtx.SnapshotStore.Close()
		}
		baseCleanup()
	}

	return svcCtx, cleanup, nil
}
