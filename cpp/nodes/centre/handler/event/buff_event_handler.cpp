#include "buff_event_handler.h"
#include "threading/dispatcher_manager.h"

void BuffEventHandler::Register()
{
    dispatcher.sink<BuffTestEvet>().connect<&BuffEventHandler::BuffTestEvetHandler>();
}

void BuffEventHandler::UnRegister()
{
    dispatcher.sink<BuffTestEvet>().disconnect<&BuffEventHandler::BuffTestEvetHandler>();
}
void BuffEventHandler::BuffTestEvetHandler(const BuffTestEvet& event)
{

}
