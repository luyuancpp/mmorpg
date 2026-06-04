//go:build integration

package noderegistry

// 集成测试:需要本地 etcd (127.0.0.1:2379) 在跑。
// 启动 etcd: pwsh tools/scripts/dev_tools.ps1 -Command etcd-up
// 跑测试:    go test -tags=integration ./internal/noderegistry/...

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/google/uuid"
	clientv3 "go.etcd.io/etcd/client/v3"

	base "proto/common/base"
)

const testEtcdEndpoint = "127.0.0.1:2379"

var nodeTypeCounter uint32 = 1300000

func uniqueNodeType() uint32 {
	nodeTypeCounter++
	return nodeTypeCounter
}

func newTestClient(t *testing.T) *clientv3.Client {
	t.Helper()
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{testEtcdEndpoint},
		DialTimeout: 3 * time.Second,
	})
	if err != nil {
		t.Skipf("etcd not available at %s: %v", testEtcdEndpoint, err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	if _, err := cli.Get(ctx, "_ping"); err != nil {
		cli.Close()
		t.Skipf("etcd ping failed: %v", err)
	}
	return cli
}

func cleanupNodeType(t *testing.T, cli *clientv3.Client, nodeType uint32) {
	t.Helper()
	prefix := fmt.Sprintf("%s.rpc", base.ENodeType_name[int32(nodeType)])
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	_, _ = cli.Delete(ctx,
		fmt.Sprintf("%s/allocated/node_type/%d/", prefix, nodeType),
		clientv3.WithPrefix())
	resp, err := cli.Get(ctx, prefix+"/zone/", clientv3.WithPrefix(), clientv3.WithKeysOnly())
	if err == nil {
		needle := fmt.Sprintf("/node_type/%d/", nodeType)
		for _, kv := range resp.Kvs {
			k := string(kv.Key)
			for i := 0; i+len(needle) <= len(k); i++ {
				if k[i:i+len(needle)] == needle {
					_, _ = cli.Delete(ctx, k)
					break
				}
			}
		}
	}
}

func makeInfo(nodeType, zoneID uint32) *base.NodeInfo {
	return &base.NodeInfo{
		NodeType:     nodeType,
		ZoneId:       zoneID,
		NodeUuid:     uuid.New().String(),
		LaunchTime:   uint64(time.Now().Unix()),
		ProtocolType: uint32(base.ENodeProtocolType_PROTOCOL_GRPC),
		Endpoint:     &base.EndpointComp{Ip: "127.0.0.1", Port: 9000},
	}
}

func grantLease(t *testing.T, cli *clientv3.Client, ttl int64) clientv3.LeaseID {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	resp, err := cli.Grant(ctx, ttl)
	if err != nil {
		t.Fatalf("grant lease: %v", err)
	}
	return resp.ID
}

func TestAllocStartsFromOne(t *testing.T) {
	cli := newTestClient(t)
	defer cli.Close()
	nt := uniqueNodeType()
	defer cleanupNodeType(t, cli, nt)

	info := makeInfo(nt, 1)
	lease := grantLease(t, cli, 30)
	defer cli.Revoke(context.Background(), lease)

	id, err := allocateNodeID(context.Background(), cli, rpcPrefix(nt), info, lease)
	if err != nil {
		t.Fatalf("allocate: %v", err)
	}
	if id != 1 {
		t.Fatalf("expected first allocation=1, got %d", id)
	}
}

func TestAllocSkipsZero(t *testing.T) {
	cli := newTestClient(t)
	defer cli.Close()
	nt := uniqueNodeType()
	defer cleanupNodeType(t, cli, nt)

	prefix := rpcPrefix(nt)
	leases := make([]clientv3.LeaseID, 0, 4)
	defer func() {
		for _, l := range leases {
			cli.Revoke(context.Background(), l)
		}
	}()

	for i := 0; i < 3; i++ {
		l := grantLease(t, cli, 30)
		leases = append(leases, l)
		id, err := allocateNodeID(context.Background(), cli, prefix, makeInfo(nt, 1), l)
		if err != nil {
			t.Fatalf("alloc #%d: %v", i, err)
		}
		if id == 0 {
			t.Fatalf("allocator returned 0")
		}
	}

	l := grantLease(t, cli, 30)
	leases = append(leases, l)
	id, err := allocateNodeID(context.Background(), cli, prefix, makeInfo(nt, 1), l)
	if err != nil {
		t.Fatalf("alloc 4th: %v", err)
	}
	if id != 4 {
		t.Fatalf("expected id=4, got %d", id)
	}
}

func TestAllocCrossZoneUnique(t *testing.T) {
	cli := newTestClient(t)
	defer cli.Close()
	nt := uniqueNodeType()
	defer cleanupNodeType(t, cli, nt)

	prefix := rpcPrefix(nt)
	const perZone = 20
	zones := []uint32{1, 2, 3}

	var mu sync.Mutex
	allIDs := make(map[uint32]bool)
	leases := make([]clientv3.LeaseID, 0, len(zones)*perZone)

	defer func() {
		for _, l := range leases {
			cli.Revoke(context.Background(), l)
		}
	}()

	var wg sync.WaitGroup
	errCh := make(chan error, len(zones)*perZone)

	for _, z := range zones {
		for i := 0; i < perZone; i++ {
			wg.Add(1)
			go func(zone uint32) {
				defer wg.Done()
				l := grantLease(t, cli, 30)
				mu.Lock()
				leases = append(leases, l)
				mu.Unlock()

				id, err := allocateNodeID(context.Background(), cli, prefix, makeInfo(nt, zone), l)
				if err != nil {
					errCh <- err
					return
				}
				mu.Lock()
				if allIDs[id] {
					errCh <- fmt.Errorf("duplicate node_id=%d (zone=%d)", id, zone)
				}
				allIDs[id] = true
				mu.Unlock()
			}(z)
		}
	}
	wg.Wait()
	close(errCh)

	for err := range errCh {
		t.Fatalf("cross-zone uniqueness violated: %v", err)
	}
	if len(allIDs) != len(zones)*perZone {
		t.Fatalf("expected %d unique IDs, got %d", len(zones)*perZone, len(allIDs))
	}
	if allIDs[0] {
		t.Fatalf("0 was allocated")
	}
}

func TestAllocDifferentTypeReuse(t *testing.T) {
	cli := newTestClient(t)
	defer cli.Close()
	ntA := uniqueNodeType()
	ntB := uniqueNodeType()
	defer cleanupNodeType(t, cli, ntA)
	defer cleanupNodeType(t, cli, ntB)

	leaseA := grantLease(t, cli, 30)
	leaseB := grantLease(t, cli, 30)
	defer cli.Revoke(context.Background(), leaseA)
	defer cli.Revoke(context.Background(), leaseB)

	idA, err := allocateNodeID(context.Background(), cli, rpcPrefix(ntA), makeInfo(ntA, 1), leaseA)
	if err != nil {
		t.Fatalf("alloc A: %v", err)
	}
	idB, err := allocateNodeID(context.Background(), cli, rpcPrefix(ntB), makeInfo(ntB, 1), leaseB)
	if err != nil {
		t.Fatalf("alloc B: %v", err)
	}
	if idA != 1 || idB != 1 {
		t.Fatalf("expected both first allocations =1, got A=%d B=%d", idA, idB)
	}
}

func TestAllocReclaimAfterRevoke(t *testing.T) {
	cli := newTestClient(t)
	defer cli.Close()
	nt := uniqueNodeType()
	defer cleanupNodeType(t, cli, nt)

	prefix := rpcPrefix(nt)
	lease1 := grantLease(t, cli, 30)
	id1, err := allocateNodeID(context.Background(), cli, prefix, makeInfo(nt, 1), lease1)
	if err != nil {
		t.Fatalf("alloc 1: %v", err)
	}
	if _, err := cli.Revoke(context.Background(), lease1); err != nil {
		t.Fatalf("revoke: %v", err)
	}
	time.Sleep(100 * time.Millisecond)

	lease2 := grantLease(t, cli, 30)
	defer cli.Revoke(context.Background(), lease2)
	id2, err := allocateNodeID(context.Background(), cli, prefix, makeInfo(nt, 1), lease2)
	if err != nil {
		t.Fatalf("alloc 2: %v", err)
	}
	if id2 != id1 {
		t.Fatalf("expected reclaim id=%d, got id=%d", id1, id2)
	}
}

func TestAllocCASRetryOnConflict(t *testing.T) {
	cli := newTestClient(t)
	defer cli.Close()
	nt := uniqueNodeType()
	defer cleanupNodeType(t, cli, nt)

	prefix := rpcPrefix(nt)
	intruderLease := grantLease(t, cli, 30)
	defer cli.Revoke(context.Background(), intruderLease)

	allocKey := allocationKey(prefix, nt, 1)
	if _, err := cli.Put(context.Background(), allocKey, "intruder-uuid", clientv3.WithLease(intruderLease)); err != nil {
		t.Fatalf("seed intruder: %v", err)
	}

	myLease := grantLease(t, cli, 30)
	defer cli.Revoke(context.Background(), myLease)

	info := makeInfo(nt, 1)
	ok, err := tryClaimNodeID(context.Background(), cli, prefix, 1, info, myLease)
	if err != nil {
		t.Fatalf("tryClaim id=1: %v", err)
	}
	if ok {
		t.Fatalf("tryClaim id=1 should have failed (intruder holds it)")
	}

	id, err := allocateNodeID(context.Background(), cli, prefix, makeInfo(nt, 1), myLease)
	if err != nil {
		t.Fatalf("allocateNodeID: %v", err)
	}
	if id != 2 {
		t.Fatalf("expected id=2 (next free), got %d", id)
	}
}

// TestReRegisterRefusesOverwrite 是 scene_manager 独有的关键测试:
//
// 场景:实例 A 持有 (nodeType, node_id=1, zone=1)。
//      A 的 lease 因为网络故障失效,etcd 自动清掉 A 的两个 key。
//      期间实例 B(zone=2,新 NodeUuid)启动,allocator 看到 id=1 空闲,占住它。
//      此后 A 网络恢复,触发 reRegister。
//
// 期望:
//   - reRegister 必须发现 allocationKey 已被别的 uuid 占用(allocationKey.Version != 0)
//   - 不能盲目 Put 老 key 覆盖 B 的 NodeInfo
//   - 应当走 "allocate 新 node_id" 分支,A 拿到新 id(比如 2),不影响 B
//
// 这条断言是把 scene_manager 历史 bug —— 直接 Put 覆盖 —— 钉死。
func TestReRegisterRefusesOverwrite(t *testing.T) {
	cli := newTestClient(t)
	defer cli.Close()
	nt := uniqueNodeType()
	defer cleanupNodeType(t, cli, nt)

	prefix := rpcPrefix(nt)

	// 1) A 注册并占住 id=1
	leaseA := grantLease(t, cli, 30)
	infoA := makeInfo(nt, 1)
	idA, err := allocateNodeID(context.Background(), cli, prefix, infoA, leaseA)
	if err != nil {
		t.Fatalf("A alloc: %v", err)
	}
	if idA != 1 {
		t.Fatalf("A should get id=1, got %d", idA)
	}

	// 构造 NodeRegistration 模拟 A 的注册状态
	nrA := &NodeRegistration{
		client:   cli,
		leaseID:  leaseA,
		ttl:      30,
		prefix:   prefix,
		rpcKey:   rpcPath(prefix, infoA.ZoneId, infoA.NodeType, idA),
		allocKey: allocationKey(prefix, infoA.NodeType, idA),
		Info:     infoA,
	}
	// nrA.value 不影响测试逻辑(reRegister 不读它的旧 value)
	nrA.value = "{}"

	// 2) 模拟 A lease 丢失 —— revoke 即可
	if _, err := cli.Revoke(context.Background(), leaseA); err != nil {
		t.Fatalf("revoke A: %v", err)
	}
	time.Sleep(100 * time.Millisecond)

	// 3) B 启动,以全新 uuid 占住 id=1
	leaseB := grantLease(t, cli, 30)
	defer cli.Revoke(context.Background(), leaseB)
	infoB := makeInfo(nt, 2) // 不同 zone,但新 schema 下 zone 不影响 allocation key
	idB, err := allocateNodeID(context.Background(), cli, prefix, infoB, leaseB)
	if err != nil {
		t.Fatalf("B alloc: %v", err)
	}
	if idB != 1 {
		t.Fatalf("B should also get id=1 after A's lease expired, got %d", idB)
	}

	// 4) A 触发 reRegister。它应当发现 id=1 已经被 B(NodeUuid != A.NodeUuid)占用,
	//    走 "allocate 新 node_id" 分支,而不是覆盖 B。
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	go func() {
		// reRegister 内部带无限退避循环,需要靠 ctx 退出
		nrA.reRegister(ctx)
	}()

	// 等 A 完成 reRegister。轮询直到 A 拿到一个新的、合法的 node_id。
	deadline := time.Now().Add(3 * time.Second)
	var aNewID uint32
	for time.Now().Before(deadline) {
		if nrA.Info.NodeId != idA {
			aNewID = nrA.Info.NodeId
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	cancel() // 让 reRegister goroutine 退出

	if aNewID == 0 {
		t.Fatalf("A reRegister did not pick a new node_id (still=%d)", nrA.Info.NodeId)
	}
	if aNewID == idB {
		t.Fatalf("A reRegister picked the same id=%d as B — collision!", aNewID)
	}

	// 5) 关键断言:B 的 NodeInfo 没有被 A 覆盖。
	bRpcKey := rpcPath(prefix, infoB.ZoneId, infoB.NodeType, idB)
	resp, err := cli.Get(context.Background(), bRpcKey)
	if err != nil {
		t.Fatalf("get B rpcKey: %v", err)
	}
	if len(resp.Kvs) == 0 {
		t.Fatalf("B's rpcKey was deleted — A overwrote it!")
	}

	// allocationKey 的 value 应当仍是 B 的 NodeUuid
	bAllocKey := allocationKey(prefix, infoB.NodeType, idB)
	resp, err = cli.Get(context.Background(), bAllocKey)
	if err != nil {
		t.Fatalf("get B allocKey: %v", err)
	}
	if len(resp.Kvs) == 0 {
		t.Fatalf("B's allocKey was deleted — A overwrote it!")
	}
	if string(resp.Kvs[0].Value) != infoB.NodeUuid {
		t.Fatalf("B's allocKey value = %q, expected B uuid %q (likely overwritten)",
			string(resp.Kvs[0].Value), infoB.NodeUuid)
	}
}
