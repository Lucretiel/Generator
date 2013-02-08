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

class Filter : public Generator<int, 512>
{
private:
	std::vector<int>* container;
	bool (*func)(int);

	void run()
	{
		for(int item : *container)
		{
			if(func(item))
				yield(item);
		}
	}

public:
	Filter(std::vector<int>& container, bool(*func)(int)):
		container(&container),
		func(func)
	{}
};

bool divby3(int x)
{
	return x%3 == 0;
}

int main(int argc, char **argv)
{
	std::vector<int> vec;
	for(int i = 0; i < 20; ++i)
		vec.push_back(i);

	Filter filter(vec, &divby3);

	for(int i : vec)
	{
		std::cout << filter.next() << '\n';
	}
}
