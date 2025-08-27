
// ChildView.h : CChildView 类的接口
//


#pragma once


#include <random>
#include <unordered_map>

#include "Shape.h"
// CChildView 窗口

class CChildView : public CWnd
{
// 构造
public:
	
	CChildView();

// 特性
public:

// 操作
public:

// 重写
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// 实现
public:
	virtual ~CChildView();

	// 生成的消息映射函数
protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void On32771();
	std::mt19937 gen;
	std::random_device rd;
	afx_msg void OnCircleSector();

private:
	std::unordered_map<int32_t, Shape::shapepointer_type> m_mShape;
	int32_t m_nCurShapeType;
public:
	afx_msg void OnCircleSector1();
};

