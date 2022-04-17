#include "rgscene_rg.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

namespace regionservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void RgServiceImpl::StartCrossMainGS(common::EntityPtr& entity,
    const regionservcie::StartMainRoomGSRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE StartCrossMainGS
    
///<<< END WRITING YOUR CODE StartCrossMainGS
}

///<<<rpc end
void RgServiceImpl::StartCrossRoomGS(common::EntityPtr& entity,
    const regionservcie::StartCrossRoomGSRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE StartCrossRoomGS
    
///<<< END WRITING YOUR CODE StartCrossRoomGS
}

///<<<rpc end
}// namespace regionservcie
