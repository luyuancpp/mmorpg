#include <gtest/gtest.h>


#include "DK_Mysql.h"
#include "PlayerDB.pb.h"

#include <iostream>

using namespace std;


TEST(PBTypeTest, typetest)
{
	cout << UINTMAX_MAX << endl;
	EXPECT_EQ(UINTMAX_MAX, stringToNum<uint64_t>("18446744073709551615"));
}

TEST(PBTypeTest, TestCreateSql)
{
	DK_Mysql pbmysql;
	pbmysql.initHandle();
	playerdb pb;	
}

TEST(PBTypeTest, TestSerialize)
{
	CooldownDBList data;
	data.add_cool_down_datas()->set_id(2);
	std::string bf = data.SerializeAsString();
	CooldownDBList data2;
	data2.ParseFromString(bf);
	EXPECT_EQ(data2.cool_down_datas(0).id(), 2);
}

TEST(PBTypeTest, TestInsertSql)
{
	CMysql_DBURL dbinfo("127.0.0.1", 3306, "test", "root", "luyuan");
	DK_Mysql pbmysql;
	pbmysql.SetDBInfo(dbinfo);
	pbmysql.initHandle();
	playerdb pb;
	pb.set_id(100);
	pb.set_cur_mp(100);
	pb.mutable_cool_down_datas()->add_cool_down_datas()->set_id(2);
	pbmysql.Insert(pb);
	pb.set_id(200);
	pbmysql.Insert(pb);

	playerdb spb;
	pbmysql.Select(spb, "id", "200");

	EXPECT_EQ(spb.cur_mp(), 100);
	EXPECT_EQ(spb.mutable_cool_down_datas()->cool_down_datas(0).id(), 2);

	playerdb spb1;
	pbmysql.Select(spb1, "id", "200");

	EXPECT_EQ(spb1.cur_mp(), 100);
	EXPECT_EQ(spb1.mutable_cool_down_datas()->cool_down_datas(0).id(), 2);

}

TEST(PBTypeTest, TestUpdateSql)
{
	CMysql_DBURL dbinfo("127.0.0.1", 3306, "test", "root", "luyuan");
	DK_Mysql pbmysql;
	pbmysql.SetDBInfo(dbinfo);
	pbmysql.initHandle();
	playerdb pb;
	pb.set_id(100);
	pb.set_cur_mp(10000);
	pb.mutable_cool_down_datas()->add_cool_down_datas()->set_id(2);


	pbmysql.Update(pb);
	pb.set_id(200);
	pbmysql.Update(pb);

	playerdb spb;
	spb.set_id(100);
	pbmysql.Select(spb, "id", "100");

	EXPECT_EQ(spb.cur_mp(), 10000);
	EXPECT_EQ(spb.mutable_cool_down_datas()->cool_down_datas(0).id(), 2);


	playerdb spb1;
	spb1.set_id(200);
	pbmysql.Select(spb1, "id", "200");

	EXPECT_EQ(spb1.cur_mp(), 10000);
	EXPECT_EQ(spb1.mutable_cool_down_datas()->cool_down_datas(0).id(), 2);


}


TEST(PBTypeTest, TestReplaceSql)
{
	CMysql_DBURL dbinfo("127.0.0.1", 3306, "test", "root", "luyuan");
	DK_Mysql pbmysql;
	pbmysql.SetDBInfo(dbinfo);
	pbmysql.initHandle();
	playerdb pb;
	pb.set_id(100);
	pb.set_cur_mp(10000);
	pb.mutable_cool_down_datas()->add_cool_down_datas()->set_id(2);


	pbmysql.Replace(pb);
	pb.set_id(200);
	pbmysql.Replace(pb);

	playerdb spb;
	spb.set_id(100);
	pbmysql.Select(spb, "id", "100");

	EXPECT_EQ(spb.cur_mp(), 10000);
	EXPECT_EQ(spb.mutable_cool_down_datas()->cool_down_datas(0).id(), 2);


	playerdb spb1;
	spb1.set_id(200);
	pbmysql.Select(spb1, "id", "200");

	EXPECT_EQ(spb1.cur_mp(), 10000);
	EXPECT_EQ(spb1.mutable_cool_down_datas()->cool_down_datas(0).id(), 2);


}


TEST(PBTypeTest, selectall)
{
	CMysql_DBURL dbinfo("127.0.0.1", 3306, "test", "root", "luyuan");
	DK_Mysql pbmysql;
	pbmysql.SetDBInfo(dbinfo);
	pbmysql.initHandle();
	playerdblist pb;

	pbmysql.Select(pb, "", "");
	
	EXPECT_EQ(pb.playerlist_size(), 2);

	EXPECT_EQ(pb.playerlist_size(), 2);
	EXPECT_EQ(pb.playerlist(0).id(), 100);
	EXPECT_EQ(pb.playerlist(0).cur_mp(), 10000);

	EXPECT_EQ(pb.playerlist(1).id(), 200);
	EXPECT_EQ(pb.playerlist(1).cur_mp(), 10000);

	

}

int main(int argc, char **argv)
{

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}


