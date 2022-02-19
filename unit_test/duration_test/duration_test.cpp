#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include <iostream>
#include <xstddef>

#include "muduo/base/Timestamp.h"



#include "CommonLogic/TimerList/DurationManager.h"



#include <iostream>
#include <sstream>
#include <locale>
#include <iomanip>
#include <algorithm>
#include <string>



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


muduo::net::EventLoop * pLoop = NULL;


void OnBegin()
{
    //std::cout << " OnBegin " << std::endl;
}

void OnEnd()
{
    //std::cout << " OnEnd " << std::endl;
}

TEST(Quest, DateFormat)
{
    time_t now = time(NULL);

    std::tm ta = {};
    ta.tm_year = 118;
    ta.tm_mon = 7;
    ta.tm_mday = 16;
    ta.tm_hour = 11;
    ta.tm_min = 32;
    ta.tm_sec = 0;
    time_t t1 = std::mktime(&ta);
    time_t t2 = game::YmdHmsStringToTime("2018-08-16 11-32-00");
    EXPECT_EQ(t1, t2);
    

}



TEST(QuestTest, DailyDuration)
{

    DurationManager dm;
    
    time_t t1 = game::YmdHmsStringToTime("2018-01-1 11-32-00");
    muduo::setTestTimeNow(t1);
    dm.Add(TimerDuration::E_DURATION_DAILY, t1, "12-00-00", "14-00-00", std::bind(&::OnBegin), std::bind(&::OnEnd));
    dm.Add(TimerDuration::E_DURATION_DAILY, t1, "20-00-00", "22-00-00", std::bind(&::OnBegin), std::bind(&::OnEnd));
   
    int julianDayNumber = 2415021;
    int weekDay = 1; // Monday
    pLoop->loop();
    EXPECT_FALSE(dm.IsOpen(t1));
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
             
                EXPECT_TRUE(dm.IsOpen(t2));

               
                
                for (int32_t i = 0; i < game::kSecondsPerHours * 2; i += 5)
                {
                    EXPECT_TRUE(dm.IsOpen(t2 + i));
                }

                time_t t3 = game::GetTime(d, " 14-00-00");  
                muduo::setTestTimeNow(t3);
                pLoop->loop();
                for (int32_t i = 0; i < 60 * 60 * 2; i += 5)
                {
                    EXPECT_FALSE(dm.IsOpen(t3 + i));
                }
                for (int32_t i = 0; i < 6 * game::kSecondsPerHours; i = i + game::kSecondsPerHours)
                {
                    EXPECT_FALSE(dm.IsOpen(t3 + i));
                }
                time_t t4 = game::GetTime(d, " 20-00-00");
                

                muduo::setTestTimeNow(t4);
                pLoop->loop();
                for (int32_t i = 0; i < game::kSecondsPerHours * 2; i += 5)
                {
                    EXPECT_TRUE(dm.IsOpen(t4 + i));
                }


                time_t t5 = game::GetTime(d, " 22-00-00");


                muduo::setTestTimeNow(t5);
                pLoop->loop();
                for (int32_t i = 0; i < game::kSecondsPerHours * 2; i += 5)
                {
                    EXPECT_FALSE(dm.IsOpen(t5 + i));
                }

            }
        }
    }
}

TEST(QuestTest, WeekDuration)
{
    DurationManager dm;

    time_t t1 = game::YmdHmsStringToTime("2018-01-1 11-32-00");
    muduo::setTestTimeNow(t1);
    dm.Add(TimerDuration::E_DURATION_WEEK, t1, "1 12-00-00", "1 14-00-00", std::bind(&::OnBegin), std::bind(&::OnEnd));
    dm.Add(TimerDuration::E_DURATION_WEEK, t1, "6 00-00-00", "0 23-59-59", std::bind(&::OnBegin), std::bind(&::OnEnd));

    int julianDayNumber = 2415021;
 
    pLoop->loop();
    EXPECT_FALSE(dm.IsOpen(t1));
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
                        EXPECT_FALSE(dm.IsOpen(t3 + i));
                    }
                  
                }
                else if (d.weekDay() == 6 || d.weekDay() == 0)
                {
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

TEST(QuestTest, WeekDuration1)
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

TEST(QuestTest, ActvityDuration)
{


}


TEST(QuestTest, DateHMS)
{

    Date d(2018, 8, 16);
    std::tm ta = {};
    ta.tm_year = 118;
    ta.tm_mon = 7;
    ta.tm_mday = 16;
    ta.tm_hour = 19;
    ta.tm_min = 0;
    ta.tm_sec = 0;


    time_t t1 = std::mktime(&ta);
    time_t t2 = game::GetTime(d, " 19-00-00");
    EXPECT_EQ(t1, t2);
    int julianDayNumber = 2415021;
    int weekDay = 1; // Monday

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
                ta.tm_min = 0;
                ta.tm_sec = 0;


                time_t t1 = std::mktime(&ta);
                time_t t2 = game::GetTime(d, " 19-00-00");
               // EXPECT_EQ(t1, t2);
               


            }
        }
    }
}

TEST(QuestTest, TestTimerDuration)
{
    TimerDuration t(0, 10);


    EXPECT_EQ(10 , t.GetEndTime());    
    EXPECT_EQ(10, t.GetRemainTime(0));
    EXPECT_EQ(1, t.GetRemainTime(9));
    EXPECT_EQ(0, t.GetRemainTime(10));
    EXPECT_EQ(0, t.GetRemainTime(11));

    for (int32_t i = 0; i < 11; ++i) 
    {
        EXPECT_TRUE(t.IsOpen(i));
    }
    EXPECT_FALSE(t.IsOpen(11));
}


TEST(QuestTest, DurationManager1)
{
    DurationManager dm;

    dm.Add(TimerDuration::E_NORMAL, 0, 0, 10, std::bind(&::OnBegin), std::bind(&::OnEnd));
    dm.Add(TimerDuration::E_NORMAL, 0, 20, 60, std::bind(&::OnBegin), std::bind(&::OnEnd));
    
     
   
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

TEST(QuestTest, GetMondayDate)
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

                EXPECT_EQ(game::kWeekBeginDay, game::GetWeekBeginDay(d).weekDay() );
                EXPECT_EQ(game::kWeekBeginDay, game::GetWeekBeginDay(d2).weekDay() );

                ++julianDayNumber;
                weekDay = (weekDay + 1) % 7;

                
                
               
            }
        }
    }
}



int main(int argc, char **argv)
{
    muduo::net::EventLoop loop;
    pLoop = &loop;
    BaseModule::SetThreadLocalStorageLoop(&loop);
    //GlobalDailyActivity::Instance();

    testing::InitGoogleTest(&argc, argv);

    while (true)
    {
        RUN_ALL_TESTS();
    }
    return RUN_ALL_TESTS();
}

