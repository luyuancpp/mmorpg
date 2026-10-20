#pragma once

#include "engine/core/utils/registry/scene_entity.h"

#include <cstddef>

class CreateSceneRequest;
class SceneInfoComp;

class SceneLifecycle
{
public:
    // 仅在节点 EventLoop 调用。入口先校验非零 scene_id、按号去重,并持有 Agones 创建许可。
    // config_id 的传输层差异暂保留:muduo 要求非零,gRPC 允许零,此处不另加准入规则。
    // 返回值借用 sceneRegistry 中的组件,调用方须在下一次 registry 修改前复制回包。
    [[nodiscard]] static const SceneInfoComp& CreateScene(const CreateSceneRequest& request);

    // 入口先按业务号找到有效场景;返回本次排空人数,非零时场景保留,零表示已经销毁。
    [[nodiscard]] static std::size_t DestroyScene(SceneEntity scene);
};
