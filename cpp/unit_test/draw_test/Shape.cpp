#include "stdafx.h"
#include "Shape.h"
#include "ChildView.h"
#include "Detour/Include/DetourCommon.h"

#include <array>
#include <random>

typedef std::array<float, 3> Pos3;

#define PI 3.14159265f

static float GetCos(float degrees)
{
	float c = (float)std::cos(degrees * PI / 180);

	if (std::abs(0 - c) < 0.000001)
	{
		c = 0;
	}
	return c;
}

static float GetSin(float degrees)
{
	float s = (float)std::sin(degrees * PI / 180);

	if (std::abs(0 - s) < 0.000001)
	{
		s = 0;
	}
	return s;
}





void GeneratePointInCircle(float fCenterX, float fCenterZ, float radius, float & fPointX, float & fPointZ)
{
	//https://stackoverflow.com/questions/5837572/generate-a-random-point-within-a-circle-uniformly
	static std::default_random_engine ge;
	std::uniform_real_distribution<float> ad(0, 360);

	std::uniform_real_distribution<float> rd(0, radius);

	float u = rd(ge) + rd(ge);
	float t = ad(ge);
	float rr = u > radius ? (2 * radius - u) : radius;
	fPointX = fCenterX + rr * GetCos(t);
	fPointZ = fCenterZ + rr * GetSin(t);

}

bool PointInCircle(float fCenterX, float fCenterZ, float radius, float & fPointX, float & fPointZ)
{
    return (dtSqr(fPointX - fCenterX) + dtSqr(fPointZ - fCenterZ)) <= dtSqr(radius);
	return (dtSqr(fPointX - fCenterX) + dtSqr(fPointZ - fCenterZ)) <= dtSqr(radius);
}



class  Circle : public Shape
{
public:
	virtual void Draw(CChildView * pView)
	{
		CPaintDC dc(pView);
		CPoint center(300, 300);
		float r = 100;

	
		for (int i = 0; i < 100000; ++i)
		{
			float x = 0.0f;
			float y = 0.0f;
			GeneratePointInCircle(center.x, center.y, r, x, y);
			COLORREF colorrrefRGB = RGB(0, 255, 0);
			dc.SetPixel(CPoint(x, y), colorrrefRGB);

			if (!PointInCircle(center.x, center.y, r, x, y))
			{
				dc.SetPixel(CPoint(x, y), RGB(255, 0, 0));
			}
		}

		for (float x = 100; x < 500; x++)
		{
			for (float y = 100; y < 500; y++)
			{
				if (!PointInCircle(center.x, center.y, r, x, y))
				{
					dc.SetPixel(CPoint(x, y), RGB(255, 0, 0));
				}
			}
		}

		dc.LineTo(200, 200);
		dc.LineTo(200, 400);
		dc.LineTo(400, 400);
		dc.LineTo(400, 200);
		dc.LineTo(200, 200);
	}

private:

};
namespace Sector
{
	Pos3 GetTargetPos(float fAngle, float fDistance)
	{
		Pos3 ret;
		ret[0] = GetCos(fAngle) * fDistance;
		ret[2] = GetSin(fAngle) * fDistance;
		return ret;
	}

	//https://stackoverflow.com/questions/13652518/efficiently-find-points-inside-a-circle-sector
	bool areClockwise(Pos3 & v1, Pos3 & v2) {
		return -v1[0] * v2[2] + v1[2] * v2[0] > 0;
	}

	bool isWithinRadius(Pos3 & v, float radiusSquared) {
		return v[0] * v[0] + v[2] * v[2] <= radiusSquared;
	}

	bool isInsideSector(Pos3 & point, Pos3 & center, Pos3 & sectorStart, Pos3 & sectorEnd, float radius) {
		Pos3 relPoint;
		relPoint[0] = point[0] - center[0];
		relPoint[2] = point[2] - center[2];

		return !areClockwise(sectorStart, relPoint) &&
			areClockwise(sectorEnd, relPoint) &&
			PointInCircle(0, 0, radius, relPoint[0], relPoint[2]);
	}

	
	bool isInsideSectorB(Pos3 & point, Pos3 & center, Pos3 & sectorStart, Pos3 & sectorEnd, float radius) {
		Pos3 relPoint;
		relPoint[0] = point[0] - center[0];
		relPoint[2] = point[2] - center[2];
		return !areClockwise(sectorStart, relPoint) &&
			areClockwise(sectorEnd, relPoint) &&
			isWithinRadius(relPoint, radius*radius);
	}

	
}


bool isInsideSector(float mydir, float fSectorTheta, float fSectorRedius, Pos3 & point, Pos3 & center)
{
	Pos3 sectorStart = Sector::GetTargetPos(mydir - fSectorTheta, fSectorRedius);
	Pos3 sectorEnd = Sector::GetTargetPos(mydir + fSectorTheta, fSectorRedius);
	return Sector::isInsideSector(point, center, sectorStart, sectorEnd, fSectorRedius);
}


class  CircleSector : public Shape
{
public:
	virtual void Draw(CChildView * pView)
	{
		CPaintDC dc(pView);
	
		static std::default_random_engine ge;
		std::uniform_int_distribution<int32_t> ax(400, 800);
		std::uniform_int_distribution<int32_t> ay(0, 100);

		CPoint center(ax(ge), ay(ge));



		//dc.LineTo(100, 100);
		
		float r = 400;
		

		Pos3  centerp;
		centerp[0] = center.x;
		centerp[2] = center.y;

		std::uniform_real_distribution<float> amin(0, 60);
		std::uniform_real_distribution<float> amax(61, 179);

		Pos3 sectorStart = Sector::GetTargetPos(amin(ge), r);
	
		Pos3 sectorEnd = Sector::GetTargetPos(amax(ge), r);
		
		for (int x = 0; x < 1500; ++x)
		{
			for (int y = 0; y < 1500; ++y)
			{
					
				Pos3  point;
				point[0] = x;
				point[2] = y;

				
				if (Sector::isInsideSectorB(point, centerp, sectorStart, sectorEnd, r))
				{
					COLORREF colorrrefRGB = RGB(0, 255, 0);
					dc.SetPixel(CPoint(x, y), colorrrefRGB);
					
				}
				else
				{
					//dc.SetPixel(CPoint(x, y), RGB(255, 0, 0));
				}
			}
	
		}

		sectorStart[0] += centerp[0];
		sectorStart[2] += centerp[2];

		sectorEnd[0] += centerp[0];
		sectorEnd[2] += centerp[2];

		dc.LineTo(centerp[0], centerp[2]);
		dc.LineTo(sectorStart[0], sectorStart[2]);
		dc.LineTo(sectorEnd[0], sectorEnd[2]);
		dc.LineTo(centerp[0], centerp[2]);
	}



	


private:

};



class  CircleSector1 : public Shape
{
public:


	virtual void Draw(CChildView * pView)
	{
		CPaintDC dc(pView);

		static std::default_random_engine ge;
		std::uniform_int_distribution<int32_t> ax(400, 600);
		std::uniform_int_distribution<int32_t> ay(400, 600);

		CPoint center(ax(ge), ay(ge));



		//dc.LineTo(100, 100);

		float r = 400;


		Pos3  centerp;
		centerp[0] = center.x;
		centerp[2] = center.y;

		std::uniform_real_distribution<float> theta(0, 89);
		float sectorAngle = theta(ge);

		for (int x = 0; x < 1500; ++x)
		{
			for (int y = 0; y < 1500; ++y)
			{

				Pos3  point;
				point[0] = x;
				point[2] = y;
				



				if (isInsideSector(45, sectorAngle, r, point, centerp))
				{
					COLORREF colorrrefRGB = RGB(0, 255, 0);
					dc.SetPixel(CPoint(x, y), colorrrefRGB);

				}
				else
				{
					//dc.SetPixel(CPoint(x, y), RGB(255, 0, 0));
				}
			}

		}
	}






private:

};

Shape::shapepointer_type Shape::Create(int32_t nType)
{


	switch (nType)
	{
	case E_CIRCLE:
	{
					 return shapepointer_type(new Circle);
	}
	break;
	case E_CIRCLE_SECTOR:
	{
					 return shapepointer_type(new CircleSector);
	}
		break;
	case E_CIRCLE_SECTOR1:
	{
							return shapepointer_type(new CircleSector1);
	}
		break;
	default:
		break;
	}
	return shapepointer_type(new Circle);
}