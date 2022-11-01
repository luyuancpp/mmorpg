#include "ms_scene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/tips_id.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
#include "src/controller_server.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"

#include "component_proto/scene_comp.pb.h"
#include "logic_proto/scene_rg.pb.h"
#include "gs_service.pb.h"

using GsStubPtr = std::unique_ptr<RpcStub<gsservice::GsService_Stub>>;


using EnterRegionMainRpc = std::shared_ptr<NormalClosure<regionservcie::EnterCrossMainSceneRequest, regionservcie::EnterCrossMainSceneResponese>>;
void EnterRegionMainSceneReplied(EnterRegionMainRpc replied)
{
    // todo 跨服切换不行，return error
    //切跨到b服过程中，跨服没返回又切到c，跨服回来再到c目前就不考虑这种情况了，考虑的话写代码麻烦
    //todo 异步跨服返回来之前又去切换场景，导致已经切换到别的场景了，再切的话可能就不对了，不考虑这种情况了，正常人不会切那么快
    auto player = g_player_list->GetPlayer(replied->s_rq_.player_id());
    if (entt::null == player)
    {
        LOG_ERROR << "player not found" << replied->s_rq_.player_id();
        return;
    }
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerMsChangeSceneQueue);
	if (change_scene_queue.empty())
	{
		return;
	}
	auto scene = ScenesSystem::get_scene(replied->s_rq_.scene_id());
	if (entt::null == scene)
	{
		LOG_ERROR << "scene not found" << replied->s_rq_.scene_id();
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}
	auto& change_scene_info = change_scene_queue.front();
	change_scene_info.set_change_cross_server_status(MsChangeSceneInfo::eEnterCrossServerSceneSucceed);
	PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
}

//前一个队列完成的时候才应该调用到这里去判断当前队列
void UpdateFrontChangeSceneInfoInitState(entt::entity player)
{
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerMsChangeSceneQueue);
	if (change_scene_queue.empty())
	{
		return;
	}
	auto try_from_scene = registry.try_get<SceneEntity>(player);
	if (nullptr == try_from_scene)
	{
		PlayerTipSystem::Tip(player, kRetEnterSceneYourSceneIsNull, {});// todo 
		return;
	}
	auto& change_scene_info = change_scene_queue.front();
	if (change_scene_info.processing())
	{
		return;
	}
	change_scene_info.set_processing(true);
	auto& scene_info = change_scene_info.scene_info();
	auto scene_id = scene_info.scene_id();
	entt::entity to_scene = entt::null;
	if (scene_id <= 0)//用scene_config id 去换本服的ms
	{
		GetSceneParam getp;
		getp.scene_confid_ = scene_info.scene_confid();
		to_scene = ServerNodeSystem::GetWeightRoundRobinMainScene(getp);
		if (entt::null == to_scene)
		{
			PlayerTipSystem::Tip(player, kRetEnterSceneSceneFull, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
			return;
		}
		scene_id = registry.get<SceneInfo>(to_scene).scene_id();
	}
	else
	{
		to_scene = ScenesSystem::get_scene(scene_id);
		if (entt::null == to_scene)
		{
			PlayerTipSystem::Tip(player, kRetEnterSceneSceneNotFound, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
			return;
		}
	}

	auto try_from_scene_gs = registry.try_get<GsNodePtr>(try_from_scene->scene_entity_);
	auto try_to_scene_gs = registry.try_get<GsNodePtr>(to_scene);
	if (nullptr == try_from_scene_gs || nullptr == try_to_scene_gs)
	{
		LOG_ERROR << " scene null : " << (nullptr == try_from_scene_gs) << " " << (nullptr == try_to_scene_gs);
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}
	auto from_gs_it = g_gs_nodes.find((*try_from_scene_gs)->node_id());
	auto to_gs_it = g_gs_nodes.find((*try_to_scene_gs)->node_id());
	if (from_gs_it == g_gs_nodes.end() || to_gs_it == g_gs_nodes.end())
	{
		LOG_ERROR << " gs not found  : " <<
			(*try_from_scene_gs)->node_id() <<
			" " << (*try_to_scene_gs)->node_id();
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}
	entt::entity from_gs = from_gs_it->second;
	entt::entity to_gs = to_gs_it->second;
	bool is_from_gs_is_cross_server = registry.any_of<CrossMainSceneServer>(from_gs);
	bool is_to_gs_is_cross_server = registry.any_of<CrossMainSceneServer>(to_gs);

	//不是跨服才在本地判断,跨服有自己的判断
	if (!change_scene_info.ignore_full() && !is_to_gs_is_cross_server)
	{
		auto ret = ScenesSystem::CheckScenePlayerSize(to_scene);
		if (kRetOK != ret)
		{
			PlayerTipSystem::Tip(player, ret, {});
			PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
			return;
		}
	}

	if (entt::null != from_gs)
	{
		if (from_gs == to_gs)
		{
			change_scene_info.set_change_gs_type(MsChangeSceneInfo::eSameGs);
		}
		else if (from_gs != to_gs)
		{
			change_scene_info.set_change_gs_type(MsChangeSceneInfo::eDifferentGs);
		}		
	}

	//跨服间切换,如果另一个跨服满了就不应该进去了
	//如果是跨服，就应该先跨服去处理
	//原来服务器之间换场景，不用通知跨服离开场景
	//todo 如果是进出镜像，一定保持在原来的gs切换,主世界分线和镜像没关系，这样就节省了玩家切换流程，效率也提高了
	//todo 跨服的时候重新上线
	//目标场景是跨服场景，通知跨服去换,跨服只做人数检测，不做其他的事情

	if (is_from_gs_is_cross_server || is_to_gs_is_cross_server)
	{
		change_scene_info.set_change_cross_server_type(MsChangeSceneInfo::eCrossServer);
		change_scene_info.set_change_cross_server_status(MsChangeSceneInfo::eEnterCrossServerScene);
		if (is_from_gs_is_cross_server)
		{
			//跨服到原来服务器，通知跨服离开场景，todo注意回到原来服务器的时候可能原来服务器满了
			regionservcie::LeaveCrossMainSceneRequest rpc;
			rpc.set_player_id(registry.get<Guid>(player));
			g_controller_node->rg_stub().CallMethod(rpc, &regionservcie::RgService_Stub::LeaveCrossMainScene);
		}
		if (is_to_gs_is_cross_server)
		{
			//注意虽然一个逻辑，但是不一定是在leave后面处理
			EnterRegionMainRpc rpc(std::make_shared<EnterRegionMainRpc::element_type>());
			rpc->s_rq_.set_scene_id(registry.get<SceneInfo>(to_scene).scene_id());
			rpc->s_rq_.set_player_id(registry.get<Guid>(player));
			g_controller_node->rg_stub().CallMethod(EnterRegionMainSceneReplied, rpc, &regionservcie::RgService_Stub::EnterCrossMainScene);
			return;
		}
	}
	else
	{
		change_scene_info.set_change_cross_server_type(MsChangeSceneInfo::eDotnotCrossServer);
	}

	if (MsChangeSceneInfo::eDotnotCrossServer == change_scene_info.change_cross_server_status() )
	{
		PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);//不跨服就开始处理同一个gs 或者不同gs
		return;
	}	
	
}
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::EnterSceneGs2Ms(entt::entity player,
    const ::Gs2MsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    //正在切换场景中，不能马上切换，gs崩溃了怎么办
    MsChangeSceneInfo change_scene_info;
    change_scene_info.mutable_scene_info()->CopyFrom(request->scene_info());
	auto ret = PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_scene_info);
	if (ret != kRetOK)
	{
		PlayerTipSystem::Tip(player, ret, {});
		return;
	}
	UpdateFrontChangeSceneInfoInitState(player);
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::EnterSceneMs2Gs(entt::entity player,
    const ::Ms2GsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::LeaveSceneGs2Ms(entt::entity player,
    const ::Gs2MsLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::LeaveSceneMs2Gs(entt::entity player,
    const ::Ms2GsLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::Gs2MsLeaveSceneAsyncSavePlayerComplete(entt::entity player,
    const ::Gs2MsLeaveSceneAsyncSavePlayerCompleteRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    //异步切换考虑消息队列
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerMsChangeSceneQueue);
	if (change_scene_queue.empty())
	{
		return;
	}
	auto& change_scene_info = change_scene_queue.front();
	LOG_DEBUG << "Gs2MsLeaveSceneAsyncSavePlayerComplete  change scene " << change_scene_info.processing();
	auto to_scene = ScenesSystem::get_scene(change_scene_info.scene_info().scene_id());
    //todo异步加载完场景已经不在了scene了

	if (entt::null == to_scene)//todo 场景崩溃了要去新的场景
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << registry.get<Guid>(player);
		return;
	}
	PlayerChangeSceneSystem::SetChangeGsStatus(player, MsChangeSceneInfo::eLeaveGsSceneSucceed);

    auto try_player_session = registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << registry.get<Guid>(player);
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;

    }
    else
    {
		//todo gs崩溃
        auto* p_gs_data = registry.try_get<GsNodePtr>(to_scene);
        if (nullptr == p_gs_data)//找不到gs了，放到好的gs里面
        {
            // todo default
            LOG_ERROR << "player " << registry.get<Guid>(player) << " enter default secne";
        }
        else
        {
            try_player_session->gs_ = *p_gs_data;
        }
    }
    PlayerSceneSystem::CallPlayerEnterGs(player);
///<<< END WRITING YOUR CODE
}

///<<<rpc end
