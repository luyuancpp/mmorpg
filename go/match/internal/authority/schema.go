package authority

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"os"
	"strings"
	"time"

	"match/internal/config"
	pb "proto/match"
	teampb "proto/team"
	"schemamigrate"

	"github.com/go-sql-driver/mysql"
	"google.golang.org/protobuf/proto"
)

// Tables 是匹配、组队权威的唯一迁移清单；DDL 继续由正式 proto 描述生成。
func Tables() []proto.Message {
	return []proto.Message{
		&pb.MatchSpectatorRecord{}, &pb.MatchSpectatorIntentRecord{}, &pb.MatchSpectatorHeadRecord{},
		&pb.MatchNetworkProfileRecord{}, &pb.MatchAuthorityTicketRecord{}, &pb.MatchTicketRegionRecord{},
		&pb.MatchChallengeInvitationRecord{}, &pb.MatchGlobalPolicyRecord{}, &pb.MatchJoinIntentRecord{},
		&pb.MatchDirectIntentRecord{}, &pb.MatchFormationRecord{}, &pb.MatchRatingRecord{},
		&pb.MatchRatingAppliedRecord{}, &pb.MatchRankSourceRecord{}, &pb.MatchRankOutboxRecord{},
		&pb.MatchBattleCompletionRecord{}, &pb.MatchBattleSettlementSourceRecord{},
		&teampb.TeamAuthorityRecord{}, &teampb.TeamMembershipRecord{}, &teampb.TeamInviteIndexRecord{},
		&teampb.TeamActionReceiptRecord{}, &teampb.TeamLaunchRecord{}, &teampb.TeamLaunchRegionRecord{},
		&teampb.TeamApplicationIndexRecord{}, &teampb.TeamLaunchRoleRecord{},
	}
}

type schemaRunner func(context.Context, *sql.DB, schemamigrate.Options) (schemamigrate.Report, error)

// requireCurrentSchema 只读核对已迁移 schema，缺表、漂移或人工项都不能由常驻账号补建。
func requireCurrentSchema(ctx context.Context, db *sql.DB, database string, plan schemaRunner) error {
	report, err := plan(ctx, db, schemamigrate.Options{Database: database, Tables: Tables()})
	if err != nil {
		return fmt.Errorf("全球匹配权威 schema 核对失败: %w", err)
	}
	if !report.Clean() {
		return errors.New("全球匹配权威 schema 尚未就绪，使用迁移专用账号执行 match -f <配置> -migrate；人工项须先审阅，拒绝接流")
	}
	return nil
}

// Migrate 只执行显式 schema 迁移，不读取探测密钥或装配节点、消费者、匹配循环。
// 返回报告及错误原样交给 schemamigrate.ExitCode；改列只接受本次 CLI 的显式授权。
func Migrate(ctx context.Context, c config.GlobalMatchConf, allowModify bool) (schemamigrate.Report, error) {
	db, err := openAuthorityDatabase(ctx, c)
	if err != nil {
		return schemamigrate.Report{}, err
	}
	defer db.Close()
	report, err := schemamigrate.Up(ctx, db, schemamigrate.Options{
		Database: c.Database, Tables: Tables(), AllowModifyColumn: allowModify,
	})
	// 普通索引的历史漂移不由迁移器擅自改写；Job也不能将其误报为已可接流。
	if err == nil && len(report.Manual) == 0 {
		if indexErr := (&Store{DB: db}).CheckCandidateIndexes(ctx); indexErr != nil {
			report.Manual = append(report.Manual, fmt.Sprintf("迁移后候选索引仍需人工核对: %v", indexErr))
		}
	}
	return report, err
}

// openAuthorityDatabase 共用原强读池，迁移与常驻形态只在 schema 写权限上分离。
func openAuthorityDatabase(ctx context.Context, c config.GlobalMatchConf) (*sql.DB, error) {
	if c.Host == "" || c.User == "" || c.Database != "mmorpg_match" {
		return nil, errors.New("全球匹配数据库配置无效")
	}
	mc := mysql.NewConfig()
	mc.User, mc.Passwd = c.User, os.Getenv(c.PasswordEnv)
	mc.Net, mc.Addr, mc.DBName = "tcp", c.Host, c.Database
	mc.ParseTime = true
	mc.Timeout, mc.ReadTimeout, mc.WriteTimeout = 3*time.Second, 3*time.Second, 3*time.Second
	mc.Params = map[string]string{"charset": "utf8mb4", "sql_mode": "'STRICT_TRANS_TABLES'", "transaction_isolation": "'READ-COMMITTED'"}
	if c.RequireTiDB {
		for k, v := range map[string]string{"tidb_txn_mode": "'pessimistic'", "tidb_read_staleness": "0", "tidb_low_resolution_tso": "OFF", "tidb_replica_read": "'leader'", "tidb_read_consistency": "'strict'", "tidb_snapshot": "''"} {
			mc.Params[k] = v
		}
	}
	db, err := sql.Open("mysql", mc.FormatDSN())
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(16)
	db.SetMaxIdleConns(4)
	ok := false
	defer func() {
		if !ok {
			db.Close()
		}
	}()
	if err = db.PingContext(ctx); err != nil {
		return nil, errors.New("匹配权威数据库不可用")
	}
	var database, version, comment string
	if err = db.QueryRowContext(ctx, "SELECT DATABASE(),VERSION(),@@version_comment").Scan(&database, &version, &comment); err != nil {
		return nil, err
	}
	if database != c.Database || (c.RequireTiDB && !strings.Contains(strings.ToLower(version+comment), "tidb")) {
		return nil, errors.New("匹配权威事务域校验失败")
	}
	if c.RequireTiDB {
		var mode, isolation, replica, consistency, snapshot string
		var stale, low int64
		err = db.QueryRowContext(ctx, "SELECT @@tidb_txn_mode,@@transaction_isolation,@@tidb_replica_read,@@tidb_read_consistency,@@tidb_snapshot,@@tidb_read_staleness,@@tidb_low_resolution_tso").Scan(&mode, &isolation, &replica, &consistency, &snapshot, &stale, &low)
		if err != nil || mode != "pessimistic" || isolation != "READ-COMMITTED" || replica != "leader" || consistency != "strict" || snapshot != "" || stale != 0 || low != 0 {
			return nil, errors.New("匹配权威连接必须使用完整TiDB强读策略")
		}
	}
	ok = true
	return db, nil
}
