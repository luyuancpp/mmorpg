#ifndef SHAPE_H
#define SHAPE_H

#include <memory>

class CChildView;

class Shape
{
public:
	typedef std::shared_ptr<Shape> shapepointer_type;

	enum EnumShapeType
	{
		E_CIRCLE,
		E_CIRCLE_SECTOR,
		E_CIRCLE_SECTOR1,
		E_MAX
	};

	virtual void Draw(CChildView * pView){}

	static shapepointer_type Create(int32_t nType);

private:

};

#endif // !SHAPE_H
