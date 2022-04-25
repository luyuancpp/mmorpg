#include "MatchRoom.h"

#ifndef __TEST__
#include "ModuleScene.pb.h"
#endif // !__TEST__

#include "GenCode/GameDefine_Result.h"
#include "GenCode/Match/MatchModule.h"
#include "GenCode/StringNotice/StringNoticeModule.h"
#include "GenCode/StringNotice.h"
#include "GameEnum.h"
#ifndef __TEST__
#include "Game/DK_TimeManager.h"
#endif

namespace GameMMR
{
  
 Snowflake MatchRoom::m_oSnowflake;
 const CreateMatchRoomParam::match_cb_type CreateMatchRoomParam::kEmptyRematchCallback;
 const CreateMatchRoomParam::remove_unit_cb_type  CreateMatchRoomParam::kEmptyRemoveUnitCallback;

 MatchRoom::MatchRoom(CreateMatchRoomParam & cp
 )
     : m_nGroup(cp.groupId),
     m_nMatchType(cp.matchType),
     m_oReMatchCallBack(cp.matchcb),
     m_oRemoveMatchUnitCallBack(cp.rucb),
     m_vCamp(cp.v),
     m_p_team_list_(cp.p_team_list_),
     m_oSendCallBack(cp.sendcb),
     m_oMirrorMatchCallBack(cp.m_oMirrorMatchCallBack),
     team_id_(cp.team_id_),
     team_all_ready_callback_(cp.team_all_ready_callback_),
     name_(cp.name),
     create_guid_(cp.create_guid_)
 {
     m_nRoomId = m_oSnowflake.generate(m_nRoomId);

     if (nullptr == cp.p_match_unit_)
     {
         int32_t campId = 0;
         //camps
         for (room_camps_list_type::iterator it = m_vCamp.begin(); it != m_vCamp.end(); ++it)
         {
             camp_mu_list_type & v = (*it);
             //camp group
             for (camp_mu_list_type::iterator ji = v.begin(); ji != v.end(); ++ji)
             {
                 //group per unit
                 match_unit_list_type & ts = (*ji);
                 for (match_unit_list_type::iterator ki = ts.begin(); ki != ts.end(); ++ki)
                 {
                     for (auto && mi : ki->second->GetPlayers())
                     {
                         mi.second.set_ready(E_STATUS_NONE);
                         mi.second.set_camp(campId);
                         m_vMatchInfoList.emplace(mi.first, mi.second);
                     }
                     m_vUnits.emplace(ki->first, ki->second);
                 }

             }
             ++campId;
         }
     }
     else
     {
         m_vUnits.emplace(cp.p_match_unit_->GetUnitId(), cp.p_match_unit_);
         for (auto && it : m_vUnits)
         {
             for (auto && pi : it.second->GetPlayers())
             {
                 pi.second.set_ready(E_STATUS_NONE);
                 m_vMatchInfoList.emplace(pi.first, pi.second);
             }

         }
     }

#ifndef __TEST__

     if (m_nMatchType == E_NOMAL_TEAM_MATCH)
     {
         for (auto && it : m_vMatchInfoList)
         {
             if (m_p_team_list_->IsLeader(it.first))
             {
                 it.second.set_ready(E_STATUS_READY);
             }
         }
     }
#endif // !__TEST__
	 if (cp.p_match_unit_)
	 {
		 for (auto && it : cp.p_match_unit_->GetPlayers())
		 {
			 if (it.second.robot())
			 {
				 RobotReady(it.second.roleid());
			 }
		 }
	 }
     time_out_ = g_pTimeManager->CurrentTimeS() + cp.prepare_seconds_;
     InitTarget();
     NotifyClientPrepare();

	 auto iterteammem = m_vUnits.begin();
	 if (iterteammem == m_vUnits.end())
	 {
		 return;
	 }

	 Team::team_id_type tId = iterteammem->second->GetTeamId();
	 // 镜像的话 进入星耀计数操作
	 if (E_SCENE_MIRROR == m_oTarget.scenetype())
	 {
		 m_oMirrorMatchCallBack(m_oTarget.sceneconfigid(), m_oTarget.starbattleid(), MIRROR_MATCH_IN, tId);
	 }

     
 }

 void MatchRoom::ToClientPb(PBMatchRoom & pb)
 {
     pb.Clear();

     for (room_camps_list_type::iterator it = m_vCamp.begin(); it != m_vCamp.end(); ++it)
     {
         ::MatchRoomCamps* p = pb.add_camps();

         for (camp_mu_list_type::iterator ji = it->begin(); ji != it->end(); ++ji)
         {
             for (match_unit_list_type::iterator ki = ji->begin(); ki != ji->end(); ++ki)
             {
                 for (auto & mi : ki->second->GetPlayers())
                 {
                     p->add_players()->CopyFrom(mi.second);
                 }
             }
         }
     }

     if (m_vCamp.empty())
     {
         ::MatchRoomCamps* p = pb.add_camps();
         for (auto && mi : m_vMatchInfoList)
         {
             p->add_players()->CopyFrom(mi.second);
         }
     }


     pb.mutable_matchtarget()->CopyFrom(m_oTarget);
     pb.set_roomid(GetRoomId());
     pb.set_matchtype(m_nMatchType);
     pb.set_endtime(time_out_);
 }

 bool MatchRoom::TestTeamInTheSameCamp(TeamInfo & teamInfo)
 {
     std::unordered_set<int32_t> campSet;
     for (int32_t i = 0; i < teamInfo.members_size(); ++i)
     {
         auto it = m_vMatchInfoList.find(teamInfo.members(i).roleid());
         if (it == m_vMatchInfoList.end())
         {
             continue;
         }
         campSet.emplace(it->second.camp());
     }
     bool ret = campSet.size() == 1;
     return campSet.size() == 1;
 }

 bool MatchRoom::InRoome(GUID_t playerId)
 {
    return  m_vMatchInfoList.find(playerId) != m_vMatchInfoList.end();
 }

 void MatchRoom::OnAllPlayerReady()
 {

 }

 void MatchRoom::RemoveRoom()
 {
     m_vUnits.clear();
     m_vMatchInfoList.clear();

 }

 void MatchRoom::InitTarget()
 {
     if (m_vUnits.empty() )
     {
         return;
     }
     m_oTarget = m_vUnits.begin()->second->GetMatchTarget();
 }

 void MatchRoom::NotifyClientPrepare()
 {
     if (!m_oSendCallBack)
     {
         return;
     }
     MatchRpcPrepareRoomNotify n;
     ToClientPb(*n.mutable_readyroom());
     for (auto && rId : m_vMatchInfoList)
     {
         m_oSendCallBack(rId.first, ModuleMatch::RPC_CODE_MATCH_PREPAREROOM_NOTIFY, n);
     }

 }

 int32_t MatchRoom::RobotReady(GUID_t playerId)
 {
	 matchinfo_pb_list::iterator it = m_vMatchInfoList.find(playerId);
	 if (it == m_vMatchInfoList.end())
	 {
		 return OR_MATCH_NO_IN_ROOM;
	 }
	 it->second.set_ready(E_STATUS_READY);
	 return OR_OK;
 }
 int32_t MatchRoom::Ready(GUID_t playerId)
 {
     matchinfo_pb_list::iterator it = m_vMatchInfoList.find(playerId);
     if (it == m_vMatchInfoList.end())
     {
         return OR_MATCH_NO_IN_ROOM;
     }
     it->second.set_ready(E_STATUS_READY);

    if (IsAllReady())
    {
        OnEnterDungeon();
        if (team_all_ready_callback_)
        {
            team_all_ready_callback_(m_nRoomId);
        }
    }

    if (m_oSendCallBack)
    {

        MatchRpcReadyReply r;
        r.set_result(OR_OK);
        r.set_roleid(playerId);
        m_oSendCallBack(playerId, ModuleMatch::RPC_CODE_MATCH_READY_REQUEST, r);

        MatchRpcMemberReadyNotify msg;
        msg.set_roleid(playerId);
        for (auto && rId : m_vMatchInfoList)
        {
            m_oSendCallBack(rId.first, ModuleMatch::RPC_CODE_MATCH_MEMBERREADY_NOTIFY, msg);
        }
    }

     return OR_OK;
 }
   
 int32_t MatchRoom::Cancel(GUID_t playerId)
 {
     bool bInRoom = false;

     if (!InRoome(playerId))
     {
         return OR_MATCH_NO_IN_ROOM;
     }
     // send msg
     if (m_oSendCallBack)
     {

         MatchRpcCancelReadyReply r;
         r.set_roleid(playerId);
         r.set_result(OR_OK);
         m_oSendCallBack(playerId, ModuleMatch::RPC_CODE_MATCH_CANCELREADY_REQUEST, r);

         MatchRpcRoomCancelNotify msg;
         StringNoticeRpcNoticeNotify oNotify;
         oNotify.set_configid(E_NOTICE_TEAM_MEMBER_CANCEL_READY);
         matchinfo_pb_list::iterator playerit = m_vMatchInfoList.find(playerId);
         if (playerit != m_vMatchInfoList.end())
         {
             *oNotify.add_param() = playerit->second.name();
         }

         for (auto && rId : m_vMatchInfoList)
         {
             m_oSendCallBack(rId.first, ModuleMatch::RPC_CODE_MATCH_ROOMCANCEL_NOTIFY, msg);
             if (rId.first != playerId)
             {
                 m_oSendCallBack(rId.first, ModuleStringNotice::RPC_CODE_STRINGNOTICE_NOTICE_NOTIFY, oNotify);
             }

         }
     }

     match_unit_list_type::iterator fi;

     match_unit_id ui = 0;
  
	 Team::team_id_type tId = 0;
	 auto iterteammem = m_vUnits.begin();
	 if (iterteammem != m_vUnits.end())
	 {
		 tId = iterteammem->second->GetTeamId();
	 }

     for (auto & it : m_vUnits)
     {
         match_unit_ptr_type pUnit = it.second;
         if (pUnit->HasPlayer(playerId))
         {
             ui = it.first;
             if (m_oRemoveMatchUnitCallBack)
             {
                 m_oRemoveMatchUnitCallBack(pUnit);
             }
         }
     }

     m_vUnits.erase(ui);

     if (m_oReMatchCallBack)
     {
         m_oReMatchCallBack(m_vUnits, m_nGroup, m_nRoomId);
     }
   
	 // 镜像的话 进入星耀计数操作
	 if (E_SCENE_MIRROR == m_oTarget.scenetype())
	 {
		 m_oMirrorMatchCallBack(m_oTarget.sceneconfigid(), m_oTarget.starbattleid(), MIRROR_MATCH_OUT, tId);
	 }

     RemoveRoom();
     return OR_OK;
 }

 void MatchRoom::OnEnterDungeon()
 {
#ifndef __TEST__
     if (!m_oEnterDungeonCallBack)
     {
         return;
     }
     CreateDungeonAsk msg;
     msg.set_targetsceneconfigid(m_oTarget.sceneconfigid());
     msg.set_dungeonconfigid(m_oTarget.dungeonconfigid());
     msg.set_type(m_oTarget.scenetype());
     msg.set_mirrorconfigid(m_oTarget.mirrorconfigid());
	 msg.set_starbattleid(m_oTarget.starbattleid());
     for (auto && it : m_vMatchInfoList)
     {
         msg.add_teamroleids(it.first);
     }
     m_oEnterDungeonCallBack(msg);
#endif//__TEST__
 }

 std::size_t MatchRoom::GetReadySize()
 {
     std::size_t rsize = 0;
     for (auto & it : m_vMatchInfoList)
     {
         if (it.second.ready() == E_STATUS_READY)
         {
             ++rsize; 
         }

     }
     return rsize;
 }

 void MatchRoom::TimeOut()
 {
     match_unit_list_type s;

     if (m_oSendCallBack)
     {
         StringNoticeRpcNoticeNotify oNotify;

         for (auto & it : m_vMatchInfoList)
         {
             if (it.second.ready() != E_STATUS_READY)
             {
                 oNotify.set_configid(E_NOTICE_TEAM_MEMBER_YOU_CANCCLEREADY_TIME_OUT);
             }
             else
             {
                 oNotify.set_configid(E_NOTICE_TEAM_MEMBER_CANCEL_READY_TIME_OUT);
             }

             m_oSendCallBack(it.first, ModuleStringNotice::RPC_CODE_STRINGNOTICE_NOTICE_NOTIFY, oNotify);

         }
     }
     

     for (auto & it : m_vMatchInfoList)
     {
         if (it.second.ready() == E_STATUS_READY)
         {
             continue;
         }

        

         for (auto & ji : m_vUnits)
         {
             if (ji.second->HasPlayer(it.second.roleid()))
             {
                 s.emplace(ji.first, ji.second);
                 break;
             }
         }
     }
     
	 Team::team_id_type tId = 0;
	 auto iterteammem = m_vUnits.begin();
	 if (iterteammem != m_vUnits.end())
	 {
		 tId = iterteammem->second->GetTeamId();
	 }

     for (auto & it : s)
     {
         if (m_oRemoveMatchUnitCallBack)
         {
             m_oRemoveMatchUnitCallBack(it.second);
             m_vUnits.erase(it.first);
         }
     }

     if (m_oReMatchCallBack)
     {
         m_oReMatchCallBack(m_vUnits, m_nGroup, m_nRoomId);
     }
     if (m_oSendCallBack)
     {
         MatchRpcRoomCancelNotify msg;
         msg.set_roomtype(m_nMatchType);
         for (auto && rId : m_vMatchInfoList)
         {
             m_oSendCallBack(rId.first, ModuleMatch::RPC_CODE_MATCH_ROOMCANCEL_NOTIFY, msg);
         }
     }
     
	 // 镜像的话 进入星耀计数操作
	 if (E_SCENE_MIRROR == m_oTarget.scenetype())
	 {
		 m_oMirrorMatchCallBack(m_oTarget.sceneconfigid(), m_oTarget.starbattleid(), MIRROR_MATCH_OUT, tId);
	 }

     RemoveRoom();
 }

 MatchRoom::room_guids_type MatchRoom::GetGuids()
 {
     room_guids_type t;
     for (auto & it : m_vCamp)
     {
         for (auto & ji : it)
         {
             for (auto & ki : ji)
             {
                 for (auto & mi : ki.second->GetPlayers())
                 {
                     t.emplace(mi.first);
                 }
             }
         }
     }
     return t;
 }

 MatchRoom::room_guids_type MatchRoom::GetTeamGuid(GUID_t p)
 {
     room_guids_type t;

     for (room_camps_list_type::iterator it = m_vCamp.begin(); it != m_vCamp.end(); ++it)
     {
         camp_mu_list_type & v = (*it);
         //camp group
         for (camp_mu_list_type::iterator ji = v.begin(); ji != v.end(); ++ji)
         {
             //group per unit
             match_unit_list_type & ts = (*ji);
             for (match_unit_list_type::iterator ki = ts.begin(); ki != ts.end(); ++ki)
             {

                 if (!ki->second->HasPlayer(p))
                 {
                     continue;
                 }
                 for (auto & mi : ki->second->GetPlayers())
                 {
                     t.emplace(mi.first);
                 }
                 
             }

         }
       
     }
     return t;
 }

 void MatchRoom::SetAllReady()
 {
     for (auto & it : m_vMatchInfoList)
     {
         it.second.set_ready(E_STATUS_READY);
     }
 }

 bool MatchRoom::IsAllReady()
 {
     bool bAllReady = true;
     for (auto & it : m_vMatchInfoList)
     {
         if (it.second.ready() == E_STATUS_NONE)
         {
             bAllReady = false;
             break;
         }
     }

     return bAllReady;
 }

 bool MatchRoom::HasRobot()
 {
	 for (auto & it : m_vMatchInfoList)
	 {
		 if (it.second.robot())
		 {
			 return true;
		 }
	 }
	 return false;
 }

 int32_t MatchRoom::SetNone(GUID_t playerId)
 {

     matchinfo_pb_list::iterator it = m_vMatchInfoList.find(playerId);
     if (it == m_vMatchInfoList.end())
     {
         return OR_MATCH_NO_IN_ROOM;
     }
     it->second.set_ready(E_STATUS_NONE);

     return OR_OK;
 }

 GUID_t MatchRoom::RandomGuid()
 {
#ifdef __TEST__

     std::random_device rd;
     std::mt19937 gen(rd());

     std::uniform_int_distribution<std::size_t> dis(0, m_vMatchInfoList.size() - 1);
     std::size_t pos =  dis(gen);


     std::vector<GUID_t> gs;

     for (auto & it : m_vMatchInfoList)
     {
         gs.push_back(it.first);
     }

  
     return gs[pos];

#endif // __TEST__

     return 0;
 }

 void MatchRoom::NotifyRoomInfo(GUID_t guid)
 {
     MatchRpcPrepareRoomNotify n;
     ToClientPb(*n.mutable_readyroom());
     m_oSendCallBack(guid, ModuleMatch::RPC_CODE_MATCH_PREPAREROOM_NOTIFY, n);
 }

}//namespace GameMMR