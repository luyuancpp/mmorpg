#include "Obj/Obj/Obj.h"
#include "Obj/Obj_Monster/Obj_Monster.h"
#include <memory>

#include <iostream>


#include "DK_TimeManager.h"
#include "DK_Log.h"
#include "ActionMoudle/ActionDelegator_T.h"
#include "include/PacketMgr.h"
#include "Config.h"
#include "Skill/Skill_Core_T.h"
#include "Skill/BuffLogic.h"
#include "Scene/WayPoint/WayPointManager.h"

#include "Obj/Obj_Human/Obj_HumanSceneModule.h"
#include "Obj/Obj_Human/Obj_Human.h"
#include "Scene/WayPoint/Wonder.h"
using namespace std;



//
//Obj::msg_ptr		Obj::CreateDeleteObjPacket(void)
//{
//	
//	return NULL;
//}
//
//void  Obj::OnEnterScene(class Scene *)
//{
//
//}
//void Obj::OnLeaveScene(void)
//{
//
//}
//
//Obj_Character::Obj_Character(void)
//:m_oRefreshAttrModule(this),
// m_ActionParams(this)
//{
//
//}
//
//Obj_Character::~Obj_Character(void)
//{
//
//}
//
//bool Obj_Character::HeartBeat(unsigned int)
//{
//	return true;
//}
//bool  Obj_Character::HeartBeat_OutZone(unsigned int)
//{
//	return true;
//}

//std::shared_ptr<DK_TimeManager> g_pTimeManager(new  DK_TimeManager);

void  DK_Log::SaveLog(char *, char *, ...){}
void  DK_Log::FastSaveLog(char* msg, ...){}

DK_Log::~DK_Log(){}


SkillInfo_T::SkillInfo_T(void){}

SkillInfo_T::~SkillInfo_T(void){}

void  SkillInfo_T::CleanUp(void){}
ActionParams_T::ActionParams_T(class Obj_Character *){}

ActionParams_T::~ActionParams_T(void){}
void  ActionParams_T::Reset(void){}

SkillTargetingParams_T::SkillTargetingParams_T(void){}
SkillTargetingParams_T::~SkillTargetingParams_T(void){}
void  SkillTargetingParams_T::Reset(void){}
StateMgr::StateMgr(void){}

StateMgr::~StateMgr(void){}
void  StateMgr::CleanUp(void){}
bool  StateMgr::Logic(class Obj_Character *, unsigned int){ return true; }

bool  StateMgr::ChangeState(class Obj_Character *, enum ENUM_STATE){ return true; }


bool DK_Session::disConnect()
{
	cout << "DK_Session" << endl;
	return true;
}



CConfig g_config;



EnemyListManager::EnemyListManager(void){}
EnemyListManager::~EnemyListManager(void){}
bool  EnemyListManager::Init(void){ return true; }
void  EnemyListManager::CleanUP(void){}
void  EnemyListManager::HeartBeart(class Obj_Character *, unsigned int){}
ObjBuffModule::ObjBuffModule(void){}

ObjBuffModule::~ObjBuffModule(void){}
bool  ObjBuffModule::Init(class Obj_Character *){
	return true;
}

void  ObjBuffModule::CleanUp(void){}
bool  ObjBuffModule::HeartBeat(unsigned int){ return true; }

unsigned int  EnemyListManager::GetTargetObjID(void){
	return 1;
}
void  EnemyListManager::PopTargetObj(void)
{}

OPERATE_RESULT  Skill_Core_T::ProcessSkillRequest(class Obj_Character &, unsigned short, unsigned char, unsigned int, struct WORLD_POS const &, float, unsigned __int64)
{
	return OR_OK;
}

bool  ObjBuffModule::Buff_RegisterBuff(class OWN_BUFF &)
{
	return false;
}
BuffLogic_T *  BuffLogic_Manager::Buff_GetLogic(class OWN_BUFF &)
{
	return NULL;
}

Skill_Core_T::Skill_Core_T(void){}
Skill_Core_T::~Skill_Core_T(void){}
BuffLogic_Manager::BuffLogic_Manager(void){}
BuffLogic_Manager::~BuffLogic_Manager(void){}
CConfig::CConfig(void){}
CConfig::~CConfig(void){}

const WayPointElement* WayPointManager::GetWayPoint(unsigned int)
{
	return NULL;
}
bool WayPointManager::InitWayPoint(void)
{
	return true;
}


void Logger::Log(char const *, char const *, int, char, char const *, ...){}


void  EnemyListManager::AddEnemyInfo(unsigned int, int){}

void  Obj_Character::HealthIncrement(int, unsigned short, class Obj_Character *, bool)
{

}

void  ObjBuffModule::OnDBLoad(Own_BuffDBList const &)
{

}
void ObjBuffModule::OnSave(Own_BuffDBList *)
{

}

void  Obj_Human::SendOperate_Result(enum OPERATE_RESULT)
{

}


WORLD_POS WonderInRange::GetRandPosByRange(struct WORLD_POS const &, struct WORLD_POS const &, float)
{
	return WORLD_POS();
}

OPERATE_RESULT Skill_Core_T::CanUseSkill(class Obj_Character &, unsigned short, unsigned char)
{
	return OR_OK;
}



int  HumanSceneModule::ReturnLastScene(void){ return 0; }

Logger::Logger(void)
{

}
Thread_Base::Thread_Base(void)
{

}

Thread_Base::~Thread_Base(void)
{

}
Logger::~Logger(void){}
void  Logger::Loop(void)
{

}

void  HumanSceneModule::SetTargetSceneInfo(int, float, float)
{

}

int  HumanSceneModule::ResetPrimeSceneDB(int)
{
	return 0;
}

IScriptHost::IScriptHost(void)
{

}
IScriptHost::~IScriptHost(void)
{

}

struct ILuaScript *  ScriptFactory::Create(void)
{
	return NULL;
}

ThreadLuaScript::ThreadLuaScript(void)
{

}

ThreadLuaScript::~ThreadLuaScript(void)
{

}

void  ThreadLuaScript::OnThreadStart(void)
{

}

MapScriptGroup::~MapScriptGroup(void)
{

}