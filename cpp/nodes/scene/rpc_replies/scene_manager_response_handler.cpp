#include "scene_manager_response_handler.h"

#include "grpc_client/scene_manager/scene_manager_service_grpc_client.h"
#include "muduo/base/Logging.h"

void InitSceneManagerReply()
{
    scene_manager::AsyncSceneManagerEnterSceneHandler =
        [](const grpc::ClientContext& /*ctx*/, const ::scene_manager::EnterSceneResponse& resp)
    {
        if (resp.error_code() != 0)
        {
            LOG_ERROR << "SceneManager.EnterScene error: code=" << resp.error_code()
                      << " msg=" << resp.error_message();
            return;
        }

        if (resp.has_redirect())
        {
            LOG_INFO << "SceneManager.EnterScene returned cross-zone redirect to "
                     << resp.redirect().target_gate_ip() << ":"
                     << resp.redirect().target_gate_port();
        }
    };
}
