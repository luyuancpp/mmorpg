// retry_requeue 把 go/db 按原始分区拆开的重试 ready(kafka:retry:queue:{topic}:p*)搬回共享收件箱
// kafka:retry:queue:{topic},是存盘管线 S1 ④「重试只交给分区属主」的回滚工具(docs/design/db-retry-owner-routing.md §10)。
//
// 为什么需要它:RetryRouting=owner 起,重试收据按载荷里的原始分区分拣进 p{N};S1 之前的二进制只认共享键、看不到 p*,
// 直接回滚镜像会让 p* 里的存盘滞留不落库。正常回滚先切 shared(shared 实例每拍兜底排空 p*),用 -check 确认 p* 全 0
// 再回滚;来不及切 shared 的紧急回滚,回滚之后立刻用本工具搬回。
//
// 用法(工作目录 go/db;口令只经环境变量传入,不上命令行、不打印):
//
//	go run ./cmd/retry_requeue -f etc/db.yaml -check                       # 只读盘点
//	go run ./cmd/retry_requeue -f etc/db.yaml                              # 搬回
//	go run ./cmd/retry_requeue -f etc/db.yaml -zone 2 -redis 127.0.0.1:16379 -redis-password-env REDIS_PASSWORD
//
// 约定:
//   - -check 只读(LLEN / ZRANGE / SCAN),不改任何键,打印盘点后按结果退出:p* 全 0 以 0 退出,p* 非 0 以 3 退出
//     (回滚规程或包装脚本可直接按退出码拦截「p* 未清空就回滚镜像」);
//   - 不带 -check 即执行搬回(原规格的约定,不另设 dry-run 开关;想先看就先跑 -check)。动手之前先做一次只读盘点,
//     参数、配置、Redis 连通性、盘点任一失败都直接非零退出、一条不动(fail-closed);
//   - 只搬 v2 载荷(带原始分区),其余进死信;某个 p 键类型不对时脚本整体拒绝、一条不动;
//   - SCAN 用的模式先对 topic 做 glob 转义(kafka.RetryQueueDepths / RequeuePartitionedRetries 内部完成),
//     也处理 p0..p{N-1} 之外的越界 / 多余 p 键;
//   - 可重入:中途失败或被 Ctrl+C 打断,修好原因后重跑即可,已搬的不会重复;
//   - 搬完再盘点一次,p* 仍不为 0(多半是还有 RetryRouting=owner 的实例在把收件箱分拣回 p*)时以 3 退出;
//   - 前置条件:该 zone 的每个 go/db 都已是 RetryRouting=shared 或 S1 之前的镜像。本工具不连 Kafka、也无从判断
//     各实例的模式,只在开头打印这条前置条件,由执行者负责。
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"os/signal"
	"sort"
	"syscall"

	db_config "db/internal/config"
	"db/internal/kafka"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
)

// 退出码:0 = 成功(-check 时还要求 p* 全 0);exitError = 参数 / 配置 / Redis / 盘点 / 搬回出错;
// exitResidual = -check 盘点到 p* 不为 0,或搬回完成但 p* 仍不为 0。
const (
	exitError    = 1
	exitResidual = 3
)

const preconditionLine = "precondition: run only after every go/db of this zone runs RetryRouting=shared or the pre-owner image; owner instances would forward the items back"

// options 是命令行参数(flag 解析结果)。
type options struct {
	configFile  string
	zone        uint64 // 0 = 配置文件里的 ZoneId
	redisAddr   string // "" = ServerConfig.RedisClient.Hosts
	redisDB     int    // -1 = ServerConfig.RedisClient.DB
	passwordEnv string // "" = ServerConfig.RedisClient.Password
	check       bool
	batch       int
}

// target 是本次要盘点 / 搬回的 topic 与它所在的 Redis。password 只用于建连接,从不打印。
type target struct {
	topic      string
	partitions int32
	addr       string
	db         int
	password   string
}

func main() {
	var opt options
	flag.StringVar(&opt.configFile, "f", "etc/db.yaml", "go/db config file; the topic, ServerConfig.Kafka.PartitionCnt and the Redis client are derived from it")
	flag.Uint64Var(&opt.zone, "zone", 0, "zone id override (0 = ZoneId of the config file)")
	flag.StringVar(&opt.redisAddr, "redis", "", "redis host:port override (empty = ServerConfig.RedisClient.Hosts)")
	flag.IntVar(&opt.redisDB, "redis-db", -1, "redis DB override (-1 = ServerConfig.RedisClient.DB)")
	flag.StringVar(&opt.passwordEnv, "redis-password-env", "", "name of the environment variable that holds the redis password (empty = ServerConfig.RedisClient.Password); the password is never read from the command line and never printed")
	flag.BoolVar(&opt.check, "check", false, "read-only inventory of the retry lists; moves nothing; exits 3 when any partitioned ready list is not empty")
	flag.IntVar(&opt.batch, "batch", kafka.RequeueDefaultBatch, fmt.Sprintf("payloads moved per script call, in [1, %d]", kafka.RequeueMaxBatch))
	flag.Parse()

	// Ctrl+C / SIGTERM 取消 ctx:搬回是可重入的,半途停下不丢收据,重跑即可。
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	code := run(ctx, opt, os.Stdout, os.Stderr)
	stop()
	os.Exit(code)
}

// run 执行一次盘点(-check)或搬回,返回退出码。
func run(ctx context.Context, opt options, out, errOut io.Writer) int {
	fail := func(err error) int {
		fmt.Fprintf(errOut, "retry_requeue: %v\n", err)
		return exitError
	}

	fmt.Fprintln(out, preconditionLine)
	if err := kafka.ValidateRequeueBatch(opt.batch); err != nil {
		return fail(err)
	}

	var cfg db_config.Config
	if err := conf.Load(opt.configFile, &cfg); err != nil {
		return fail(fmt.Errorf("load %s: %w", opt.configFile, err))
	}
	cfg.Normalize()
	t, err := resolveTarget(cfg, opt, os.LookupEnv)
	if err != nil {
		return fail(err)
	}

	rc := redis.NewClient(&redis.Options{
		Addr:             t.addr,
		Password:         t.password,
		DB:               t.db,
		DisableIndentity: true, // suppress CLIENT SETINFO on Redis < 7.2(同 svc.NewServiceContext)
	})
	defer func() { _ = rc.Close() }()
	if err := rc.Ping(ctx).Err(); err != nil {
		return fail(fmt.Errorf("redis ping %s db=%d (nothing moved): %w", t.addr, t.db, err))
	}

	mode := "requeue"
	if opt.check {
		mode = "check (read-only)"
	}
	fmt.Fprintf(out, "target: topic=%s partitions=%d redis=%s db=%d mode=%s batch=%d\n",
		t.topic, t.partitions, t.addr, t.db, mode, opt.batch)

	before, err := kafka.RetryQueueDepths(ctx, rc, t.topic, t.partitions)
	if err != nil {
		return fail(fmt.Errorf("inventory failed (nothing moved): %w", err))
	}
	printDepths(out, "before", before)
	if opt.check {
		// 仍然只读,只把盘点结果写进退出码:回滚前置门禁(p* 全 0 才能回滚到 S1 之前的镜像)可以由脚本按退出码 fail-closed,
		// 不必靠人读输出。口径与搬回之后的残留一致(exitResidual)。
		if left := before.PartitionedTotal(); left > 0 {
			fmt.Fprintf(errOut, "retry_requeue: %d payloads in partitioned ready lists of %s; do not roll back to a pre-S1 image yet\n",
				left, t.topic)
			return exitResidual
		}
		return 0
	}
	if before.PartitionedTotal() == 0 {
		fmt.Fprintln(out, "nothing to requeue: every partitioned ready list is empty")
		return 0
	}

	report, requeueErr := kafka.RequeuePartitionedRetries(ctx, rc, t.topic, t.partitions, opt.batch)
	printRequeue(out, t.topic, report)
	if requeueErr != nil {
		return fail(fmt.Errorf("requeue stopped; the counts above were already moved, the tool is re-entrant, rerun after fixing the cause: %w", requeueErr))
	}

	after, err := kafka.RetryQueueDepths(ctx, rc, t.topic, t.partitions)
	if err != nil {
		return fail(fmt.Errorf("inventory after requeue failed (rerun with -check): %w", err))
	}
	printDepths(out, "after", after)
	if left := after.PartitionedTotal(); left > 0 {
		fmt.Fprintf(errOut, "retry_requeue: %d payloads are still in partitioned ready lists of %s; an instance running RetryRouting=owner is probably forwarding the shared inbox back. Switch every go/db of this zone to shared (or the pre-owner image) and rerun.\n",
			left, t.topic)
		return exitResidual
	}
	fmt.Fprintln(out, "done: every partitioned ready list is empty")
	return 0
}

// resolveTarget 由配置与命令行覆盖算出 topic、分区数与 Redis 连接参数。任何一项缺失或非法都返回 error(调用方不动手)。
// topic 的推导与 go/db 启动(db.go)相同:DbTaskTopicForGeneration(ZoneId, TopicGeneration)。
func resolveTarget(cfg db_config.Config, opt options, lookupEnv func(string) (string, bool)) (target, error) {
	zone := cfg.ZoneId
	if opt.zone != 0 {
		if opt.zone > math.MaxUint32 {
			return target{}, fmt.Errorf("-zone %d is out of range", opt.zone)
		}
		zone = uint32(opt.zone)
	}
	if zone == 0 {
		return target{}, errors.New("ZoneId must be > 0 (set it in the config file or pass -zone)")
	}
	partitions := cfg.ServerConfig.Kafka.PartitionCnt
	if partitions <= 0 {
		return target{}, fmt.Errorf("ServerConfig.Kafka.PartitionCnt must be > 0, got %d", partitions)
	}

	r := cfg.ServerConfig.RedisClient
	t := target{
		topic:      db_config.DbTaskTopicForGeneration(zone, cfg.ServerConfig.Kafka.TopicGeneration),
		partitions: partitions,
		addr:       r.Hosts,
		db:         r.DB,
		password:   r.Password,
	}
	if opt.redisAddr != "" {
		t.addr = opt.redisAddr
	}
	if t.addr == "" {
		return target{}, errors.New("redis address is empty (set ServerConfig.RedisClient.Hosts or pass -redis)")
	}
	switch {
	case opt.redisDB >= 0:
		t.db = opt.redisDB
	case opt.redisDB != -1:
		return target{}, fmt.Errorf("-redis-db must be >= 0 (or -1 for the config value), got %d", opt.redisDB)
	}
	if opt.passwordEnv != "" {
		v, ok := lookupEnv(opt.passwordEnv)
		if !ok {
			// 指定了却没设:多半是 shell 里漏了 export,用配置里的口令去试只会得到一个误导性的认证错误。
			return target{}, fmt.Errorf("environment variable %s named by -redis-password-env is not set", opt.passwordEnv)
		}
		t.password = v
	}
	return t, nil
}

// printDepths 打印一次盘点:各列表长度,p 键逐个列出(p0..p{N-1} 全列,外加越界 / 多余的键)。
func printDepths(out io.Writer, label string, r kafka.RetryQueueDepthReport) {
	fmt.Fprintf(out, "[%s] shared_inbox=%d partitioned_total=%d dead=%d legacy_processing=%d instances=%d\n",
		label, r.Shared, r.PartitionedTotal(), r.Dead, r.LegacyProcessing, len(r.Instances))
	for _, key := range sortedKeys(r.Partitioned) {
		fmt.Fprintf(out, "  partitioned %s = %d\n", key, r.Partitioned[key])
	}
	for _, id := range sortedKeys(r.Instances) {
		fmt.Fprintf(out, "  instance %s processing = %d\n", id, r.Instances[id])
	}
}

// printRequeue 打印一次搬回的结果(出错时也打印,让执行者知道哪些已经搬过)。键名不在这里重拼(只在 kafka 包里定义一次),
// 去向用盘点里的 shared_inbox / dead 两列核对。
func printRequeue(out io.Writer, topic string, r kafka.RequeueReport) {
	partitions := make([]int32, 0, len(r.Moved))
	var moved int64
	for p, n := range r.Moved {
		partitions = append(partitions, p)
		moved += n
	}
	sort.Slice(partitions, func(i, j int) bool { return partitions[i] < partitions[j] })
	for _, p := range partitions {
		fmt.Fprintf(out, "  requeued partition=%d moved=%d\n", p, r.Moved[p])
	}
	for _, key := range sortedKeys(r.Stray) {
		fmt.Fprintf(out, "  requeued stray key %s moved=%d\n", key, r.Stray[key])
		moved += r.Stray[key]
	}
	fmt.Fprintf(out, "requeue: topic=%s moved_to_shared_inbox=%d dead_lettered_non_v2=%d\n", topic, moved, r.Dead)
}

// sortedKeys 按「先长度、再字典序」排:同一前缀下 p2 排在 p10 之前。
func sortedKeys(m map[string]int64) []string {
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Slice(keys, func(i, j int) bool {
		if len(keys[i]) != len(keys[j]) {
			return len(keys[i]) < len(keys[j])
		}
		return keys[i] < keys[j]
	})
	return keys
}
