#include <iomanip>
#include <iostream>

#include <gtest/gtest.h>

using namespace std;

#define PI 3.14159265

float GetCos(float degrees)
{
	degrees = std::fmod(degrees, 360);
	float c =  std::cos(degrees * PI / 180);

	if (std::abs(0 - c) < 0.0001)
	{
		c = 0;
	}
	return c;
}

float GetSin(float degrees)
{
	degrees = std::fmod(degrees, 360);
	float s = std::sin(degrees * PI / 180);

	if (std::abs(0 - s) < 0.0001)
	{
		s = 0;
	}
	return s;
}


TEST(DistanceTest, 0Dgree)
{
	float c = GetCos(0);
	float s = GetSin(0);

	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;

	EXPECT_EQ(nX, 5);
	EXPECT_EQ(nZ, 0);


	for (int i = -500; i < 500; ++i)
	{
		float c = GetCos(i * 360 + 0);
		float s = GetSin(i * 360 + 0);
		float nX = 0 + c * 5;
		float nZ = 0 + s * 5;
		EXPECT_EQ(nX, 5);
		EXPECT_EQ(nZ, 0);
	}
}

TEST(DistanceTest, 30Dgree)
{
	float c = GetCos(30);
	float s = GetSin(30);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_TRUE(nX > nZ);

	for (int i = -500; i < 500; ++i)
	{
		float c = GetCos(i * 360 + 30);
		float s = GetSin(i * 360 + 30);
		float nX = 0 + c * 5;
		float nZ = 0 + s * 5;
		EXPECT_TRUE(nX > nZ);
	}
}

TEST(DistanceTest, 45Dgree)
{
	float c = GetCos(45);
	float s = GetSin(45);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_EQ(nX, nZ);

	

	for (int i = -500; i < 500; ++i)
	{
		float c = GetCos(i * 360 + 45);
		float s = GetSin(i * 360 + 45);
		float nX = 0 + c * 5;
		float nZ = 0 + s * 5;
		EXPECT_EQ(nX, nZ);
	}
}

TEST(DistanceTest, 60Dgree)
{
	float c = GetCos(60);
	float s = GetSin(60);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_TRUE(nX < nZ);

	for (int i = -500; i < 500; ++i)
	{
		float c = GetCos(i * 360 + 60);
		float s = GetSin(i * 360 + 60);
		float nX = 0 + c * 5;
		float nZ = 0 + s * 5;
		EXPECT_TRUE(nX < nZ);
	}
}

TEST(DistanceTest, 90Dgree)
{
	float c = GetCos(90);
	float s = GetSin(90);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_EQ(nX, 0);
	EXPECT_EQ(nZ, 5);

	for (int i = -500; i < 500; ++i)
	{
		c = GetCos(i * 360 + 90);
		s = GetSin(i * 360 + 90);
		nX = 0 + c * 5;
		nZ = 0 + s * 5;
		EXPECT_EQ(nX, 0);
		EXPECT_EQ(nZ, 5);
	}
}

TEST(DistanceTest, 120Dgree)
{
	float c = GetCos(120);
	float s = GetSin(120);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_TRUE(nX < nZ);


	for (int i = -500; i < 500; ++i)
	{
		c = GetCos(i * 360 + 90);
		s = GetSin(i * 360 + 90);
		nX = 0 + c * 5;
		nZ = 0 + s * 5;
		EXPECT_TRUE(nX < nZ);
	}
}

TEST(DistanceTest, 135Dgree)
{
	float c = GetCos(135);
	float s = GetSin(135);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_EQ(nX, -nZ);
	EXPECT_TRUE(nX < 0);

	for (int i = -500; i < 500; ++i)
	{
		c = GetCos(i * 360 + 135);
		s = GetSin(i * 360 + 135);
		nX = 0 + c * 5;
		nZ = 0 + s * 5;
		EXPECT_EQ(nX, -nZ);
		EXPECT_TRUE(nX < 0);
	}
}

TEST(DistanceTest, 150Dgree)
{
	float c = GetCos(150);
	float s = GetSin(150);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_TRUE(fabs(nX) > fabs(nZ));


	for (int i = -500; i < 500; ++i)
	{
		c = GetCos(i * 360 + 150);
		s = GetSin(i * 360 + 150);
		nX = 0 + c * 5;
		nZ = 0 + s * 5;
		EXPECT_TRUE(fabs(nX) > fabs(nZ));
	}
}

TEST(DistanceTest, 180Dgree)
{
	float c = GetCos(180);
	float s = GetSin(180);

	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_EQ(nX, -5);
	EXPECT_EQ(nZ, 0);

	for (int i = -500; i < 500; ++i)
	{
		c = GetCos(i * 360 + 180);
		s = GetSin(i * 360 + 180);

		nX = 0 + c * 5;
		nZ = 0 + s * 5;
		EXPECT_EQ(nX, -5);
		EXPECT_EQ(nZ, 0);
	}

	
}

TEST(DistanceTest, 210Dgree)
{
	float c = GetCos(210);
	float s = GetSin(210);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_TRUE(fabs(nX) > fabs(nZ));


	for (int i = -500; i < 500; ++i)
	{
		c = GetCos(i * 360 + 210);
		s = GetSin(i * 360 + 210);
		nX = 0 + c * 5;
		nZ = 0 + s * 5;
		EXPECT_TRUE(fabs(nX) > fabs(nZ));
	}
}

TEST(DistanceTest, 225Dgree)
{
	float c = GetCos(225);
	float s = GetSin(225);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_EQ(nX, nZ);
	EXPECT_TRUE(nX < 0);
}

TEST(DistanceTest, 270Dgree)
{
	float c = GetCos(270);
	float s = GetSin(270);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_EQ(nX, 0);
	EXPECT_EQ(nZ, -5);

	for (int i = -500; i < 500; ++i)
	{
		float c = GetCos(i * 360 + 270);
		float s = GetSin(i * 360 +  270);
		float nX = 0 + c * 5;
		float nZ = 0 + s * 5;
		EXPECT_EQ(nX, 0);
		EXPECT_EQ(nZ, -5);
	}
}

TEST(DistanceTest, 300Dgree)
{
	float c = GetCos(300);
	float s = GetSin(300);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_TRUE(fabs(nX) < fabs(nZ));


	for (int i = -500; i < 500; ++i)
	{
		c = GetCos(i * 360 + 300);
		s = GetSin(i * 360 + 300);
		nX = 0 + c * 5;
		nZ = 0 + s * 5;
		EXPECT_TRUE(fabs(nX) < fabs(nZ));
	}
}

TEST(DistanceTest, 315Dgree)
{
	float c = GetCos(315);
	float s = GetSin(315);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_EQ(nX, -nZ);
	EXPECT_TRUE(nX > 0);

	for (int i = -500; i < 500; ++i)
	{
		float c = GetCos(i * 360 + 315);
		float s = GetSin(i * 360 + 315);
		float nX = 0 + c * 5;
		float nZ = 0 + s * 5;
		EXPECT_EQ(nX, -nZ);
		EXPECT_TRUE(nX > 0);
	}
}

TEST(DistanceTest, 330Dgree)
{
	float c = GetCos(330);
	float s = GetSin(330);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_TRUE(fabs(nX) > fabs(nZ));


	for (int i = -500; i < 500; ++i)
	{
		c = GetCos(i * 360 + 330);
		s = GetSin(i * 360 + 330);
		nX = 0 + c * 5;
		nZ = 0 + s * 5;
		EXPECT_TRUE(fabs(nX) > fabs(nZ));
	}
}


TEST(DistanceTest, 360Dgree)
{
	float c = GetCos(360);
	float s = GetSin(360);
	float nX = 0 + c * 5;
	float nZ = 0 + s * 5;
	EXPECT_EQ(nX, 5);
	EXPECT_EQ(nZ, 0);

	for (int i = -500; i < 500; ++i)
	{
		float c = GetCos(i * 360 + 360);
		float s = GetSin(i * 360 + 360);
		float nX = 0 + c * 5;
		float nZ = 0 + s * 5;
		EXPECT_EQ(nX, 5);
		EXPECT_EQ(nZ, 0);
	}
}




int main(int argc, char **argv)
{

	std::setprecision(2);

	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}



