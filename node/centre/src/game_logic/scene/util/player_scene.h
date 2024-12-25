﻿#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "type_define/type_define.h"

class PlayerSceneSystem
{
public:
    static void HandleLoginEnterScene(entt::entity player);
    static void SendToGameNodeEnterScene(entt::entity player);
    static void ProcessPlayerEnterGameServer(entt::entity player, NodeId node_id);
    static void AttemptEnterNextScene(entt::entity player);
    static uint32_t GetDefaultSceneConfigurationId();
};


//跨服间切换,如果另一个跨服满了就不应该进去了
//如果是跨服，就应该先跨服去处理
//原来服务器之间换场景，不用通知跨服离开场景
//todo 如果是进出镜像，一定保持在原来的gs切换,主世界分线和镜像没关系，这样就节省了玩家切换流程，效率也提高了
//todo 跨服的时候重新上线
 //跨服到原来服务器，通知跨服离开场景，todo注意回到原来服务器的时候可能原来服务器满了