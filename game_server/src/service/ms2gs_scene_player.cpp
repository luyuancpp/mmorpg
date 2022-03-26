#include "ms2gs_scene_player.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

using namespace common;
///<<< END WRITING YOUR CODE

namespace ms2gsscene{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void PlayerC2GsServiceImpl::OnLogin(common::EntityPtr& entity,
    const ms2gsscene::OnLoginRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE OnLogin
    LOG_INFO << "player login" << reg.get<common::Guid>(entity.entity());
///<<< END WRITING YOUR CODE OnLogin
}

///<<<rpc end
}// namespace ms2gsscene
