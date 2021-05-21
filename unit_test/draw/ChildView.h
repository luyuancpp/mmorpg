
// ChildView.h : CChildView ��Ľӿ�
//


#pragma once


#include <random>
#include <unordered_map>

#include "Shape.h"
// CChildView ����

class CChildView : public CWnd
{
// ����
public:
	
	CChildView();

// ����
public:

// ����
public:

// ��д
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// ʵ��
public:
	virtual ~CChildView();

	// ���ɵ���Ϣӳ�亯��
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

