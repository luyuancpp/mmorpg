#pragma once

#include <random>

class Random
{
public:
	Random() : rand(std::random_device{}()) {}

	static Random& GetSingleton()
	{
		thread_local Random singleton;
		return singleton;
	}

	template <typename RandType>
	inline RandType Rand(RandType min_num, RandType max_num)
	{
		std::uniform_int_distribution<RandType> dist(min_num, max_num);
		return dist(rand);
	}

	inline double Rand(double min_num, double max_num)
	{
		std::uniform_real_distribution<double> dist(min_num, max_num);
		return dist(rand);
	}

	inline float Rand(float min_num, float max_num)
	{
		std::uniform_real_distribution<float> dist(min_num, max_num);
		return dist(rand);
	}

private:
	std::mt19937 rand;
};
