package main

// retry_requeue 的回滚门禁(R7 复核)单测:-zone 必填、-topic-generation 覆盖、目标看起来没在用(退出码 4)、
// 登记表里有 owner 实例(退出码 5)。全部跑在 miniredis 上,不读配置文件(runWithConfig 直接收配置)。

import (
	"bytes"
	"context"
	"testing"

	db_config "db/internal/config"
	"db/internal/kafka"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

const testZone = 101

// testTopic 是 testConfig 推导出的 topic(TopicGeneration=1 即不带代次后缀)。
var testTopic = db_config.DbTaskTopicForGeneration(testZone, 1)

// 键名在 kafka 包里定义一次;这里按字面量拼,同时钉住工具读的是这几个键。
func instancesKey() string { return "kafka:retry:instances:" + testTopic }
func modesKey() string     { return "kafka:retry:instance_mode:" + testTopic }
func pKey(p string) string { return "kafka:retry:queue:" + testTopic + ":p" + p }
func inboxKey() string     { return "kafka:retry:queue:" + testTopic }

// testConfig 是一份最小配置:配置文件里的 ZoneId 故意写成 1,用来断言它不被当作缺省。
func testConfig(addr string) db_config.Config {
	var cfg db_config.Config
	cfg.ZoneId = 1
	cfg.ServerConfig.Kafka.PartitionCnt = 2
	cfg.ServerConfig.Kafka.TopicGeneration = 1
	cfg.ServerConfig.RedisClient.Hosts = addr
	return cfg
}

func noEnv(string) (string, bool) { return "", false }

// v2Payload 是一条带原始分区的 v2 重试载荷(搬回脚本只看第 1 字节与长度)。
func v2Payload() string {
	return string([]byte{2, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 'x'})
}

type toolRun struct {
	code   int
	stdout string
	stderr string
}

func runTool(t *testing.T, mr *miniredis.Miniredis, opt options) toolRun {
	t.Helper()
	if opt.batch == 0 {
		opt.batch = kafka.RequeueDefaultBatch
	}
	if opt.zone == 0 {
		opt.zone = testZone
	}
	var out, errOut bytes.Buffer
	code := runWithConfig(context.Background(), testConfig(mr.Addr()), opt, noEnv, &out, &errOut)
	return toolRun{code: code, stdout: out.String(), stderr: errOut.String()}
}

func newToolRedis(t *testing.T) (*miniredis.Miniredis, *redis.Client) {
	t.Helper()
	mr := miniredis.RunT(t)
	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rc.Close() })
	return mr, rc
}

// register 在登记表里登记一个实例;mode 为空时不写模式 HASH(模拟本版本之前的镜像)。
func register(t *testing.T, rc *redis.Client, id, mode string) {
	t.Helper()
	ctx := context.Background()
	require.NoError(t, rc.ZAdd(ctx, instancesKey(), redis.Z{Score: 1, Member: id}).Err())
	if mode != "" {
		require.NoError(t, rc.HSet(ctx, modesKey(), id, mode).Err())
	}
}

// R7 [8]:-zone 必填,配置文件里的 ZoneId 不作缺省。
func TestResolveTarget_RequiresZone(t *testing.T) {
	cfg := testConfig("127.0.0.1:6379")
	_, err := resolveTarget(cfg, options{redisDB: -1, topicGeneration: -1}, noEnv)
	require.Error(t, err)
	assert.Contains(t, err.Error(), "-zone is required")

	tg, err := resolveTarget(cfg, options{zone: testZone, redisDB: -1, topicGeneration: -1}, noEnv)
	require.NoError(t, err)
	assert.Equal(t, testTopic, tg.topic, "the topic follows -zone, not the ZoneId of the config file")
}

// R7 [5]:-topic-generation 覆盖配置文件里的代次;-1 取配置值;其余负数拒绝。
func TestResolveTarget_TopicGenerationOverride(t *testing.T) {
	cfg := testConfig("127.0.0.1:6379")
	tg, err := resolveTarget(cfg, options{zone: testZone, redisDB: -1, topicGeneration: 2}, noEnv)
	require.NoError(t, err)
	assert.Equal(t, db_config.DbTaskTopicForGeneration(testZone, 2), tg.topic)

	tg, err = resolveTarget(cfg, options{zone: testZone, redisDB: -1, topicGeneration: -1}, noEnv)
	require.NoError(t, err)
	assert.Equal(t, testTopic, tg.topic)

	_, err = resolveTarget(cfg, options{zone: testZone, redisDB: -1, topicGeneration: -2}, noEnv)
	assert.Error(t, err)
}

// R7 [5]:登记表为空、所有列表都是 0(指错了 topic / Redis / DB)时 -check 与搬回都以 4 退出;
// 显式 -allow-empty-target 才放行。
func TestRun_TargetLooksUnusedFailsClosed(t *testing.T) {
	mr, rc := newToolRedis(t)

	check := runTool(t, mr, options{check: true, redisDB: -1, topicGeneration: -1})
	assert.Equal(t, exitTargetUnused, check.code, check.stderr)
	assert.Contains(t, check.stderr, "target looks unused")

	requeue := runTool(t, mr, options{redisDB: -1, topicGeneration: -1})
	assert.Equal(t, exitTargetUnused, requeue.code, requeue.stderr)

	allowed := runTool(t, mr, options{check: true, allowEmptyTarget: true, redisDB: -1, topicGeneration: -1})
	assert.Equal(t, 0, allowed.code, allowed.stderr)

	// 有一个登记着的 shared 实例:目标在用,p* 全 0 → 0。
	register(t, rc, "db-a:6000", "shared")
	inUse := runTool(t, mr, options{check: true, redisDB: -1, topicGeneration: -1})
	assert.Equal(t, 0, inUse.code, inUse.stderr)
	assert.Contains(t, inUse.stdout, "instance db-a:6000 processing = 0 mode = shared")
}

// R7 [7]:登记表里有登记为 owner 的实例时 -check 以 5 退出(即使 p* 全 0),搬回也一条不动。
func TestRun_OwnerInstanceBlocksCheckAndRequeue(t *testing.T) {
	mr, rc := newToolRedis(t)
	register(t, rc, "db-a:6000", "shared")
	register(t, rc, "db-b:6000", "owner")

	check := runTool(t, mr, options{check: true, redisDB: -1, topicGeneration: -1})
	assert.Equal(t, exitOwnerInstances, check.code, "p* is empty, but an owner instance is still registered")
	assert.Contains(t, check.stderr, "db-b:6000")

	require.NoError(t, rc.LPush(context.Background(), pKey("0"), v2Payload()).Err())
	requeue := runTool(t, mr, options{redisDB: -1, topicGeneration: -1})
	assert.Equal(t, exitOwnerInstances, requeue.code)
	n, err := rc.LLen(context.Background(), pKey("0")).Result()
	require.NoError(t, err)
	assert.Equal(t, int64(1), n, "nothing is moved while an owner instance is registered")
}

// 没登记模式的实例(本版本之前的镜像)不算 owner:p* 非 0 时 -check 照旧以 3 退出;搬回把 p* 搬回收件箱并以 0 退出。
func TestRun_UnknownModeInstanceFallsBackToResidualCheck(t *testing.T) {
	mr, rc := newToolRedis(t)
	register(t, rc, "db-old:6000", "")
	require.NoError(t, rc.LPush(context.Background(), pKey("1"), v2Payload()).Err())

	check := runTool(t, mr, options{check: true, redisDB: -1, topicGeneration: -1})
	assert.Equal(t, exitResidual, check.code, check.stderr)
	assert.Contains(t, check.stdout, "instance db-old:6000 processing = 0 mode = unknown")

	requeue := runTool(t, mr, options{redisDB: -1, topicGeneration: -1})
	assert.Equal(t, 0, requeue.code, requeue.stderr)
	inbox, err := rc.LRange(context.Background(), inboxKey(), 0, -1).Result()
	require.NoError(t, err)
	assert.Equal(t, []string{v2Payload()}, inbox)
}
