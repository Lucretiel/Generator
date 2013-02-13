/*
 * main.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  Tests
 */

#include <iostream>
#include <vector>
#include "Generator.h"
#include "GeneratorIterator.h"

class Range : public GeneratorInterface<int>
{
private:
	int val;
	int stop;
	int step;

	bool increment()
	{
		val += step;
		return val >= stop;
	}

public:
	Range(int start, int stop, int step = 1):
		val(start),
		stop(stop),
		step(step)
	{
		val -= step;
	}
	Range(int stop):
		Range(0, stop, 1)
	{}

	Range::yield_type next() override
	{
		if(increment())
			throw Range::generator_finished();
		return val;
	}
};

template<class T>
class Chain : public Generator<T>
{
private:
	std::vector<GeneratorInterface<T>* > gens;

	void add_generator(GeneratorInterface<T>* gen)
	{
		gens.push_back(gen);
	}

	void add_generators()
	{}

	template<typename... Args>
	void add_generators(GeneratorInterface<T>* gen, Args... rest)
	{
		add_generator(gen);
		add_generators(rest...);
	}

	void run() override
	{
		for(auto gen : gens)
		{
			this->yield_from(*gen);
		}
	}

public:
	template<typename... Args>
	Chain(Args... args)
	{
		add_generators(args...);
	}
};

int main()
{
	Range range1(0, 10);
	Range range2(5, 15);
	Range range3(0, 20, 7);

	Chain<int> chain(&range1, &range2, &range3);


	for(int i : chain)
	{
		std::cout << i << '\n';
	}
}
