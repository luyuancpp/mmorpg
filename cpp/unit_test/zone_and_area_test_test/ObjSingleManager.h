// ObjSingleManager.h
// 
// 功能：单一类型的Obj对象的管理
//////////////////////////////////////////////////////

#ifndef __OBJSINGLEMANAGER_H__
#define __OBJSINGLEMANAGER_H__
#include "Util.h"

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
	ObjSingleManager(  );
	virtual ~ObjSingleManager(  );

	bool Init(const _OBJSINGLEMANAGER_INIT *pInit);
	
	void Term(  );

	void SetLoadFactor( float fFactor );
	bool HeartBeat( UINT uTime = 0 );

	bool Add( Obj *pObj );
	bool Remove( Obj *pObj );

	UINT GetCount( void )const
	{
		return m_uCount;
	}

	UINT GetLength( void )const
	{
		return m_uLength;
	}

	void SetScene( Scene* pScene )
	{
		m_pScene = pScene;
	}

	Scene *GetScene( void )
	{
		return m_pScene;
	}

	Obj *GetObjByIndex( UINT uIndex )
	{
		if (uIndex < m_uCount )
		{
			return m_papObjs[uIndex];
		}
		else
		{
			return NULL;
		}
	}

private:
	bool Resize( UINT uSize );

protected:
	Obj				**m_papObjs;
	uint32_t			m_uCount;
	UINT			m_uLength;
	UINT			m_uLogicInterval;			// 正常状态下每两个逻辑帧之间的时间间隔
	UINT			m_uCurrentLogicInterval;	// 当前的每两个逻辑帧之间的时间间隔

	Scene			*m_pScene;
	CMyTimer		m_LogicTimer;
};

#endif	// __OBJSINGLEMANAGER_H__
