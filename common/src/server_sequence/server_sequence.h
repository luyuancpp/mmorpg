#ifndef SRC_SERVER_SEQUEUENCE_SERVER_SEQUENCE_H_
#define SRC_SERVER_SEQUEUENCE_SERVER_SEQUENCE_H_

#include "src/common_type/common_type.h"

namespace common
{
class ServerSequence
{
public:
    void set_server_id(uint32_t server_id)
    {
        server_id_ = server_id;
    }

    GameGuid Generate()
    {
        return server_id_ ^ ++seq_;
    }
private:
    uint32_t server_id_{ 0 };
    uint32_t seq_{ 0 };
};

}//namespace common

#endif//SRC_SERVER_SEQUEUENCE_SERVER_SEQUENCE_H_


