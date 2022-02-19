
// ChildView.cpp : CChildView 类的实现
//

#include "stdafx.h"
#include "draw.h"
#include "ChildView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <cmath>
#include <random>

// CChildView

CChildView::CChildView()
:	rd(),
	gen(rd()),
	m_nCurShapeType(Shape::E_CIRCLE_SECTOR1)
{
	for (int32_t i = 0; i < Shape::E_MAX; ++i)
	{
		m_mShape.emplace(i, Shape::Create(i));
	}
}

CChildView::~CChildView()
{
}


BEGIN_MESSAGE_MAP(CChildView, CWnd)
	ON_WM_PAINT()
	ON_COMMAND(ID_32771, &CChildView::On32771)
	ON_COMMAND(ID_32773, &CChildView::OnCircleSector)
	ON_COMMAND(ID_32774, &CChildView::OnCircleSector1)
END_MESSAGE_MAP()



// CChildView 消息处理程序

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_WINDOW+1), NULL);

	return TRUE;
}


void CChildView::OnPaint() 
{
	//CPaintDC dc(this); // 用于绘制的设备上下文
	
	// TODO:  在此处添加消息处理程序代码
	
	// 不要为绘制消息而调用 CWnd::OnPaint()

	
	std::unordered_map<int32_t, Shape::shapepointer_type>::iterator it = m_mShape.find(m_nCurShapeType);
	it->second->Draw(this);	

}



void CChildView::On32771()
{
	
	m_nCurShapeType = Shape::E_CIRCLE;
	Invalidate();
}


void CChildView::OnCircleSector()
{
	m_nCurShapeType = Shape::E_CIRCLE_SECTOR;
	Invalidate();
}


void CChildView::OnCircleSector1()
{
	m_nCurShapeType = Shape::E_CIRCLE_SECTOR1;
	Invalidate();
}
