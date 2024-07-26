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
	inline RandType Rand(RandType minNum, RandType maxNum)
	{
		std::uniform_int_distribution<RandType> dist(minNum, maxNum);
		return dist(rand);
	}

	inline double Rand(double minNum, double maxNum)
	{
		std::uniform_real_distribution<double> dist(minNum, maxNum);
		return dist(rand);
	}

	inline float Rand(float minNum, float maxNum)
	{
		std::uniform_real_distribution<float> dist(minNum, maxNum);
		return dist(rand);
	}

private:
	std::mt19937 rand;
};
