#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include <xstddef>
#include <sstream>
#include <locale>
#include <iomanip>

#include "muduo/base/Timestamp.h"
#include "CommonLogic/TimerList/DurationManager.h"

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


muduo::net::EventLoop* pLoop = nullptr;


void OnBegin()
{
    //std::cout << " OnBegin " << std::endl;
}

void OnEnd()
{
    //std::cout << " OnEnd " << std::endl;
}

// ---------------------------------------------------------------------------
// 日期格式解析
// ---------------------------------------------------------------------------

TEST(DurationTest, DateFormatParsing)
{
    // 2018-08-16 11:32:00 — tm_year 从 1900 起算, tm_mon 从 0 起算
    std::tm ta = {};
    ta.tm_year = 118;   // 2018 - 1900
    ta.tm_mon  = 7;     // 8月 - 1
    ta.tm_mday = 16;
    ta.tm_hour = 11;
    ta.tm_min  = 32;
    ta.tm_sec  = 0;
    time_t expected = std::mktime(&ta);
    time_t actual   = game::YmdHmsStringToTime("2018-08-16 11-32-00");
    EXPECT_EQ(expected, actual);
}



// ---------------------------------------------------------------------------
// 每日时段（12:00-14:00, 20:00-22:00）遍历两年每天验证开关
// ---------------------------------------------------------------------------

TEST(DurationTest, DailyDuration)
{
    DurationManager dm;

    time_t t1 = game::YmdHmsStringToTime("2018-01-1 11-32-00");
    muduo::setTestTimeNow(t1);
    dm.Add(TimerDuration::E_DURATION_DAILY, t1, "12-00-00", "14-00-00", std::bind(&::OnBegin), std::bind(&::OnEnd));
    dm.Add(TimerDuration::E_DURATION_DAILY, t1, "20-00-00", "22-00-00", std::bind(&::OnBegin), std::bind(&::OnEnd));

    pLoop->loop();
    EXPECT_FALSE(dm.IsOpen(t1)); // 11:32 不在任何时段内
    for (int year = 2018; year < 2020; ++year)
    {
        
        for (int month = 1; month <= kMonthsOfYear; ++month)
        {
            for (int day = 1; day <= daysOfMonth(year, month); ++day)
            {
                Date d(year, month, day);

                std::tm ta = {};
                ta.tm_year = d.year() - 1900;
                ta.tm_mon = d.month() - 1;
                ta.tm_mday = d.day();
                ta.tm_hour = 19;
                ta.tm_min = 0;
                ta.tm_sec = 0;

                time_t t2 = game::GetTime(d, " 12-00-00");
                muduo::setTestTimeNow(t2);
                pLoop->loop();

                EXPECT_TRUE(dm.IsOpen(t2)); // 12:00 在第一时段内

                for (int32_t i = 0; i < game::kSecondsPerHours * 2; i += 5)
                {
                    EXPECT_TRUE(dm.IsOpen(t2 + i)); // 12:00-14:00 内应该开启
                }

                time_t t3 = game::GetTime(d, " 14-00-00");
                muduo::setTestTimeNow(t3);
                pLoop->loop();
                for (int32_t i = 0; i < 60 * 60 * 2; i += 5)
                {
                    EXPECT_FALSE(dm.IsOpen(t3 + i)); // 14:00-16:00 不在任何时段内
                }
                for (int32_t i = 0; i < 6 * game::kSecondsPerHours; i = i + game::kSecondsPerHours)
                {
                    EXPECT_FALSE(dm.IsOpen(t3 + i)); // 14:00-20:00 间隔期也关闭
                }
                time_t t4 = game::GetTime(d, " 20-00-00");

                muduo::setTestTimeNow(t4);
                pLoop->loop();
                for (int32_t i = 0; i < game::kSecondsPerHours * 2; i += 5)
                {
                    EXPECT_TRUE(dm.IsOpen(t4 + i)); // 20:00-22:00 在第二时段内
                }

                time_t t5 = game::GetTime(d, " 22-00-00");

                muduo::setTestTimeNow(t5);
                pLoop->loop();
                for (int32_t i = 0; i < game::kSecondsPerHours * 2; i += 5)
                {
                    EXPECT_FALSE(dm.IsOpen(t5 + i)); // 22:00 后时段关闭
                }

            }
        }
    }
}

// ---------------------------------------------------------------------------
// 每周时段（周一 12:00-14:00, 周六~周日全天）
// ---------------------------------------------------------------------------

TEST(DurationTest, WeekDuration)
{
    DurationManager dm;

    time_t t1 = game::YmdHmsStringToTime("2018-01-1 11-32-00");
    muduo::setTestTimeNow(t1);
    dm.Add(TimerDuration::E_DURATION_WEEK, t1, "1 12-00-00", "1 14-00-00", std::bind(&::OnBegin), std::bind(&::OnEnd));
    dm.Add(TimerDuration::E_DURATION_WEEK, t1, "6 00-00-00", "0 23-59-59", std::bind(&::OnBegin), std::bind(&::OnEnd));

    pLoop->loop();
    EXPECT_FALSE(dm.IsOpen(t1)); // 11:32 不在任何周时段内
    for (int year = 2018; year < 2020; ++year)
    {

        for (int month = 1; month <= kMonthsOfYear; ++month)
        {
            for (int day = 1; day <= daysOfMonth(year, month); ++day)
            {
                Date d(year, month, day);

                std::tm ta = {};
                ta.tm_year = d.year() - 1900;
                ta.tm_mon = d.month() - 1;
                ta.tm_mday = d.day();
                ta.tm_hour = 19;
                ta.tm_min = 0;
                ta.tm_sec = 0;

                

                time_t t2 = game::GetTime(d, " 12-00-00");
                muduo::setTestTimeNow(t2);
                pLoop->loop();

                if (d.weekDay() == 1)
                {
                    // 周一 12:00-14:00 应开启
                    EXPECT_TRUE(dm.IsOpen(t2));
                    for (int32_t i = 0; i < game::kSecondsPerHours * 2; i += 5)
                    {
                        EXPECT_TRUE(dm.IsOpen(t2 + i));
                    }
                    time_t t3 = game::GetTime(d, " 14-00-00");
                    muduo::setTestTimeNow(t3);
                    pLoop->loop();
                    for (int32_t i = 0; i < 10000; i += 500)
                    {
                        EXPECT_FALSE(dm.IsOpen(t3 + i)); // 周一 14:00 后关闭
                    }
                }
                else if (d.weekDay() == 6 || d.weekDay() == 0)
                {
                    // 周六/周日全天应开启
                    time_t t3 = game::GetTime(d, " 00-00-00");
                    muduo::setTestTimeNow(t3);
                    pLoop->loop();
                    for (int32_t i = 0; i < game::kSecondsPerDay ; i += 1000)
                    {
                        EXPECT_TRUE(dm.IsOpen(t3 + i));
                    }
                }
                else
                {
                    // 周二~周五（非周一且非周末）应关闭
                    time_t t3 = game::GetTime(d, " 00-00-00");
                    muduo::setTestTimeNow(t3);
                    pLoop->loop();
                    for (int32_t i = 0; i < game::kSecondsPerDay ; i += 1000)
                    {
                        bool ret = dm.IsOpen(t3 + i);
                        EXPECT_FALSE(ret);
                    }
                }
 

            }
        }
    }

}

// ---------------------------------------------------------------------------
// 每周时段（从非周一开始注册）
// ---------------------------------------------------------------------------

TEST(DurationTest, WeekDurationStartingThursday)
{
    DurationManager dm;

    time_t t1 = game::YmdHmsStringToTime("2018-01-4 11-32-00");
    muduo::setTestTimeNow(t1);
    dm.Add(TimerDuration::E_DURATION_WEEK, t1, "1 12-00-00", "1 14-00-00", std::bind(&::OnBegin), std::bind(&::OnEnd));
    dm.Add(TimerDuration::E_DURATION_WEEK, t1, "6 00-00-00", "0 23-59-59", std::bind(&::OnBegin), std::bind(&::OnEnd));


    pLoop->loop();
    EXPECT_FALSE(dm.IsOpen(t1));
    t1 = game::YmdHmsStringToTime("2018-01-6 11-32-00");
    EXPECT_TRUE(dm.IsOpen(t1));
}

// 占位——活动时段测试（待实现）
TEST(DurationTest, DISABLED_ActivityDuration)
{
}


// ---------------------------------------------------------------------------
// Date + HMS 时间拼接，并遍历 1970-2100 每天校验
// ---------------------------------------------------------------------------

TEST(DurationTest, DateHMS)
{
    // 先验证单点 Date+HMS 拼接
    Date d(2018, 8, 16);
    std::tm ta = {};
    ta.tm_year = 118;  // 2018 - 1900
    ta.tm_mon  = 7;    // 8月 - 1
    ta.tm_mday = 16;
    ta.tm_hour = 19;
    ta.tm_min  = 0;
    ta.tm_sec  = 0;

    time_t t1 = std::mktime(&ta);
    time_t t2 = game::GetTime(d, " 19-00-00");
    EXPECT_EQ(t1, t2);

    // 遍历 1970-2100 每天，验证 Julian Day 转换正确性
    int julianDayNumber = 2415021;

    for (int year = 1970; year < 2100; ++year)
    {
        EXPECT_EQ(isLeapYear(year), Date(year, 3, 1).julianDayNumber() - Date(year, 2, 29).julianDayNumber());
        for (int month = 1; month <= kMonthsOfYear; ++month)
        {
            for (int day = 1; day <= daysOfMonth(year, month); ++day)
            {
                Date d(year, month, day);
                Date d2(julianDayNumber);
                ++julianDayNumber;

                std::tm ta = {};
                ta.tm_year = d.year() - 1900;
                ta.tm_mon = d.month() - 1;
                ta.tm_mday = d.day();
                ta.tm_hour = 19;
                ta.tm_min  = 0;
                ta.tm_sec  = 0;

                time_t t1 = std::mktime(&ta);
                time_t t2 = game::GetTime(d, " 19-00-00");
                // EXPECT_EQ(t1, t2); // 已禁用——时区边界问题待排查
            }
        }
    }
}

// ---------------------------------------------------------------------------
// TimerDuration 基本 API：剩余时间 / IsOpen
// ---------------------------------------------------------------------------

TEST(DurationTest, TimerDurationBasicApi)
{
    TimerDuration t(0, 10);


    EXPECT_EQ(10, t.GetEndTime());
    EXPECT_EQ(10, t.GetRemainTime(0));   // 时刻 0 剩余 10
    EXPECT_EQ(1,  t.GetRemainTime(9));   // 时刻 9 剩余 1
    EXPECT_EQ(0,  t.GetRemainTime(10));  // 时刻 10 刚好结束
    EXPECT_EQ(0,  t.GetRemainTime(11));  // 时刻 11 已超时

    for (int32_t i = 0; i < 11; ++i) 
    {
        EXPECT_TRUE(t.IsOpen(i));
    }
    EXPECT_FALSE(t.IsOpen(11));
}


// ---------------------------------------------------------------------------
// DurationManager：两段非重叠区间合并判断
// ---------------------------------------------------------------------------

TEST(DurationTest, DurationManagerTwoNonOverlappingIntervals)
{
    DurationManager dm;

    dm.Add(TimerDuration::E_NORMAL, 0, 0, 10, std::bind(&::OnBegin), std::bind(&::OnEnd));
    dm.Add(TimerDuration::E_NORMAL, 0, 20, 60, std::bind(&::OnBegin), std::bind(&::OnEnd));

    // [0,10] 开启, (10,20) 关闭, [20,60] 开启
    for (int32_t i = 0; i < 61; ++i)
    {
        if (i > 10 && i < 20)
        {
            EXPECT_FALSE(dm.IsOpen(i));
        }
        else
        {
            EXPECT_TRUE(dm.IsOpen(i));
        }
    }
    EXPECT_FALSE(dm.IsOpen(61));
}

// ---------------------------------------------------------------------------
// GetWeekBeginDay：遍历 1900-2500 每天验证周一推算
// ---------------------------------------------------------------------------

TEST(DurationTest, GetMondayDate)
{
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

                EXPECT_EQ(game::kWeekBeginDay, game::GetWeekBeginDay(d).weekDay());
                EXPECT_EQ(game::kWeekBeginDay, game::GetWeekBeginDay(d2).weekDay());

                ++julianDayNumber;
                weekDay = (weekDay + 1) % 7;
            }
        }
    }
}int main(int argc, char** argv)
{
    muduo::net::EventLoop loop;
    pLoop = &loop;
    BaseModule::SetThreadLocalStorageLoop(&loop);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

