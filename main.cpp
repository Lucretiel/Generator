/*
 * main.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  This is really just demos. The library itself is header-only.
 */

#include <iostream>
#include <deque>
#include <string>

#include "Generator.hpp"

class Foo: public generator::Generator<Foo, int>
{
	friend class Generator::GeneratorCoreAccess;

	void countdown(int x)
	{
		for(; x > 0; --x)
			yield(x);
	}
	void run()
	{
		countdown(3);
		countdown(5);
	}
};

int main()
{
	for(int i: Foo())
	{
		std::cout << i << '\n';
	}
}
