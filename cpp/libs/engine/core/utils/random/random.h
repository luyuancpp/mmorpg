#pragma once

#include <random>
#include <concepts>

class Random
{
public:
	Random()
	{
		std::random_device rd;
		std::seed_seq seed{ rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd() };
		rng.seed(seed);
	}

	template <std::integral IntType>
	IntType Rand(IntType min, IntType max)
	{
		std::uniform_int_distribution<IntType> dist(min, max);
		return dist(rng);
	}

	template <std::floating_point FloatType>
	FloatType RandReal(FloatType min, FloatType max)
	{
		std::uniform_real_distribution<FloatType> dist(min, max);
		return dist(rng);
	}

private:
	std::mt19937_64 rng;
};


extern thread_local Random tlsRandom;
