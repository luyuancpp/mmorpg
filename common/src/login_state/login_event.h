#ifndef COMMON_SRC_LOGIN_STATE_LOGIN_EVENT_H_
#define COMMON_SRC_LOGIN_STATE_LOGIN_EVENT_H_

namespace common
{
    struct EeventLoginSetState
    {
        EeventLoginSetState(int32_t state_id)
            : state_id_(state_id){}
        int32_t state_id_{ 0};
    };
}//namespace common

#endif//COMMON_SRC_LOGIN_STATE_LOGIN_EVENT_H_