/*
 * main.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  Tests
 */

#include <iostream>
#include "GeneratorIterator.h"

class Range5 : public Generator<int>
{
public:
	Range5():
		Generator(2)
	{}
	void run()
	{
		this->yield(0);
		this->yield(1);
		this->yield(2);
		this->yield(3);
		this->yield(4);
	}
};

int main()
{
	Range5 range;

	for(int i : range)
	{
		std::cout << i << '\n';
	}
}
