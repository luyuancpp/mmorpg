#pragma once

#include <random>

class Random
{
public:
	Random() : rand(std::random_device{}()) {}

	template <typename RandType>
	inline RandType Rand(RandType minNum, RandType maxNum)
	{
		std::uniform_int_distribution<RandType> dist(minNum, maxNum);
		return dist(rand);
	}

	inline double RandDobule(double minNum, double maxNum)
	{
		std::uniform_real_distribution<double> dist(minNum, maxNum);
		return dist(rand);
	}

	inline float RandFloat(float minNum, float maxNum)
	{
		std::uniform_real_distribution<float> dist(minNum, maxNum);
		return dist(rand);
	}

private:
	std::mt19937_64 rand;
};

thread_local Random tls_rand;
