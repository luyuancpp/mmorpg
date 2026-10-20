//go:build merge_integration

package main

// verify:save_barrier 在真 MySQL / Redis 上的读口径(r2-mz#1,2026-10-20 修复)。判定与汇总逻辑在 player_rows_barrier_test.go
// 用替身覆盖;这里只证替身证不了的两件事:
//   - sqlRowFenceReader 的 SELECT 在真 MySQL 上按 id 取对两列围栏(含 uint64 上界),没有行的 id 不出现;缺围栏列时报错
//     (调用方据此报 INFRA,不当作「没有行」)。
//   - readSaveBarrierState 在真 Redis 上只 HMGET pending 与两表 ACK:hash 里 C++ 写的 schema_gen / schema_writer 不影响结果,
//     读完 hash 原样在(只读)。
//
// 行围栏用一次性库 merge_zone_it_fence_db,本用例自建、自删:共享夹具 zone_901_db / zone_902_db 的玩家表没有围栏列,
// 给它们加列会改变其他用例的拷行列集。

import (
	"context"
	"testing"
)

const itFenceDB = "merge_zone_it_fence_db"

func TestIT_SQLRowFenceReader_ReadsBothFenceColumnsAndFailsWithoutThem(t *testing.T) {
	requireIT(t)
	ctx := context.Background()
	db := itOpen(t)
	t.Cleanup(func() { _, _ = db.Exec("DROP DATABASE IF EXISTS " + itFenceDB) })
	for _, q := range []string{
		"DROP DATABASE IF EXISTS " + itFenceDB,
		"CREATE DATABASE " + itFenceDB,
		"CREATE TABLE " + itFenceDB + ".player_database (player_id BIGINT UNSIGNED NOT NULL, " +
			"fence_owner_epoch BIGINT UNSIGNED NOT NULL DEFAULT 0, fence_save_seq BIGINT UNSIGNED NOT NULL DEFAULT 0, " +
			"payload MEDIUMBLOB, PRIMARY KEY (player_id))",
		// uint64 上界用字面量写入,不经驱动的参数转换。
		"INSERT INTO " + itFenceDB + ".player_database (player_id, fence_owner_epoch, fence_save_seq, payload) VALUES " +
			"(1, 8, 90, 'a'), (2, 18446744073709551615, 1, 'b'), (4, 0, 0, 'c')",
	} {
		if _, err := db.Exec(q); err != nil {
			t.Fatalf("%s: %v", q, err)
		}
	}

	read := sqlRowFenceReader(db)
	got, err := read(ctx, itFenceDB, "player_database", []uint64{3, 2, 1, 4})
	if err != nil {
		t.Fatal(err)
	}
	want := map[uint64]string{
		1: saveBarrierVersion(8, 90),
		2: saveBarrierVersion(18446744073709551615, 1),
		4: saveBarrierVersion(0, 0), // 零围栏照实返回,由 judgeSaveBarrier 判它与 pending 不同版
	}
	if len(got) != len(want) {
		t.Fatalf("fences = %v, want %v (id 3 has no row and must be absent)", got, want)
	}
	for id, v := range want {
		if got[id] != v {
			t.Errorf("player %d fence = %q, want %q", id, got[id], v)
		}
	}

	// 共享夹具表没有围栏列:必须报错,不能当成「这些人没有行」。
	if _, err := read(ctx, zoneDBName(itSrcZone), "player_database", []uint64{1}); err == nil {
		t.Fatal("a table without fence columns must make the read fail (the audit reports INFRA)")
	}
}

func TestIT_ReadSaveBarrierState_IgnoresSchemaGenAndWriterFields(t *testing.T) {
	requireIT(t)
	itReset(t)
	ctx := context.Background()
	shared := itRedis(t, itSharedRD)
	v := saveBarrierVersion(8, 90)
	if err := shared.Set(ctx, playerOwnerEpochKey(9501), "9", 0).Err(); err != nil {
		t.Fatal(err)
	}
	if err := shared.Set(ctx, playerOwnerEpochKey(9502), "0", 0).Err(); err != nil {
		t.Fatal(err)
	}
	barrier := playerSaveBarrierKey("9501")
	if err := shared.HSet(ctx, barrier, saveBarrierPendingField, v, saveBarrierAckTables[0], v, saveBarrierAckTables[1], v,
		"schema_gen", "3", "schema_writer", "6f1c2a9e-4b7d-4e0a-9c3f-2d8b1e5a7c40").Err(); err != nil {
		t.Fatal(err)
	}

	obs, required, err := readSaveBarrierState(ctx, shared, []uint64{9501, 9502, 9503})
	if err != nil {
		t.Fatal(err)
	}
	if len(required) != 1 || required[0] != 9501 {
		t.Fatalf("required = %v, want only 9501 (9502 has owner_epoch 0, 9503 has none)", required)
	}
	o := obs[9501]
	if o.Pending != v || o.Acks != [2]string{v, v} || o.OwnerRaw != "9" {
		t.Fatalf("observation of 9501 = %+v", *o)
	}
	if !obs[9502].OwnerPresent || obs[9502].OwnerRaw != "0" || obs[9503].OwnerPresent {
		t.Fatalf("owner observations: 9502=%+v 9503=%+v", *obs[9502], *obs[9503])
	}
	// 行围栏与屏障同版时判定通过:schema_gen / schema_writer 不是拒绝条件。
	o.RowFence, o.RowPresent = [2]string{v, v}, [2]bool{true, true}
	if verdict := judgeSaveBarrier(*o); verdict != barrierVerdictOK {
		t.Fatalf("judge = %q, want pass", verdict)
	}
	// 只读:hash 的五个字段都还在。
	if n, err := shared.HLen(ctx, barrier).Result(); err != nil || n != 5 {
		t.Fatalf("barrier hash fields after the read = %d (err %v), want 5", n, err)
	}
}
