#ifndef MASTER_SRC_COMP_MS_LOGIN_ACCOUNT_COMP_H_
#define MASTER_SRC_COMP_MS_LOGIN_ACCOUNT_COMP_H_

#include "src/game_logic/entity/entity.h"

namespace master
{
struct MSLoginAccount : public common::EntityPtr
{
    common::GuidVector playing_guids_;
};
struct AccountLoginNode
{
    uint32_t login_node_id_{UINT32_MAX};
    uint32_t gate_node_id_{ UINT32_MAX };
};
}//namespace master

#endif//#ifndef MASTER_SRC_COMP_MS_LOGIN_ACCOUNT_COMP_H_