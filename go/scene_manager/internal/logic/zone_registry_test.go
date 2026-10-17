package logic

// zone 登记集合(zone_registry.go)的契约用例。行为侧(清扫 / 孤儿清理按登记发现、不 SCAN)见
// load_refresh_scope_test.go。

import (
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// 键名是跨 module 契约(tools/merge_zone/scene_hot_state.go 的 sceneManagerZonesKey 同值,那边也钉死);
// 它不得以任何按 zone 前缀 SCAN / 清理的模式开头,否则旧版本 scene_manager 的 SCAN 与 merge_zone 会误命中。
func TestSceneManagerZonesKey_PinnedAndOutsideLegacyPatterns(t *testing.T) {
	assert.Equal(t, "scene_manager:zones", sceneManagerZonesKey)
	for _, prefix := range []string{"scene_nodes:zone:", "world_channels:zone:", "world_channels:desired:zone:", "node:zone:"} {
		assert.False(t, strings.HasPrefix(sceneManagerZonesKey, prefix), "不得落进 %s* 模式", prefix)
	}
	_, _, isChannelSet := parseWorldChannelsKey(sceneManagerZonesKey)
	assert.False(t, isChannelSet)
}

// 脏成员跳过、不报错;结果去重升序。
func TestListRegisteredZones_SkipsMalformedMembers(t *testing.T) {
	sc, mr := newTestSvcCtx(t, "")
	_, err := mr.SAdd(sceneManagerZonesKey, "2", "abc", "4294967296", "1", "-1")
	require.NoError(t, err)

	zones, err := listRegisteredZones(sc)
	require.NoError(t, err)
	assert.Equal(t, []uint32{1, 2}, zones)
}

func TestListRegisteredZones_EmptyWhenKeyMissing(t *testing.T) {
	sc, _ := newTestSvcCtx(t, "")
	zones, err := listRegisteredZones(sc)
	require.NoError(t, err)
	assert.Empty(t, zones)
}

func TestUnionZones_DedupesAndSorts(t *testing.T) {
	assert.Equal(t, []uint32{0, 1, 3, 7}, unionZones([]uint32{7, 1}, nil, []uint32{3, 1, 0}))
	assert.Empty(t, unionZones())
}

// 空参数不发命令;重复的 zone 只登记一次。
func TestRegisterSceneManagerZones_DedupesAndSkipsEmpty(t *testing.T) {
	sc, mr := newTestSvcCtx(t, "")
	rec := installRedisCommandRecorderForTest(t, mr, nil)

	require.NoError(t, registerSceneManagerZones(sc))
	assert.Zero(t, rec.count("SADD", ""), "空参数不得发命令")

	require.NoError(t, registerSceneManagerZones(sc, 3, 1, 3))
	assert.Equal(t, 1, rec.count("SADD", sceneManagerZonesKey), "一次登记只发一条 SADD")
	members, err := mr.SMembers(sceneManagerZonesKey)
	require.NoError(t, err)
	assert.ElementsMatch(t, []string{"1", "3"}, members)
}

// 每个副本启动时登记自己的 ZoneId;ZoneId 为 0(未配置)不登记。
func TestRegisterOwnZoneOnStartup(t *testing.T) {
	sc, mr := newTestSvcCtx(t, "")
	rec := installRedisCommandRecorderForTest(t, mr, nil)

	sc.Config.ZoneId = 0
	registerOwnZoneOnStartup(sc)
	assert.Zero(t, rec.count("SADD", ""), "未配置 ZoneId 不登记")

	sc.Config.ZoneId = 7
	registerOwnZoneOnStartup(sc)
	ok, err := mr.SIsMember(sceneManagerZonesKey, "7")
	require.NoError(t, err)
	assert.True(t, ok)
}
