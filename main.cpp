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
	auto fibonacci = make_owning_generator<unsigned>(
	[](Yield<unsigned> yield)
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
	});

	for(unsigned i = *fibonacci.get(); i < 1000; i = *fibonacci.next())
	{
		std::cout << i << '\n';
	}

	auto printer = make_owning_generator<std::string>(
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
