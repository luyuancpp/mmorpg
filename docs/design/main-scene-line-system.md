# 主世界场景分线系统 (Main World Scene Lines)

## 概念

每个主世界地图 (config_id) 可以有 N 条线 (channel)。玩家进入时自动分配到人数最少的线。线在启动时预创建，生命周期与区服一致。

## 配置

`scene_manager_service.yaml`:
```yaml
MainSceneLineCount: 3   # 每个主世界场景的线数，默认 1（向后兼容）
MainSceneConfIds:
  - 1001
  - 1002
```

## Redis 数据结构

| Key | Type | 说明 |
|-----|------|------|
| `main_scene_lines:zone:{zoneId}:{confId}` | SET | 该地图所有线的 sceneId |
| `instance:{sceneId}:player_count` | STRING | 每条线的玩家数（与副本共用） |
| `scene:{sceneId}:node` | STRING | sceneId → nodeId 映射 |

## 核心流程

### 启动创建线
```
initMainScenesForZone(zoneId, confIds)
  for each confId:
    existing = SMEMBERS(main_scene_lines:zone:{zoneId}:{confId})
    for i = len(existing) to MainSceneLineCount-1:
      sceneId = INCR(scene:id_counter)
      SET scene:{sceneId}:node = assignNodeByHash(confId*1000+i, nodes)
      SADD main_scene_lines:zone:{zoneId}:{confId} sceneId
      SET instance:{sceneId}:player_count = 0
    for each sceneId in all lines:
      RPC → C++ CreateScene(config_id, scene_id)   // 幂等
```

### 玩家选线
```
GetBestMainSceneLine(confId, zoneId)
  members = SMEMBERS(main_scene_lines:zone:{zoneId}:{confId})
  for each sceneId in members:
    count = GET(instance:{sceneId}:player_count)
  return sceneId with lowest count
```

### 玩家进出
- `EnterScene` → `IncrInstancePlayerCount(sceneId)` — 已有机制，无需修改
- `LeaveScene` → `DecrInstancePlayerCount(sceneId)` — 已有机制，无需修改

## Proto 变更

`proto/scene/scene.proto`:
```protobuf
message CreateSceneRequest {
  uint32 config_id = 1;
  uint64 scene_id = 2;  // Go 分配的唯一 ID，C++ 按此去重
}
```

## C++ 去重逻辑

`scene_handler.cpp` CreateScene:
- `scene_id > 0`: 按 `guid == scene_id` 去重（支持同 config_id 多实体 = 多线）
- `scene_id == 0`: 按 `config_id` 去重（向后兼容）
- 新实体的 `guid` 使用 Go 传入的 `scene_id`

## 节点分配

`assignNodeByHash(confId*1000 + lineIdx, sortedNodes)` — 不同线可分配到不同节点，哈希确定性分配。

## 向后兼容

- `MainSceneLineCount` 默认 1 → 行为与旧版完全一致（每个 confId 一个场景）
- `scene_id = 0` 时 C++ 仍按 `config_id` 去重

## 测试覆盖

| 测试用例 | 验证内容 |
|---------|---------|
| `TestInitMainScenes_MultipleLines` | 3 条线各有独立 sceneId |
| `TestInitMainScenes_MultipleLines_Idempotent` | 重复初始化不产生多余线 |
| `TestGetBestMainSceneLine_SelectsLowestPlayerCount` | 选择人数最少的线 |
| `TestGetBestMainSceneLine_AllEmpty` | 所有线为空时仍能返回 |
| `TestCreateScene_MainWorld_MultipleLines_ReturnsLeastLoaded` | 高负载线→自动切换 |
| `TestCreateScene_MainWorld_LineCountDefault1_BackwardCompat` | LineCount=1 等同旧行为 |
