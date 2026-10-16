#pragma once

// scene 组合根安装本进程事件监听;测试可单独安装任务域,不依赖 proto 生成接线。
namespace scene_local_events
{
void RegisterAll();
void UnregisterAll();
void RegisterMission();
void UnregisterMission();
}
