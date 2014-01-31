/*
 * main.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  This is really just demos. The library itself is header-only.
 */

#include <string>
#include <iostream>
#include "Generator.hpp"

using namespace generator;

int main()
{
	auto fib = [](Yield<unsigned> yield)
	{
		unsigned a = 0;
		unsigned b = 1;
		while(true)
		{
			yield(a);
			unsigned tmp = b;
			b = a + b;
			a = tmp;
		}
	};

	for(unsigned i : Generator<unsigned>(fib))
	{
		std::cout << i << '\n';
		if(i > 50) break;
	}

	for(unsigned j : Generator<unsigned>(fib))
	{
		std::cout << j << '\n';
		if(j > 50) break;
	}

	Generator<std::string> printer(
	[](Yield<std::string> yield)
	{
		std::string str;
		while(str != "Stop")
		{
			yield(str);
			std::cout << "Got " << str << '\n';
		}
		std::cout << "Stopping\n";
	});

	for(std::string& str : printer)
	{
		std::cout << "Enter a string (Stop to stop): ";
		std::getline(std::cin, str);
	}
}
