
#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <gtest/gtest.h>
#include <muduo/base/Date.h>

using muduo::Date;

const int kMonthsOfYear = 12;

int isLeapYear(int year)
{
	if (year % 400 == 0)
		return 1;
	else if (year % 100 == 0)
		return 0;
	else if (year % 4 == 0)
		return 1;
	else
		return 0;
}

int daysOfMonth(int year, int month)
{
	static int days[2][kMonthsOfYear + 1] =
	{
		{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	};
	return days[isLeapYear(year)][month];
}

void passByConstReference(const Date& x)
{
	printf("%s\n", x.toIsoString().c_str());
}

void passByValue(Date x)
{
	printf("%s\n", x.toIsoString().c_str());
}

TEST(DateTest, YearMonthDay) {

	time_t now = time(NULL);
	struct tm t1 = *gmtime(&now);
	struct tm t2 = *localtime(&now);
	Date someDay(2008, 9, 10);
	printf("%s\n", someDay.toIsoString().c_str());
	passByValue(someDay);
	passByConstReference(someDay);
	Date todayUtc(t1);
	printf("%s\n", todayUtc.toIsoString().c_str());
	Date todayLocal(t2);
	printf("%s\n", todayLocal.toIsoString().c_str());

	int julianDayNumber = 2415021;
	int weekDay = 1; // Monday

	for (int year = 1900; year < 2500; ++year)
	{
		EXPECT_EQ(isLeapYear(year), Date(year, 3, 1).julianDayNumber() - Date(year, 2, 29).julianDayNumber());
		for (int month = 1; month <= kMonthsOfYear; ++month)
		{
			for (int day = 1; day <= daysOfMonth(year, month); ++day)
			{
				Date d(year, month, day);
				// printf("%s %d\n", d.toString().c_str(), d.weekDay());
				EXPECT_EQ(year, d.year());
				EXPECT_EQ(month, d.month());
				EXPECT_EQ(day, d.day());
				EXPECT_EQ(weekDay, d.weekDay());
				EXPECT_EQ(julianDayNumber, d.julianDayNumber());

				Date d2(julianDayNumber);
				EXPECT_EQ(year, d2.year());
				EXPECT_EQ(month, d2.month());
				EXPECT_EQ(day, d2.day());
				EXPECT_EQ(weekDay, d2.weekDay());
				EXPECT_EQ(julianDayNumber, d2.julianDayNumber());

				++julianDayNumber;
				weekDay = (weekDay + 1) % 7;
			}
		}
	}
}

int main(int argc, char **argv)
{
	
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

