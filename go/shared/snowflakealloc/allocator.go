// Package snowflakealloc 提供基于 etcd 的 Snowflake worker id 分配器。
//
// 与服务发现层的 NodeInfo.NodeId 分配解耦:
//   - NodeInfo.NodeId 由 node allocator(go/<service>/internal/node) 管理,
//     用于 C++ 端服务发现路由。
//   - Snowflake worker id 由本包管理,用于 ID 生成器的种子。
//
// 解耦的好处:即使 NodeInfo 在边缘场景(reRegister CAS 失败、etcd 短暂分裂)
// 出问题,Snowflake ID 也不受影响 —— 因为 worker id 由独立 lease 锚定到 hostname。
//
// hostname 亲和性:
//   - 同 hostname 重启 → 复用同一个 worker id(避免 worker id 变化导致的
//     Snowflake 时间戳回退冲突)
//   - 不同 hostname → 双 key CAS 拿一个空闲 worker id
//
// 用法:
//
//	leaseID, workerID, err := snowflakealloc.AllocateWithKeepAlive(
//	    ctx, cli, "/guild", os.Hostname(), 60)
//	if err != nil { panic(err) }
//	sf := snowflake.NewNode(workerID)
//
// 关键:**prefix 必须按服务区分**,否则两个服务的 worker id 池会互相干扰。
// 推荐 prefix:"/guild" / "/scene_manager" / "/<service_name>"。
package snowflakealloc

import (
	"context"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
	clientv3 "go.etcd.io/etcd/client/v3"

	"shared/snowflake"
)

// Options 控制分配器行为。
type Options struct {
	// LeaseTTL 是 etcd lease 的 TTL(秒)。0 时默认 60。
	LeaseTTL int64
	// MaxWorkerID 是 worker id 上限(含)。0 时默认取 shared/snowflake.NodeMask。
	// 调用方可以传更小的值,如果 Snowflake 实现位宽更窄(例如 bwmarrin 是 10 bit)。
	MaxWorkerID uint64
}

func (o Options) ttl() int64 {
	if o.LeaseTTL <= 0 {
		return 60
	}
	return o.LeaseTTL
}

func (o Options) maxWorkerID() uint64 {
	if o.MaxWorkerID == 0 {
		return uint64(snowflake.NodeMask)
	}
	return o.MaxWorkerID
}

// nodeKey 是 hostname → workerID 映射,用来支持 "同 hostname 重启复用同 id"。
//   <prefix>/snowflake_nodes/<hostname> = <id>
func nodeKey(prefix, hostname string) string {
	return fmt.Sprintf("%s/snowflake_nodes/%s", prefix, hostname)
}

// idKey 是 workerID 占位 key,反向映射回 hostname,用于扫描已占 ID。
//   <prefix>/snowflake_ids/<id> = <hostname>
func idKey(prefix string, id uint64) string {
	return fmt.Sprintf("%s/snowflake_ids/%d", prefix, id)
}

func nodeKeyPrefix(prefix string) string {
	return prefix + "/snowflake_nodes/"
}

func idKeyPrefix(prefix string) string {
	return prefix + "/snowflake_ids/"
}

// Allocate 为 (prefix, hostname) 分配一个 Snowflake worker id。
//
// 行为:
//  1. 拿一个新 lease,TTL 由 opts 决定。
//  2. 如果 nodeKey(hostname) 还在(进程刚重启,etcd 老 lease 尚未过期或上次进程没等到 TTL):
//     CAS 重新认领:If Value(nodeKey)==<oldID> Then Put nodeKey+idKey with new lease。
//     成功就复用 oldID。
//  3. 否则扫 idKeyPrefix + nodeKeyPrefix(后者是兼容 lease 过期前的并发场景)合并 used 集合,
//     找最小空闲 worker id,**从 0 开始**(Snowflake worker_id=0 是合法的,无 "未分配" 哨兵语义,
//     这点和 NodeInfo.NodeId 不同)。
//  4. CAS If CreateRev(nodeKey)==0 AND CreateRev(idKey)==0 Then Put 双 key。
//     成功返回 (leaseID, workerID, nil)。失败重新扫描重试(意味着别人并发抢到了)。
//
// 调用方负责处理 lease 的 KeepAlive,或使用 AllocateWithKeepAlive。
//
// 错误条件:worker id 池耗尽(used 集合 size > maxWorkerID),返回错误。
func Allocate(ctx context.Context, cli *clientv3.Client, prefix, hostname string, opts Options) (clientv3.LeaseID, uint64, error) {
	if prefix == "" {
		return 0, 0, fmt.Errorf("snowflakealloc: empty prefix")
	}
	if hostname == "" {
		return 0, 0, fmt.Errorf("snowflakealloc: empty hostname")
	}

	leaseResp, err := cli.Grant(ctx, opts.ttl())
	if err != nil {
		return 0, 0, fmt.Errorf("snowflakealloc: grant lease: %w", err)
	}
	leaseID := leaseResp.ID

	nKey := nodeKey(prefix, hostname)
	maxID := opts.maxWorkerID()

	for {
		select {
		case <-ctx.Done():
			_, _ = cli.Revoke(context.Background(), leaseID)
			return 0, 0, ctx.Err()
		default:
		}

		// 1) 尝试复用 hostname 已有的 workerID。
		// 注意:此处用 Value 比较而不是 CreateRevision == 0 —— 我们希望
		// "如果 hostname 的 key 还在,且 value 是某个数字 X,就把同一个 X 用新 lease 抢回来"。
		resp, err := cli.Get(ctx, nKey)
		if err != nil {
			_, _ = cli.Revoke(context.Background(), leaseID)
			return 0, 0, fmt.Errorf("snowflakealloc: get nodeKey: %w", err)
		}
		if len(resp.Kvs) > 0 {
			oldIDStr := string(resp.Kvs[0].Value)
			if oldID, perr := strconv.ParseUint(oldIDStr, 10, 64); perr == nil && oldID <= maxID {
				iKey := idKey(prefix, oldID)
				// CAS: Value(nKey)==oldIDStr 才接管。这样防止 hostname key 过期后
				// 别人已经抢过 oldID 又写回别的 value 的极端竞态。
				txnResp, err := cli.Txn(ctx).
					If(clientv3.Compare(clientv3.Value(nKey), "=", oldIDStr)).
					Then(
						clientv3.OpPut(nKey, oldIDStr, clientv3.WithLease(leaseID)),
						clientv3.OpPut(iKey, hostname, clientv3.WithLease(leaseID)),
					).
					Commit()
				if err == nil && txnResp.Succeeded {
					logx.Infof("[snowflakealloc] reused worker_id=%d (prefix=%s, host=%s)", oldID, prefix, hostname)
					return leaseID, oldID, nil
				}
				// CAS 失败 → key 的 value 已变(典型:lease 过期重写),继续走分配新 ID 流程
			}
		}

		// 2) 扫 used 集合,找最小空闲 id。
		used, err := scanUsedWorkerIDs(ctx, cli, prefix)
		if err != nil {
			_, _ = cli.Revoke(context.Background(), leaseID)
			return 0, 0, err
		}

		var targetID uint64 = maxID + 1
		for i := uint64(0); i <= maxID; i++ {
			if !used[i] {
				targetID = i
				break
			}
		}
		if targetID > maxID {
			_, _ = cli.Revoke(context.Background(), leaseID)
			return 0, 0, fmt.Errorf("snowflakealloc: worker id pool exhausted (max %d, prefix=%s)", maxID, prefix)
		}

		// 3) 双 key CAS 抢占。两个 key 都必须不存在才能拿下,任何一个有人就放弃。
		idStr := strconv.FormatUint(targetID, 10)
		iKey := idKey(prefix, targetID)
		txnResp, err := cli.Txn(ctx).
			If(
				clientv3.Compare(clientv3.CreateRevision(nKey), "=", 0),
				clientv3.Compare(clientv3.CreateRevision(iKey), "=", 0),
			).
			Then(
				clientv3.OpPut(nKey, idStr, clientv3.WithLease(leaseID)),
				clientv3.OpPut(iKey, hostname, clientv3.WithLease(leaseID)),
			).
			Commit()
		if err != nil {
			_, _ = cli.Revoke(context.Background(), leaseID)
			return 0, 0, fmt.Errorf("snowflakealloc: claim txn: %w", err)
		}
		if txnResp.Succeeded {
			logx.Infof("[snowflakealloc] allocated worker_id=%d (prefix=%s, host=%s)", targetID, prefix, hostname)
			return leaseID, targetID, nil
		}
		// CAS 失败 → 并发抢占,重试整个循环(重新扫 used)
	}
}

// scanUsedWorkerIDs 合并扫 idKey 前缀 + nodeKey 前缀,
// 任一前缀显示的 id 都算占用。
func scanUsedWorkerIDs(ctx context.Context, cli *clientv3.Client, prefix string) (map[uint64]bool, error) {
	used := make(map[uint64]bool)

	// idKey 前缀:value 是 hostname,key 尾部数字是 id
	idResp, err := cli.Get(ctx, idKeyPrefix(prefix), clientv3.WithPrefix())
	if err != nil {
		return nil, fmt.Errorf("snowflakealloc: scan id prefix: %w", err)
	}
	idP := idKeyPrefix(prefix)
	for _, kv := range idResp.Kvs {
		tail := strings.TrimPrefix(string(kv.Key), idP)
		if id, err := strconv.ParseUint(tail, 10, 64); err == nil {
			used[id] = true
		}
	}

	// nodeKey 前缀:value 是 id 字符串
	// 这个扫描是冗余的(idKey 已经覆盖),但保留以防 idKey 因运维误操作丢失却 nodeKey 还在的不对称状态。
	nodeResp, err := cli.Get(ctx, nodeKeyPrefix(prefix), clientv3.WithPrefix())
	if err != nil {
		return nil, fmt.Errorf("snowflakealloc: scan node prefix: %w", err)
	}
	for _, kv := range nodeResp.Kvs {
		if id, err := strconv.ParseUint(string(kv.Value), 10, 64); err == nil {
			used[id] = true
		}
	}

	return used, nil
}

// Handle 把 lease + worker id + keepalive 取消函数打包,方便调用方在进程退出时清理。
type Handle struct {
	LeaseID  clientv3.LeaseID
	WorkerID uint64
	cancel   context.CancelFunc
	cli      *clientv3.Client
}

// AllocateWithKeepAlive 调用 Allocate 并在后台启动 KeepAlive。
// 返回的 Handle.Close() 会取消 KeepAlive 并 Revoke lease。
//
// keepalive goroutine 必须 drain 响应 channel —— 不读会导致 etcd client 的
// 16-slot buffer 填满后频繁打 "lease keepalive response queue is full" 日志。
// 这里用 for-range 静默吃掉响应,lease 本身由 etcd client 内部维护。
func AllocateWithKeepAlive(ctx context.Context, cli *clientv3.Client, prefix, hostname string, opts Options) (*Handle, error) {
	leaseID, workerID, err := Allocate(ctx, cli, prefix, hostname, opts)
	if err != nil {
		return nil, err
	}

	kaCtx, cancel := context.WithCancel(context.Background())
	ch, err := cli.KeepAlive(kaCtx, leaseID)
	if err != nil {
		cancel()
		_, _ = cli.Revoke(context.Background(), leaseID)
		return nil, fmt.Errorf("snowflakealloc: keepalive: %w", err)
	}
	go func() {
		for range ch {
			// drain
		}
		logx.Errorf("[snowflakealloc] keepalive channel closed (prefix=%s, host=%s, worker_id=%d)",
			prefix, hostname, workerID)
	}()

	return &Handle{
		LeaseID:  leaseID,
		WorkerID: workerID,
		cancel:   cancel,
		cli:      cli,
	}, nil
}

// Close 停止 KeepAlive 并 Revoke lease。
// 调用后 etcd 会立即清掉 nodeKey + idKey,worker id 可被立即复用。
// 多次调用安全。
func (h *Handle) Close() {
	if h == nil {
		return
	}
	if h.cancel != nil {
		h.cancel()
		h.cancel = nil
	}
	if h.cli != nil && h.LeaseID != 0 {
		ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
		defer cancel()
		_, _ = h.cli.Revoke(ctx, h.LeaseID)
		h.LeaseID = 0
	}
}
