package logic

// 跨 zone 重定向读排空标记(drainingGateIDs)的 I/O 半边:键名与下标对应、失败放行、退化输入。
// 纯函数 signRedirectToGate 的排空过滤顺序见 gate_redirect_client_endpoint_test.go。

import (
	"context"
	"fmt"
	"strconv"
	"testing"

	commonpb "proto/common/base"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

func drainingTestNodes(ids ...uint32) []*commonpb.NodeInfo {
	nodes := make([]*commonpb.NodeInfo, 0, len(ids))
	for _, id := range ids {
		nodes = append(nodes, testGateNode(id, 100, 0, testEndpoint(fmt.Sprintf("10.0.0.%d", id), 18000), nil))
	}
	return nodes
}

// 只把被标记的那台认作排空中:键是 gate:<node_id>:draining,值是标记秒;下标与 nodes 一一对应。
func TestDrainingGateIDs_ReadsMarksByNodeID(t *testing.T) {
	mr := miniredis.RunT(t)
	rds := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})
	require.NoError(t, mr.Set(fmt.Sprintf(gateDrainingKeyFmt, 2), strconv.FormatInt(1_800_000_000, 10)))

	got := drainingGateIDs(context.Background(), rds, drainingTestNodes(1, 2, 3))

	assert.Equal(t, map[uint32]bool{2: true}, got)
}

// Redis 读失败时放行(返回 nil),与 login 的 loginqueue.DrainingGates 同语义:不因读不到标记就拒绝重定向。
func TestDrainingGateIDs_RedisFailureIsFailOpen(t *testing.T) {
	// 自己管理生命周期(不用 RunT):用例中途要关掉它。
	mr, err := miniredis.Run()
	require.NoError(t, err)
	rds := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})
	require.NoError(t, mr.Set(fmt.Sprintf(gateDrainingKeyFmt, 1), "1800000000"))
	mr.Close()

	assert.Nil(t, drainingGateIDs(context.Background(), rds, drainingTestNodes(1, 2)))
}

// 退化输入:没有 Redis 或没有节点时不查、返回 nil。
func TestDrainingGateIDs_DegenerateInputs(t *testing.T) {
	mr := miniredis.RunT(t)
	rds := redis.MustNewRedis(redis.RedisConf{Host: mr.Addr(), Type: "node"})

	assert.Nil(t, drainingGateIDs(context.Background(), nil, drainingTestNodes(1)))
	assert.Nil(t, drainingGateIDs(context.Background(), rds, nil))
}
