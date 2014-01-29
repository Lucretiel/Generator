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

class Foo: public Generator<Foo, int>
{
	friend class Generator::GeneratorCoreAccess;
	void run()
	{
		yield(1);
		yield(2);
		yield(1);
		yield(3);
		yield(1);
		yield(4);
	}
};

int main()
{
	Foo foo;

	for(int i: foo)
	{
		std::cout << i << '\n';
	}
}
