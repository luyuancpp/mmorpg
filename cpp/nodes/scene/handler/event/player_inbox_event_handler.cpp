#include "player_inbox_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
#include "player/system/player_inbox_system.h"
///<<< END WRITING YOUR CODE
void PlayerInboxEventHandler::Register()
{
    tlsEcs.dispatcher.sink<PlayerInboxNotifyEvent>().connect<&PlayerInboxEventHandler::PlayerInboxNotifyEventHandler>();
}

void PlayerInboxEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<PlayerInboxNotifyEvent>().disconnect<&PlayerInboxEventHandler::PlayerInboxNotifyEventHandler>();
}
void PlayerInboxEventHandler::PlayerInboxNotifyEventHandler(const PlayerInboxNotifyEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    // player_inbox 服务 --Kafka SceneCommand{DispatchEvent}--> DispatchProtoEvent 进程内分发到此(loop 线程)。
    // 只是提醒,不带条目:找到实体就把下一次检查提前到现在,找不到就丢弃,下次落地会拉(player-inbox-service.md §7.13)。
    PlayerInboxSystem::OnNotifyEvent(event.player_id(), event.domain());
///<<< END WRITING YOUR CODE
}
