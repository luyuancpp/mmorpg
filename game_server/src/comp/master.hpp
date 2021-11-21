#ifndef GAME_SERVER_SRC_COMP_CMASTER_HPP_
#define GAME_SERVER_SRC_COMP_CMASTER_HPP_

#include <memory>

#include "src/server_common/rpc_client.h"

namespace game
{
using MasterClientPtr = std::shared_ptr<common::RpcClient>;

}//namespace game

#endif//GAME_SERVER_SRC_COMP_CMASTER_HPP_
