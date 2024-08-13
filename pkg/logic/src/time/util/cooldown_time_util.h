#pragma once

#include <chrono>
#include <muduo/base/Timestamp.h>
#include "logic/component/time_meter_comp.pb.h"

class CoolDownTimeSecondUtil {
public:
    // ����ʣ��ʱ�䣨�룩
    static uint64_t Remaining(const TimeMeterComp& timeMeterComp) {
        uint64_t currentSeconds = GetCurrentTimeInSeconds();
        uint64_t elapsed = (currentSeconds > timeMeterComp.start())
            ? currentSeconds - timeMeterComp.start()
            : 0;
        return (timeMeterComp.duration() > elapsed)
            ? timeMeterComp.duration() - elapsed
            : 0;
    }

    // ���ʱ��������Ƿ�ʱ
    static bool IsExpired(const TimeMeterComp& timeMeterComp) {
        return Remaining(timeMeterComp) == 0;
    }

    // ��鵱ǰʱ���Ƿ��ڿ�ʼʱ��֮ǰ
    static bool IsBeforeStart(const TimeMeterComp& timeMeterComp) {
        return GetCurrentTimeInSeconds() < timeMeterComp.start();
    }

    // ��鵱ǰʱ���Ƿ�δ��ʼ�����Ƿ��ڿ�ʼʱ��֮ǰ��
    static bool IsNotStarted(const TimeMeterComp& timeMeterComp) {
        return IsBeforeStart(timeMeterComp);
    }

    // ����ʱ�������
    static void Reset(TimeMeterComp& timeMeterComp) {
        timeMeterComp.set_start(GetCurrentTimeInSeconds());
    }

    // ��ȡ��ǰʱ�䣨�룩
    inline static uint64_t GetCurrentTimeInSeconds() {
        return muduo::Timestamp::now().secondsSinceEpoch();
    }

    // ����ʱ���ȡʵ��
    /*
    inline static uint64_t GetCurrentTimeInSeconds() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
    */
};


class CoolDownTimeMillisecondUtil {
public:
    // ����ʣ��ʱ�䣨���룩
    static uint64_t Remaining(const TimeMeterComp& timeMeterComp) {
        uint64_t currentMilliseconds = GetCurrentTimeInMilliseconds();
        uint64_t elapsed = (currentMilliseconds > timeMeterComp.start())
            ? currentMilliseconds - timeMeterComp.start()
            : 0;
        return (timeMeterComp.duration() > elapsed)
            ? timeMeterComp.duration() - elapsed
            : 0;
    }

    // ���ʱ��������Ƿ�ʱ
    static bool IsExpired(const TimeMeterComp& timeMeterComp) {
        return Remaining(timeMeterComp) == 0;
    }

    // ��鵱ǰʱ���Ƿ��ڿ�ʼʱ��֮ǰ
    static bool IsBeforeStart(const TimeMeterComp& timeMeterComp) {
        return GetCurrentTimeInMilliseconds() < timeMeterComp.start();
    }

    // ��鵱ǰʱ���Ƿ�δ��ʼ�����Ƿ��ڿ�ʼʱ��֮ǰ��
    static bool IsNotStarted(const TimeMeterComp& timeMeterComp) {
        return IsBeforeStart(timeMeterComp);
    }

    // ����ʱ�������
    static void Reset(TimeMeterComp& timeMeterComp) {
        timeMeterComp.set_start(GetCurrentTimeInMilliseconds());
    }

    inline static uint64_t GetCurrentTimeInMilliseconds() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
};