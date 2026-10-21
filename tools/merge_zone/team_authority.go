package main

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/go-sql-driver/mysql"
)

const defaultTeamSchema = "mmorpg_match"

type teamAuthorityReader interface {
	PlayersInTeams(context.Context, []uint64) (int, error)
}
type sqlTeamAuthority struct{ db *sql.DB }

func requireTeamRouteDomain(gameDSN, routeDSN, source string) error {
	if source != routeSourceDB {
		return errors.New("全球Team合区只允许-route-source=db及SQL全部64片围栏")
	}
	game, err := mysql.ParseDSN(gameDSN)
	if err != nil {
		return errors.New("全球SQL连接配置无效")
	}
	route, err := mysql.ParseDSN(routeDSN)
	if err != nil {
		return errors.New("全局路由连接配置无效")
	}
	if game.Net != route.Net || game.Addr != route.Addr {
		return errors.New("组队与路由围栏必须使用同一全球TiDB SQL入口")
	}
	return nil
}

// 合区P7读取全球Team原件，独立池固定当前TiDB强读；不把区域Redis缓存用于无队证明。
func openTeamAuthority(ctx context.Context, dsn, schema string) (*sqlTeamAuthority, error) {
	if !schemaNamePattern.MatchString(schema) {
		return nil, errors.New("team-schema必须是有效的SQL标识符")
	}
	cfg, err := mysql.ParseDSN(dsn)
	if err != nil {
		return nil, errors.New("组队权威连接配置无效")
	}
	cfg.DBName = schema
	cfg.Timeout = 3 * time.Second
	cfg.ReadTimeout = 3 * time.Second
	cfg.WriteTimeout = 3 * time.Second
	pinCurrentTiDB(cfg)
	db, err := sql.Open("mysql", cfg.FormatDSN())
	if err != nil {
		return nil, errors.New("组队权威连接不可用")
	}
	db.SetMaxOpenConns(2)
	db.SetMaxIdleConns(1)
	ready := false
	defer func() {
		if !ready {
			db.Close()
		}
	}()
	if err = requireCurrentTiDB(ctx, db); err != nil {
		return nil, err
	}
	rows, err := db.QueryContext(ctx, "SELECT player_id,team_id,epoch FROM team_membership WHERE 1=0")
	if err != nil {
		return nil, errors.New("组队SQL成员原件表缺失")
	}
	if err = rows.Close(); err != nil {
		return nil, err
	}
	ready = true
	return &sqlTeamAuthority{db: db}, nil
}
func (s *sqlTeamAuthority) PlayersInTeams(ctx context.Context, ids []uint64) (int, error) {
	if s == nil || s.db == nil {
		return 0, errors.New("缺少组队SQL权威读连接")
	}
	found := 0
	for _, batch := range chunkUint64(ids, 500) {
		marks := make([]string, len(batch))
		args := make([]any, len(batch))
		for i, player := range batch {
			marks[i] = "?"
			args[i] = player
		}
		if len(args) == 0 {
			continue
		}
		rows, err := s.db.QueryContext(ctx, "SELECT player_id,team_id,epoch FROM team_membership WHERE player_id IN ("+strings.Join(marks, ",")+")", args...)
		if err != nil {
			return found, err
		}
		for rows.Next() {
			var player, team, epoch uint64
			if err = rows.Scan(&player, &team, &epoch); err != nil {
				rows.Close()
				return found, err
			}
			if team != 0 {
				if epoch == 0 {
					rows.Close()
					return found, fmt.Errorf("组队成员原件缺epoch")
				}
				found++
			}
		}
		err = rows.Err()
		closeErr := rows.Close()
		if err != nil {
			return found, err
		}
		if closeErr != nil {
			return found, closeErr
		}
	}
	return found, nil
}
