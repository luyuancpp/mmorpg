﻿#pragma once
#include "type_define/type_define.h"

namespace google::protobuf
{
    class Message;
}

class ActorCreateS2C;

class ViewUtil
{
public:
    // 初始化函数
    static void Initialize();

    // 检查是否应该发送NPC进入消息，例如在没有NPC任务或多人副本中不优先关注NPC
    static bool ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entrant);

    // 检查是否应该更新视图
    static bool ShouldUpdateView(entt::entity observer, entt::entity entrant);

    // 填充Actor创建消息信息
    static void FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& actorCreateMessage);

    // 处理玩家离开消息
    static void HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver);
    
    static void BroadcastToNearbyEntities(entt::entity entity, const uint32_t message_id,
    const google::protobuf::Message& message, bool excludingSel);
    
    //给能看见我的玩家包括我广播消息
    static void BroadcastMessageToVisiblePlayers(entt::entity entity, const uint32_t message_id,
    const google::protobuf::Message& message);

    // 检查参与者是否超出视野半径
    static bool IsWithinViewRadius(entt::entity observer, entt::entity entrant, double view_radius);
    
    // 获取观察者的最大视野半径
    static double GetMaxViewRadius(entt::entity observer);
private:
    // 初始化Actor消息
    static void InitializeActorMessages();

    // 检查观察者和参与者是否都是NPC
    static bool BothAreNpcs(entt::entity observer, entt::entity entrant);

    // 检查参与者是否为NPC
    static bool EntrantIsNpc(entt::entity entrant);

    // 如果需要刷新视图，返回false
    static bool ShouldRefreshView();
};

