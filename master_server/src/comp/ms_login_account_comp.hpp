#ifndef MASTER_SRC_COMP_MS_LOGIN_ACCOUNT_COMP_H_
#define MASTER_SRC_COMP_MS_LOGIN_ACCOUNT_COMP_H_

#include "src/game_logic/entity_class/entity_class.h"

namespace master
{
struct MSLoginAccount : public common::EntityPtr
{
    common::GuidVector playing_guids_;
};
struct AccountLoginNode
{
    uint32_t node_id_{0};
};
}//namespace master

#endif//#ifndef MASTER_SRC_COMP_MS_LOGIN_ACCOUNT_COMP_H_