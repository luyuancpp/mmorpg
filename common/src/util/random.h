#ifndef COMMON_SRC_RANDOM_RANDOM_H_
#define COMMON_SRC_RANDOM_RANDOM_H_

#include <random>

namespace common
{

class Random
{
public:
    Random() { rand.seed(std::random_device{}()); }

    static Random& GetSingleton()
    {
        thread_local Random singleton;
        return singleton;
    }

    template <typename RandType>
    inline decltype(auto) Rand(RandType min_num, RandType max_num)
    {
        std::uniform_int_distribution<RandType> dist(min_num, max_num);
        return dist(rand);
    }

    inline decltype(auto) Rand(double min_num, double max_num)
    {
        std::uniform_real_distribution<double> dist(min_num, max_num);
        return dist(rand);
    }

    inline decltype(auto) Rand(float min_num, float max_num)
    {
        std::uniform_real_distribution<double> dist(min_num, max_num);
        return dist(rand);
    }
private:
    std::mt19937 rand;
};

}//namespace common

#endif//COMMON_SRC_RANDOM_RANDOM_H_

