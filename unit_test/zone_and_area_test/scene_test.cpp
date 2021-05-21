#include <gtest/gtest.h>

#include "../testdef.h"

#include <algorithm>
#include <thread>


#include "Scene/ZoneManager.h"

#include "Scene/Area/Area.h"
#include "Scene/Area/SceneAreaManager.h"
#include "GenCode/Config/ConfigModule.h"
#include "GenCode/Config/EventAreaCfg.h"
#include "Obj/Obj_Human/Obj_AreaModule.h"

#ifdef __TEST_HUMAN__
class Obj_Human;
#endif//__TEST_HUMAN__

TEST(SceneManagerTest, ConvexPolygonTArea)
{
    EventAreaElement oEventAreaElement;
    oEventAreaElement.switchType = EventArea::Area::E_AREA_OPENED;
    Vector3 pos;

	pos.set_x(1);
	pos.set_z(0);
    EventArea::Area::verts_type area_v;
	area_v.push_back(pos);

	pos.set_x(2);
	pos.set_z(0);
	area_v.push_back(pos);

	pos.set_x(3);
	pos.set_z(1);
	area_v.push_back(pos);

	pos.set_x(3);
	pos.set_z(2);
	area_v.push_back(pos);

	pos.set_x(2);
	pos.set_z(3);
	area_v.push_back(pos);

	pos.set_x(1);
	pos.set_z(3);
	area_v.push_back(pos);

	pos.set_x(0);
	pos.set_z(2);
	area_v.push_back(pos);

	pos.set_x(0);
	pos.set_z(1);
	area_v.push_back(pos);


    EventArea::ConvexPolygon oConvexPolygon(&oEventAreaElement);
	oConvexPolygon.Init(area_v);

    pos.set_x(1);
	pos.set_z(1);
	EXPECT_TRUE(oConvexPolygon.InArea(pos));

    pos.set_x(0);
	pos.set_z(0);
	EXPECT_FALSE(oConvexPolygon.InArea(pos));

    pos.set_x(0.5);
	pos.set_z(0.5);
	EXPECT_TRUE(oConvexPolygon.InArea(pos));

    pos.set_x(0);
	pos.set_z(1);
	EXPECT_TRUE(oConvexPolygon.InArea(pos));

    pos.set_x(0);
	pos.set_z(1.5);
	EXPECT_TRUE(oConvexPolygon.InArea(pos));

    pos.set_x(0);
	pos.set_z(2);
	EXPECT_TRUE(oConvexPolygon.InArea(pos));

	pos.set_x(0);
	pos.set_z(3);
	EXPECT_FALSE(oConvexPolygon.InArea(pos));

	
	pos.set_x(1.5);
	pos.set_z(3);
	EXPECT_FALSE(oConvexPolygon.InArea(pos));

	pos.set_x(1.5);
	pos.set_z(0);
	EXPECT_TRUE(oConvexPolygon.InArea(pos));

	pos.set_x(7);
	pos.set_z(7);
	EXPECT_FALSE(oConvexPolygon.InArea(pos));

   

}

TEST(SceneManagerTest, CircleTArea)
{
    EventAreaElement oEventAreaElement;
    oEventAreaElement.switchType = EventArea::Area::E_AREA_OPENED;
    EventArea::Circle oCircle(&oEventAreaElement);
   

    Vector3 oCenter;
    int32_t nRadius = 100;
    oCenter.set_x(0);
    oCenter.set_y(0);
    oCenter.set_z(0);
    oCircle.Init(oCenter, nRadius);

    for (int32_t i = 0; i < nRadius; ++i)
    {
        for (int32_t j = 0; j < nRadius; ++j)
        {
            Vector3 pos;
            pos.set_x((float)i);
            pos.set_z((float)j);
            if (i * i + j * j <= nRadius * nRadius)
            {
                bool ret = oCircle.InArea(pos);
                EXPECT_TRUE(ret);
            }
            else
            {
                EXPECT_FALSE(oCircle.InArea(pos));
            }
        }
    }
}

TEST(SceneManagerTest, SceneAreaManger)
{
    EventArea::SceneAreaManager oSceneAreaManager;
    EventArea::SceneAreaManager::area_ptr ptr = oSceneAreaManager.AddArea(EventArea::Area::E_Circle, EventAreaTable::Instance().GetElement(1));
    EXPECT_EQ(ptr->GetShapType(), EventArea::Area::E_Circle);
    ptr = oSceneAreaManager.AddArea(EventArea::Area::E_ConvexPolygon,  EventAreaTable::Instance().GetElement(2));
    EXPECT_EQ(ptr->GetShapType(), EventArea::Area::E_ConvexPolygon);
}

TEST(SceneManagerTest, TestZoneArea)
{
    ZoneManager oZoneManager;
    EventAreaElement oEventAreaElement;
    oEventAreaElement.switchType = EventArea::Area::E_AREA_OPENED;

    EventArea::SceneAreaManager::area_ptr ptr = oZoneManager.GetAreaManager().AddArea(EventArea::Area::E_ConvexPolygon, EventAreaTable::Instance().GetElement(1));

   

    EventArea::Area::verts_type area_v;

    Vector3 pos;
    pos.set_x(0);
    pos.set_z(0);
    area_v.push_back(pos);

    pos.set_x(0);
    pos.set_z(10);
    area_v.push_back(pos);

    pos.set_x(10);
    pos.set_z(10);
    area_v.push_back(pos);

    pos.set_x(10);
    pos.set_z(0);
    area_v.push_back(pos);
    ptr->Init(area_v);

    ptr = oZoneManager.GetAreaManager().AddArea(EventArea::Area::E_ConvexPolygon, EventAreaTable::Instance().GetElement(2));
    area_v.clear();

    pos.set_x(90);
    pos.set_z(90);
    area_v.push_back(pos);

    pos.set_x(100);
    pos.set_z(90);
    area_v.push_back(pos);

    pos.set_x(100);
    pos.set_z(100);
    area_v.push_back(pos);

    pos.set_x(90);
    pos.set_z(100);
    area_v.push_back(pos);
    ptr->Init(area_v);

    

    ptr = oZoneManager.GetAreaManager().AddArea(EventArea::Area::E_Circle, EventAreaTable::Instance().GetElement(3));
    float nRadius = 10;
    Vector3 oCenter;
    oCenter.set_x(10);
    oCenter.set_y(10);
    oCenter.set_z(10);
    ptr->Init(oCenter, nRadius);

    oZoneManager.Init(100, 100, 10);
    
    EXPECT_EQ(2, oZoneManager.GetZoneForTest(0).GetAreaSize());
    for (int32_t i = 0 ; i < 100; ++i)
    {
        if (i == 0 || i == 1 || i == 10 || i == 11)
        {
            bool ret = oZoneManager.GetZoneForTest(i).HasArea(1);

            EXPECT_TRUE(ret);
        }
        else
        {
            bool ret = oZoneManager.GetZoneForTest(i).HasArea(1);

            EXPECT_FALSE(ret);
        }
        

        
    }
    EXPECT_TRUE(1, oZoneManager.GetZoneForTest(99).GetAreaSize());
    EXPECT_TRUE(oZoneManager.GetZoneForTest(99).HasArea(2));
    /*std::cout.width(3);
     for (int32_t i = 0; i < 100; ++i)
     {
         if (i % 10 == 0 )
         {
             std::cout << std::endl;
         }

         std::cout  << setw(4) <<  i << "  ";
     }
     std::cout << std::endl;*/

    for (int32_t i = 0; i < 100; ++i)
    {
        if (i == 88 || i == 89 || i == 98 || i == 99)
        {
            bool ret = oZoneManager.GetZoneForTest(i).HasArea(2);
         
            EXPECT_TRUE(ret);
        }
        else
        {
            bool ret = oZoneManager.GetZoneForTest(i).HasArea(2);
            EXPECT_FALSE(ret);
        }
        
    }

    for (int32_t i = 0; i < 100; ++i)
    {
        if (   i == 0 || i == 1 || i == 2 
            || i == 10 || i == 11 || i == 12
            || i == 20 || i == 21 || i == 22)
        {
            bool ret = oZoneManager.GetZoneForTest(i).HasArea(3);

            EXPECT_TRUE(ret);
        }
        else
        {
            bool ret = oZoneManager.GetZoneForTest(i).HasArea(3);
            EXPECT_FALSE(ret);
        }

    }
}

TEST(SceneManagerTest, TestZonePosition)
{
    ZoneManager oZoneManager;
    oZoneManager.Init(100, 100, 10);


    EXPECT_EQ(0, oZoneManager.GetZoneForTest(0).YMin());
    EXPECT_EQ(10, oZoneManager.GetZoneForTest(0).YMax());
    EXPECT_EQ(0, oZoneManager.GetZoneForTest(0).XMin());
    EXPECT_EQ(10, oZoneManager.GetZoneForTest(0).XMax());

    EXPECT_EQ(0, oZoneManager.GetZoneForTest(9).YMin());
    EXPECT_EQ(10, oZoneManager.GetZoneForTest(9).YMax());
    EXPECT_EQ(90, oZoneManager.GetZoneForTest(9).XMin());
    EXPECT_EQ(100, oZoneManager.GetZoneForTest(9).XMax());

    EXPECT_EQ(90, oZoneManager.GetZoneForTest(90).YMin());
    EXPECT_EQ(100, oZoneManager.GetZoneForTest(90).YMax());
    EXPECT_EQ(0, oZoneManager.GetZoneForTest(90).XMin());
    EXPECT_EQ(10, oZoneManager.GetZoneForTest(90).XMax());

    EXPECT_EQ(90, oZoneManager.GetZoneForTest(99).YMin());
    EXPECT_EQ(100, oZoneManager.GetZoneForTest(99).YMax());
    EXPECT_EQ(90, oZoneManager.GetZoneForTest(99).XMin());
    EXPECT_EQ(100, oZoneManager.GetZoneForTest(99).XMax());
}

void InitZoneManager(ZoneManager & zm)
{
    EventAreaElement oEventAreaElement;
    oEventAreaElement.switchType = EventArea::Area::E_AREA_OPENED;
    EventArea::SceneAreaManager::area_ptr ptr = zm.GetAreaManager().AddArea(EventArea::Area::E_ConvexPolygon, EventAreaTable::Instance().GetElement(1));



    EventArea::Area::verts_type area_v;

    Vector3 pos;
    pos.set_x(0);
    pos.set_z(0);
    area_v.push_back(pos);

    pos.set_x(0);
    pos.set_z(10);
    area_v.push_back(pos);

    pos.set_x(10);
    pos.set_z(10);
    area_v.push_back(pos);

    pos.set_x(10);
    pos.set_z(0);
    area_v.push_back(pos);
    ptr->Init(area_v);

    ptr = zm.GetAreaManager().AddArea(EventArea::Area::E_ConvexPolygon, EventAreaTable::Instance().GetElement(2));
    area_v.clear();

    pos.set_x(90);
    pos.set_z(90);
    area_v.push_back(pos);

    pos.set_x(100);
    pos.set_z(90);
    area_v.push_back(pos);

    pos.set_x(100);
    pos.set_z(100);
    area_v.push_back(pos);

    pos.set_x(90);
    pos.set_z(100);
    area_v.push_back(pos);
    ptr->Init(area_v);



    ptr = zm.GetAreaManager().AddArea(EventArea::Area::E_Circle, EventAreaTable::Instance().GetElement(3));
    float nRadius = 10;
    Vector3 oCenter;
    oCenter.set_x(10);
    oCenter.set_y(10);
    oCenter.set_z(10);
    ptr->Init(oCenter, nRadius);
}

TEST(SceneManagerTest, TestObjArea)
{
    ZoneManager zm;
    InitZoneManager(zm);

    Obj_Human oHuman;
    ObjHumanAreaModule o(&oHuman);

    
    zm.Init(100, 100, 10);
    o.SetZm(&zm);

    Vector3 v;
    v.set_x(9);
    v.set_z(9);


    o.SetPos(v);
    o.SetZone(zm.GetZone(0));

    o.UpdateEvetArea();
    o.UpdateEvetArea();
    EXPECT_EQ(2, o.GetAreaSize());
    EXPECT_TRUE(o.InArea(1));
    EXPECT_TRUE(o.InArea(3));
    EXPECT_FALSE(o.InArea(2));
    EXPECT_FALSE(o.InArea(4));

    v.set_x(30);
    v.set_z(0);
    o.SetZone(zm.GetZone(4));
    o.SetPos(v);
    o.UpdateEvetArea();
    EXPECT_FALSE(o.InArea(1));
    EXPECT_FALSE(o.InArea(2));
    EXPECT_FALSE(o.InArea(3));
    EXPECT_FALSE(o.InArea(4));

    v.set_x(10);
    v.set_z(10);
    o.SetZone(zm.GetZone(1));
    o.SetPos(v);
    o.UpdateEvetArea();

    EXPECT_FALSE(o.InArea(1));
    EXPECT_FALSE(o.InArea(2));
    EXPECT_TRUE(o.InArea(3));
    EXPECT_FALSE(o.InArea(4));

    v.set_x(20);
    v.set_z(20);
    o.SetZone(zm.GetZone(2));
    o.SetPos(v);
    o.UpdateEvetArea();

    EXPECT_FALSE(o.InArea(1));
    EXPECT_FALSE(o.InArea(2));
    EXPECT_FALSE(o.InArea(3));
    EXPECT_FALSE(o.InArea(4));
}

TEST(SceneManagerTest, LeaveCurrentScene)
{
    ZoneManager zm;
    InitZoneManager(zm);

    Obj_Human oHuman;
    ObjHumanAreaModule o(&oHuman);
    zm.Init(100, 100, 10);
    o.SetZm(&zm);
    Vector3 v;
    v.set_x(9);
    v.set_z(9);


    o.SetPos(v);
    o.SetZone(zm.GetZone(0));

    o.UpdateEvetArea();
    EXPECT_TRUE(o.InArea(1));
    EXPECT_TRUE(o.InArea(3));
    o.OnLeaveScene();
    EXPECT_FALSE(o.InArea(1));
    EXPECT_FALSE(o.InArea(3));
}

TEST(SceneManagerTest, CheckAreaOpen)
{
    ZoneManager zm;
    InitZoneManager(zm);

    Obj_Human oHuman;
    ObjHumanAreaModule o(&oHuman);
    zm.Init(100, 100, 10);
    o.SetZm(&zm);
    Vector3 v;
    v.set_x(9);
    v.set_z(9);


    o.SetPos(v);
    o.SetZone(zm.GetZone(0));
    zm.GetAreaManager().CloseArea(1);
    zm.GetAreaManager().CloseArea(3);
    o.UpdateEvetArea();
    EXPECT_FALSE(o.InArea(1));
    EXPECT_FALSE(o.InArea(3));
    
}

TEST(SceneManagerTest, DungeonEventType)
{
    ZoneManager oZoneManager;
    EventAreaElement oEventAreaElement;
    oEventAreaElement.switchType = EventArea::Area::E_AREA_OPENED;

    EventArea::SceneAreaManager::area_ptr ptr = oZoneManager.GetAreaManager().AddArea(EventArea::Area::E_ConvexPolygon, EventAreaTable::Instance().GetElement(4));

    oZoneManager.GetAreaManager().OpenArea(4);

    EventArea::Area::verts_type area_v;

    Vector3 pos;
    pos.set_x(0);
    pos.set_z(0);
    area_v.push_back(pos);

    pos.set_x(0);
    pos.set_z(10);
    area_v.push_back(pos);

    pos.set_x(10);
    pos.set_z(10);
    area_v.push_back(pos);

    pos.set_x(10);
    pos.set_z(0);
    area_v.push_back(pos);
    ptr->Init(area_v);

    oZoneManager.Init(100, 100, 10);

    Obj_Human oHuman;
    ObjHumanAreaModule o(&oHuman);
    ObjHumanAreaModule o1(&oHuman);
    o.SetZm(&oZoneManager);
    o1.SetZm(&oZoneManager);

    Vector3 v;
    v.set_x(9);
    v.set_z(9);


    o.SetPos(v);
    o.SetZone(oZoneManager.GetZone(0));
    o1.SetPos(v);
    o1.SetZone(oZoneManager.GetZone(0));

    o.UpdateEvetArea();
    o1.UpdateEvetArea();

    v.set_x(20);
    v.set_z(20);
    o.SetZone(oZoneManager.GetZone(2));
    o.SetPos(v);
    o.UpdateEvetArea();

    v.set_x(9);
    v.set_z(9);


    o.SetPos(v);
    o.SetZone(oZoneManager.GetZone(0));
    o1.SetPos(v);
    o1.SetZone(oZoneManager.GetZone(0));

    o.UpdateEvetArea();
    o1.UpdateEvetArea();

    oZoneManager.GetAreaManager().CleanUp();
    oZoneManager.GetAreaManager().OpenArea(4);
    o.UpdateEvetArea();
    o1.UpdateEvetArea();
    o.OnLeaveScene();
    o1.OnLeaveScene();
}

TEST(SceneManagerTest, MoreThanZoneSize)
{

	ZoneManager oZoneManager;
	oZoneManager.Init(1001,1001, 10);

	EXPECT_EQ(101 * 101, oZoneManager.GetZoneListSize());
}

TEST(SceneManagerTest, LessThanZoneSize)
{

	ZoneManager oZoneManager;
	oZoneManager.Init(999, 999, 10);

	EXPECT_EQ(100 * 100, oZoneManager.GetZoneListSize());
}

TEST(SceneManagerTest, TestTenZone)
{

	ZoneManager oZoneManager;
	oZoneManager.Init(9, 99, 10);

	EXPECT_EQ(1 * 10, oZoneManager.GetZoneListSize());
}

int32_t TestGetZoneId(float mapx, float mapy, float playerx, float playery)
{
	ZoneManager oZoneManager;
	oZoneManager.Init(mapx, mapy, 10);
	WORLD_POS pos;
	pos.m_fX = playerx;
	pos.m_fZ = playery;
	return oZoneManager.CalcZoneID(&pos);
}

TEST(SceneManagerTest, TestGetZoneId)
{

	{
		EXPECT_EQ(0, TestGetZoneId(9, 99, 9, 0));
	}
	
	{
		EXPECT_EQ(-1, TestGetZoneId(9, 99, 20, 0));
	}

	{
		EXPECT_EQ(9, TestGetZoneId(9, 99, 0, 99));
	}

	{
		EXPECT_EQ(-1, TestGetZoneId(9, 99, 0, 100));
	}
}

bool TestSurroundingZone(ZoneManager::zone_id_list_type oInputList, int32_t nZoneId)
{
	ZoneManager oZoneManager;
	oZoneManager.Init(100, 100, 10);

	ZoneManager::zone_id_list_type list = oZoneManager.GetSurroundingZoneIdList(nZoneId, 2);
	EXPECT_EQ(oInputList.size() * oInputList.size(), list.size());
	ZoneManager::zone_id_list_type retlist;
	for (ZoneManager::zone_id_list_type::iterator it = oInputList.begin(); it != oInputList.end(); ++it)
	{
		for (uint32_t i = 0; i < oInputList.size(); ++i)
		{
			
			retlist.push_back((*it) + i);
		}
	}
	
	return std::equal(list.begin(), list.end(), retlist.begin());
}

TEST(SceneManagerTest, TestSurroundingZone)
{
	ZoneManager oZoneManager;
	oZoneManager.Init(100, 100, 10);

	//top left 
	{
		ZoneManager::zone_id_list_type list = oZoneManager.GetSurroundingZoneIdList(0, 2);
		EXPECT_EQ(9, list.size());
		ZoneManager::zone_id_list_type retlist{0,1,2,10,11,12,20,21,22};
		EXPECT_TRUE(std::equal(list.begin(), list.end(), retlist.begin()));

		ZoneManager::zone_id_list_type inputlist{ 0, 10, 20};
		EXPECT_TRUE(TestSurroundingZone(inputlist, 0));
	}

	//top right
	{
		ZoneManager::zone_id_list_type list = oZoneManager.GetSurroundingZoneIdList(9, 2);
		EXPECT_EQ(9, list.size());
		ZoneManager::zone_id_list_type retlist{ 7, 8, 9, 17, 18, 19, 27, 28, 29 };
		EXPECT_TRUE(std::equal(list.begin(), list.end(), retlist.begin()));

		ZoneManager::zone_id_list_type inputlist{ 7, 17, 27 };
		EXPECT_TRUE(TestSurroundingZone(inputlist, 9));
	}

	//bottom left
	{
		ZoneManager::zone_id_list_type list = oZoneManager.GetSurroundingZoneIdList(90, 2);
		EXPECT_EQ(9, list.size());
		ZoneManager::zone_id_list_type retlist{ 70, 71, 72, 80, 81, 82, 90, 91, 92 };
		EXPECT_TRUE(std::equal(list.begin(), list.end(), retlist.begin()));

		ZoneManager::zone_id_list_type inputlist{ 70, 80, 90 };
		EXPECT_TRUE(TestSurroundingZone(inputlist, 90));
	}

	//bottom right
	{
		ZoneManager::zone_id_list_type list = oZoneManager.GetSurroundingZoneIdList(99, 2);
		EXPECT_EQ(9, list.size());
		ZoneManager::zone_id_list_type retlist{ 77, 78, 79, 87, 88, 89, 97, 98, 99 };
		EXPECT_TRUE(std::equal(list.begin(), list.end(), retlist.begin()));

		ZoneManager::zone_id_list_type inputlist{ 77, 87, 97 };
		EXPECT_TRUE(TestSurroundingZone(inputlist, 99));
	}


	//center

	{
		ZoneManager::zone_id_list_type list = oZoneManager.GetSurroundingZoneIdList(44, 2);
		EXPECT_EQ(25, list.size());
		ZoneManager::zone_id_list_type retlist{ 22, 23, 24, 25, 26,
			32, 33, 34, 35, 36,
			42, 43, 44, 45, 46,
			52, 53, 54, 55, 56,
			62, 63, 64, 65, 66 };
		EXPECT_TRUE(std::equal(list.begin(), list.end(), retlist.begin()));

		ZoneManager::zone_id_list_type inputlist{ 22, 32, 42, 52, 62 };
		EXPECT_TRUE(TestSurroundingZone(inputlist, 44));
	}

	//rect
	{
		ZoneManager oRectZoneManager;
		oRectZoneManager.Init(100, 1000, 10);
		ZoneManager::zone_id_list_type list = oRectZoneManager.GetSurroundingZoneIdList(44, 2 );
		EXPECT_EQ(25, list.size());
		ZoneManager::zone_id_list_type retlist{ 22, 23, 24, 25, 26,
			32, 33, 34, 35, 36,
			42, 43, 44, 45, 46,
			52, 53, 54, 55, 56,
			62, 63, 64, 65, 66 };
		EXPECT_TRUE(std::equal(list.begin(), list.end(), retlist.begin()));
	}

	//rect
	{
		ZoneManager oRectZoneManager;
		oRectZoneManager.Init(1000, 100, 10);
		ZoneManager::zone_id_list_type list = oRectZoneManager.GetSurroundingZoneIdList(404, 2);
		EXPECT_EQ(25, list.size());
		ZoneManager::zone_id_list_type retlist{ 202, 203, 204, 205, 206, 
			302, 303, 304, 305, 306, 
			402, 403, 404, 405, 406, 
			502, 503, 504, 505, 506, 
			602, 603, 604, 605, 606 };
		EXPECT_TRUE(std::equal(list.begin(), list.end(), retlist.begin()));
		list = oRectZoneManager.GetSurroundingZoneIdList(534);
		EXPECT_FALSE(std::equal(list.begin(), list.end(), retlist.begin()));
	}
}

void TestMoveZone(int32_t nOldZoneId, 
				  int32_t nNewZoneId, 
				  size_t nOldSize, 
				  size_t nNewSize, 
				  ZoneManager::zone_id_list_type tNewList, 
				  ZoneManager::zone_id_list_type tOldList)
{
	ZoneManager oRectZoneManager;
	oRectZoneManager.Init(1000, 100, 10);

	std::sort(tNewList.begin(), tNewList.end());
	std::sort(tOldList.begin(), tOldList.end());

	ZoneManager::zone_id_list_type oOldList;
	ZoneManager::zone_id_list_type oNewList;
	oRectZoneManager.ChangeZoneGetIdList(nOldZoneId, oOldList, nNewZoneId, oNewList, 2);
	EXPECT_EQ(nOldSize, oOldList.size());
	EXPECT_EQ(nNewSize, oNewList.size());
	EXPECT_TRUE(std::equal(oOldList.begin(), oOldList.end(), tOldList.begin()));
	EXPECT_TRUE(std::equal(oNewList.begin(), oNewList.end(), tNewList.begin()));
}



//
//TEST(SceneManagerTest, TestMoveZone2)
//{
//	//up
//	{
//		TestMoveZone(305, 205, 5, 5, { 003, 004, 005, 006, 007 }, { 503, 504, 505, 506, 507 });
//	}
//
//	//right
//	{
//
//		TestMoveZone(405, 406, 5, 5, { 208, 308, 408, 508, 608 }, { 203, 303, 403, 503, 603 });
//	}
//
//
//	//down
//	{
//
//		TestMoveZone(405, 505, 5, 5, { 703, 704, 705, 706, 707 }, { 203, 204, 205, 206, 207 });
//	}
//
//
//	//Left
//	{
//
//		TestMoveZone(405, 404, 5, 5, { 202, 302, 402, 502, 602 }, { 207, 307, 407, 507, 607 });
//	}
//
//
//	//top
//	{
//
//		TestMoveZone(105, 005, 5, 0, {}, { 303, 304, 305, 306, 307 });
//	}
//
//
//	//|<------
//	{
//		TestMoveZone(001, 000, 3, 0, {}, { 003, 103, 203 });
//	}
//
//	//left up
//	{
//		TestMoveZone(101, 000, 7, 0, {}, { 003, 103, 203, 303, 302, 301, 300 });
//	}
//
//	//left 
//	{
//		TestMoveZone(201, 200, 5, 0, {}, { 003, 103, 203, 303, 403});
//	}
//
//
//	//left down
//	{
//		TestMoveZone(801, 900, 7, 0, {}, { 600, 601, 602, 603, 703, 803, 903 });
//	}
//
//	//buttom
//	{
//		TestMoveZone(801, 901, 4, 0, {}, { 600, 601, 602, 603});
//	}
//
//	//buttom
//	{
//		TestMoveZone(708, 808, 5, 0, {}, { 506, 507, 508, 509, 510 });
//	}
//
//	//buttom right
//	{
//		TestMoveZone(898, 999, 7, 0, {}, { 696, 697, 698, 699, 796, 896, 996 });
//	}
//
//	//right
//	{
//		TestMoveZone(498, 499, 5, 0, {}, { 296, 396, 496, 596, 696});
//	}
//
//
//	//right
//	{
//		TestMoveZone(198, 99, 7, 0, {}, { 396, 397, 398, 399, 296, 196, 96 });
//	}
//
//	//top
//	{
//		TestMoveZone(196, 96, 5, 0, {}, { 394, 395, 396, 397, 398 });
//	}
//}

TEST(SceneManagerTest, TestMoveZone)
{
    //up
    {
        TestMoveZone(305, 205, 5, 5, { 003, 004, 005, 006, 007 }, { 503, 504, 505, 506, 507 });
    }

    //right
    {

        TestMoveZone(405, 406, 5, 5, { 208, 308, 408, 508, 608 }, { 203, 303, 403, 503, 603 });
    }


    //down
    {

        TestMoveZone(405, 505, 5, 5, { 703, 704, 705, 706, 707 }, { 203, 204, 205, 206, 207 });
    }


    //Left
    {

        TestMoveZone(405, 404, 5, 5, { 202, 302, 402, 502, 602 }, { 207, 307, 407, 507, 607 });
    }


    //top
    {

        TestMoveZone(105, 005, 5, 0, {}, { 303, 304, 305, 306, 307 });
    }


    //|<------
    {
        TestMoveZone(001, 000, 3, 0, {}, { 003, 103, 203 });
    }

    //left up
    {
        TestMoveZone(101, 000, 7, 0, {}, { 003, 103, 203, 303, 302, 301, 300 });
    }

    //left 
    {
        TestMoveZone(201, 200, 5, 0, {}, { 003, 103, 203, 303, 403 });
    }


    //left down
    {
        TestMoveZone(801, 900, 7, 0, {}, { 600, 601, 602, 603, 703, 803, 903 });
    }

    //buttom
    {
        TestMoveZone(801, 901, 4, 0, {}, { 600, 601, 602, 603 });
    }

    //buttom
    {
        TestMoveZone(708, 808, 5, 0, {}, { 506, 507, 508, 509, 510 });
    }

    //buttom right
    {
        TestMoveZone(898, 999, 7, 0, {}, { 696, 697, 698, 699, 796, 896, 996 });
    }

    //right
    {
        TestMoveZone(498, 499, 5, 0, {}, { 296, 396, 496, 596, 696 });
    }


    //right
    {
        TestMoveZone(198, 99, 7, 0, {}, { 396, 397, 398, 399, 296, 196, 96 });
    }

    //top
    {
        TestMoveZone(196, 96, 5, 0, {}, { 394, 395, 396, 397, 398 });
    }
}


int main(int argc, char **argv)
{
    ModuleConfig::Instance().Initialize();
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

