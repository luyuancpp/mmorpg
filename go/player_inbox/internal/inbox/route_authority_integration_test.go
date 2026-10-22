//go:build integration

package inbox

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"go/ast"
	"go/parser"
	"go/token"
	"path/filepath"
	"strconv"
	"testing"
	"time"

	inboxpb "proto/player_inbox"
	"shared/placement"

	"google.golang.org/protobuf/proto"
)

// 只在已断言的自有白名单库内执行；路由的可空列、CHECK与主键直接消费DS唯一DDL。
// proto通用迁移会把NULL渲染成空值，不能用它替代正式路由表并假称验证维护围栏。
func createInboxFixtureRouteTables(ctx context.Context, db *sql.DB, schema string) error {
	if !inboxTestDBNamePattern.MatchString(schema) {
		return errors.New("路由夹具目标不是一次性Inbox白名单库")
	}
	var current string
	if err := db.QueryRowContext(ctx, "SELECT DATABASE()").Scan(&current); err != nil {
		return err
	}
	if current != schema {
		return errors.New("路由夹具连接库与已核白名单不一致")
	}
	path := filepath.Join("..", "..", "..", "data_service", "internal", "store", "schema.go")
	f, err := parser.ParseFile(token.NewFileSet(), path, nil, parser.SkipObjectResolution)
	if err != nil {
		return fmt.Errorf("读取DS正规路由DDL: %w", err)
	}
	names := []string{"playerRouteBootstrapDDL", "zoneMergeFenceBootstrapDDL", "zoneMergedIntoBootstrapDDL", "routeMetaBootstrapDDL", "routeReconcileBootstrapDDL"}
	values := map[string]string{}
	for _, decl := range f.Decls {
		gd, ok := decl.(*ast.GenDecl)
		if !ok || gd.Tok != token.CONST {
			continue
		}
		for _, spec := range gd.Specs {
			vs, ok := spec.(*ast.ValueSpec)
			if !ok {
				continue
			}
			for i, name := range vs.Names {
				for _, want := range names {
					if name.Name != want {
						continue
					}
					if _, duplicate := values[want]; duplicate || i >= len(vs.Values) {
						return fmt.Errorf("DS DDL %s必须是唯一显式常量", want)
					}
					value, err := inboxFixtureDDLString(vs.Values[i])
					if err != nil {
						return fmt.Errorf("DS DDL %s: %w", want, err)
					}
					values[want] = value
				}
			}
		}
	}
	for _, name := range names {
		if values[name] == "" {
			return fmt.Errorf("DS正规路由DDL缺少%s", name)
		}
	}
	for _, table := range []string{"route_reconcile", "route_meta", "zone_merged_into", "zone_merge_fence", "player_route"} {
		if _, err = db.ExecContext(ctx, "DROP TABLE IF EXISTS "+table); err != nil {
			return err
		}
	}
	for _, name := range names {
		if _, err = db.ExecContext(ctx, values[name]); err != nil {
			return fmt.Errorf("执行DS正规路由DDL %s: %w", name, err)
		}
	}
	if _, err = db.ExecContext(ctx, "INSERT INTO route_meta(id,generation,writable) VALUES(1,'inbox-fixture-route-v1',1)"); err != nil {
		return err
	}
	_, err = db.ExecContext(ctx, "INSERT INTO route_reconcile(id) VALUES(1)")
	return err
}

func inboxFixtureDDLString(expr ast.Expr) (string, error) {
	switch value := expr.(type) {
	case *ast.BasicLit:
		if value.Kind == token.STRING {
			return strconv.Unquote(value.Value)
		}
	case *ast.BinaryExpr:
		if value.Op == token.ADD {
			left, err := inboxFixtureDDLString(value.X)
			if err != nil {
				return "", err
			}
			right, err := inboxFixtureDDLString(value.Y)
			return left + right, err
		}
	}
	return "", errors.New("只接受字符串及加号拼接，DS DDL表达式变更须重新审阅")
}

func ensureInboxFixtureRoute(ctx context.Context, db *sql.DB, player uint64) error {
	var existing uint64
	err := db.QueryRowContext(ctx, "SELECT player_id FROM player_route WHERE route_shard=? AND player_id=?", placement.RouteRowShard(player), player).Scan(&existing)
	if err == nil {
		return nil // 不重置已有的frozen/home/rev，也不在持P时偷偷补路由写锁。
	}
	if !errors.Is(err, sql.ErrNoRows) {
		return err
	}
	_, err = db.ExecContext(ctx, "INSERT INTO player_route(route_shard,player_id,home_zone,rev,updated_by,created_at_ms,updated_at_ms) VALUES(?,?,1,1,'inbox-fixture',1,1) ON DUPLICATE KEY UPDATE player_id=player_id", placement.RouteRowShard(player), player)
	return err
}

func setInboxFixtureMergeFence(t *testing.T, e *itEnv, player uint64) {
	t.Helper()
	shard, err := placement.RouteFenceShard(player, placement.MaxRouteFenceShards)
	if err != nil {
		t.Fatal(err)
	}
	_, err = e.db.ExecContext(itCtx(t), "INSERT INTO zone_merge_fence(zone_id,shard,run_id,expires_at_ms) VALUES(1,?,'inbox-it-merge',CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED)+60000) ON DUPLICATE KEY UPDATE run_id=VALUES(run_id),expires_at_ms=VALUES(expires_at_ms)", shard)
	if err != nil {
		t.Fatal(err)
	}
}

func TestIntegrationInboxEveryMutationHonorsActualMergeFence(t *testing.T) {
	e := openIntegration(t)
	const player = uint64(8201) // shard=9，不能误用固定63号片。
	first := e.mustEnqueue(t, player, 1)
	e.epochs.set(player, 1)
	admission := e.admission(t, player, 1)
	beforeItem, beforeStream, beforeAudit := e.item(t, player, first.Seq), e.stream(t, player), auditCount(t, e.db, player)
	setInboxFixtureMergeFence(t, e, player)
	actions := []struct {
		name string
		call func() error
	}{
		{"Enqueue", func() error { _, err := e.enqueue(t, player, 2, testNowMs, gainPayload(t, 2)); return err }},
		{"Fetch", func() error {
			_, err := e.store.Fetch(itCtx(t), &inboxpb.FetchPlayerInboxRequest{PlayerId: player, Domain: gmDomain, OwnerEpoch: 1, Admission: admission})
			return err
		}},
		{"Ack", func() error {
			_, err := e.store.Ack(itCtx(t), &inboxpb.AckPlayerInboxRequest{PlayerId: player, Domain: gmDomain, OwnerEpoch: 1, Admission: admission, Decisions: []*inboxpb.PlayerInboxDecisionReport{dec(first.Seq, inboxpb.PlayerInboxOutcome_PLAYER_INBOX_OUTCOME_APPLIED, 0, 1)}})
			return err
		}},
		{"Void", func() error {
			_, err := e.store.Void(itCtx(t), VoidParams{PlayerID: player, Domain: gmDomain, Seq: first.Seq, Reason: inboxpb.PlayerInboxVoidReason_PLAYER_INBOX_VOID_CANCELLED, Operator: "fixture"}, nil)
			return err
		}},
		{"ClearNotify", func() error {
			_, err := e.store.ClearNotifyDue(itCtx(t), player, gmDomain, beforeStream.NextSeq)
			return err
		}},
		{"DeferNotify", func() error {
			_, err := e.store.DeferNotifyDue(itCtx(t), player, gmDomain, beforeStream.NextSeq, testNowMs+1000)
			return err
		}},
	}
	for _, action := range actions {
		t.Run(action.name, func(t *testing.T) {
			if err := action.call(); !errors.Is(err, placement.ErrRoleRouteFenced) {
				t.Fatalf("实际merge围栏必须拒绝本次提交: %v", err)
			}
		})
	}
	if !proto.Equal(beforeItem, e.item(t, player, first.Seq)) || !proto.Equal(beforeStream, e.stream(t, player)) || beforeAudit != auditCount(t, e.db, player) {
		t.Fatal("维护围栏拒绝后Inbox原行/stream/审计发生变化")
	}
}

func TestIntegrationInboxMergeWinsBeforePWithoutLeavingNewItem(t *testing.T) {
	e := openIntegration(t)
	const player = uint64(8202)
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if _, err := (itPlayers{missing: map[uint64]bool{}, db: e.db}).PlayerExists(ctx, player); err != nil {
		t.Fatal(err)
	}
	shard, err := placement.RouteFenceShard(player, placement.MaxRouteFenceShards)
	if err != nil {
		t.Fatal(err)
	}
	tx, err := e.db.BeginTx(ctx, &sql.TxOptions{Isolation: sql.LevelReadCommitted})
	if err != nil {
		t.Fatal(err)
	}
	defer tx.Rollback()
	// 与merge持有者相同：先片后route/P，不制造P后补片的非法锁序。
	if _, err = tx.ExecContext(ctx, "INSERT INTO zone_merge_fence(zone_id,shard,run_id,expires_at_ms) VALUES(1,?,'inbox-it-merge-race',CAST(UNIX_TIMESTAMP(CURRENT_TIMESTAMP(3))*1000 AS UNSIGNED)+60000)", shard); err != nil {
		t.Fatal(err)
	}
	observed := &observedSQLRoles{inner: e.store.roles, entered: make(chan struct{})}
	e.store.roles = observed
	result := make(chan error, 1)
	payload := gainPayload(t, 1)
	go func() {
		_, err := e.store.Enqueue(ctx, EnqueueParams{PlayerID: player, Type: gmBlock, Producer: inboxpb.PlayerInboxProducerKind_PLAYER_INBOX_PRODUCER_GM, RequestID: 1, IssuedAtMs: testNowMs, Payload: payload, Operator: "fixture"}, nil)
		result <- err
	}()
	select {
	case <-observed.entered:
	case <-ctx.Done():
		t.Fatal("入箱未到真实SQL路由前缀")
	}
	if err = tx.Commit(); err != nil {
		t.Fatal(err)
	}
	select {
	case err = <-result:
		if !errors.Is(err, placement.ErrRoleRouteFenced) {
			t.Fatalf("先提交的原home维护围栏必须胜出: %v", err)
		}
	case <-ctx.Done():
		t.Fatal("围栏提交后入箱未结束")
	}
	var count int
	if err = e.db.QueryRowContext(ctx, "SELECT COUNT(*) FROM player_inbox_item WHERE shard=? AND player_id=?", RowShard(player), player).Scan(&count); err != nil || count != 0 {
		t.Fatalf("merge胜出后仍留下Inbox条目: count=%d err=%v", count, err)
	}
}

func TestIntegrationInboxRouteFrozenOrMetaUnknownRejectsEvenEmptyFetch(t *testing.T) {
	e := openIntegration(t)
	const player = uint64(8203)
	e.epochs.set(player, 1)
	admission := e.admission(t, player, 1)
	for _, update := range []string{"UPDATE route_meta SET writable=0 WHERE id=1", "UPDATE route_meta SET writable=1 WHERE id=1", "UPDATE player_route SET storage_id=1,placement_version=1,frozen_run_id='inbox-it-frozen' WHERE player_id=8203"} {
		if _, err := e.db.ExecContext(itCtx(t), update); err != nil {
			t.Fatal(err)
		}
		if update == "UPDATE route_meta SET writable=1 WHERE id=1" {
			continue
		}
		_, err := e.store.Fetch(itCtx(t), &inboxpb.FetchPlayerInboxRequest{PlayerId: player, Domain: gmDomain, OwnerEpoch: 1, Admission: admission})
		if !errors.Is(err, placement.ErrRoleRouteFenced) {
			t.Fatalf("空箱也必须核实际路由/维护开关: %v", err)
		}
	}
	var schema string
	if err := e.db.QueryRowContext(itCtx(t), "SELECT DATABASE()").Scan(&schema); err != nil {
		t.Fatal(err)
	}
	if _, err := e.db.ExecContext(itCtx(t), "DROP TABLE route_meta"); err != nil {
		t.Fatal(err)
	}
	if _, err := NewSQLRoleAuthority(itCtx(t), e.db, schema); err == nil {
		t.Fatal("启动缺正式路由表必须拒绝")
	}
}
