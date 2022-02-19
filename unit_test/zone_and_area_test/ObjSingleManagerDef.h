// ObjSingleManager.h
// 
// 功能：单一类型的Obj对象的管理
//////////////////////////////////////////////////////

#pragma once
#include "Util.h"

#include <unordered_map>

#include "Obj/Obj.h"

using namespace std;

class Scene;
class Obj;

struct _OBJSINGLEMANAGER_INIT
{
	UINT	m_uLength;
	Scene	*m_pScene;
	UINT	m_uLogicInterval;		// 正常状态下每两个逻辑帧之间的时间间隔
	UINT	m_uCurrentTime;		// 当前时间，初始ObjSingleManager::m_LogicTimer的参数
	_OBJSINGLEMANAGER_INIT(  )
	{
		m_uLength			= 0;
		m_pScene			= NULL;
		m_uLogicInterval	= 0;
		m_uCurrentTime		= 0;
	}

	void Cleanup(  )
	{
		m_uLength			= 0;
		m_pScene			= NULL;
		m_uLogicInterval	= 0;
		m_uCurrentTime		= 0;
	}
};

class Scene;

class ObjSingleManager
{
public:
	typedef std::unordered_map<ObjID_t, Obj *> obj_type_list;

	ObjSingleManager()
	{}

	~ObjSingleManager()
	{}

	bool Init(const _OBJSINGLEMANAGER_INIT *pInit)
	{
		m_pScene = pInit->m_pScene;

		return true;
	}
	
	void Term(  );

	void SetLoadFactor( float fFactor );
	bool HeartBeat( UINT uTime = 0 );

	bool Add(Obj *pObj)
	{
		m_papObjs.emplace(pObj->GetObjID(), pObj);

		return true;
	}

	bool Remove(Obj *pObj)
	{
		m_papObjs.erase(pObj->GetObjID());

		return true;
	}

	UINT GetCount( void )const
	{
		return m_papObjs.size();
	}


	void SetScene( Scene* pScene )
	{
		m_pScene = pScene;
	}

	Scene *GetScene( void )
	{
		return m_pScene;
	}

	Obj *GetObjByIndex(ObjID_t uIndex)
	{
		obj_type_list::iterator it = m_papObjs.find(uIndex);
		if (it != m_papObjs.end())
		{
			return it->second;
		}
		return NULL;
	}

private:


protected:
	obj_type_list				m_papObjs;
	uint32_t			m_uCount;
	UINT			m_uLength;
	UINT			m_uLogicInterval;			// 正常状态下每两个逻辑帧之间的时间间隔
	UINT			m_uCurrentLogicInterval;	// 当前的每两个逻辑帧之间的时间间隔

	Scene			*m_pScene;
	CMyTimer		m_LogicTimer;
};


