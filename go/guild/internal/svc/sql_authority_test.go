package svc

import (
	"context"
	"strings"
	"testing"

	"guild/internal/config"
)

func TestGuildSQLRefusesWrongDatabaseAndRedactsInvalidDSN(t *testing.T) {
	for _, open := range []struct {
		name string
		open func(context.Context, config.MySQLConf) error
	}{
		{"runtime", func(ctx context.Context, cfg config.MySQLConf) error {
			db, err := OpenMySQL(ctx, cfg)
			if db != nil {
				db.Close()
			}
			return err
		}},
		{"migration", func(ctx context.Context, cfg config.MySQLConf) error {
			db, err := OpenSchemaMySQL(ctx, cfg)
			if db != nil {
				db.Close()
			}
			return err
		}},
	} {
		t.Run(open.name, func(t *testing.T) {
			for _, dsn := range []string{"user:must-not-leak@tcp(::broken", "user:must-not-leak@tcp(127.0.0.1:1)/wrong_database"} {
				err := open.open(context.Background(), config.MySQLConf{DataSource: dsn, MaxOpenConn: 20})
				if err == nil {
					t.Fatal("非法DSN/错误库必须在拨号前拒绝")
				}
				if strings.Contains(err.Error(), "must-not-leak") {
					t.Fatalf("连接错误泄露密码: %v", err)
				}
			}
		})
	}
}
