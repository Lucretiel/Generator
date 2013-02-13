/*
 * main.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  Tests
 */

#include <iostream>
#include <deque>
#include "Generator.hpp"
#include "GeneratorIterator.hpp"

class Verbose
{
public:
	Verbose()
	{
		std::cout << this << " is being constructed!\n";
	}
	~Verbose()
	{
		std::cout << this << " is being destructed!\n";
	}
	Verbose(const Verbose& other)
	{
		std::cout << this << " is being copy constructed from " << &other << "!\n";
	}
	Verbose(Verbose&& other)
	{
		std::cout << this << " is being move constructed from " << &other << "!\n";
	}
	Verbose& operator=(const Verbose& other)
	{
		std::cout << this << " is being assigned from " << &other << "!\n";
		return *this;
	}
	Verbose& operator=(Verbose&& other)
	{
		std::cout << this << " is being move assigned from " << &other << "!\n";
		return *this;
	}
};

GENERATOR(VerboseGenerator, Verbose)
{
	std::cout << "Generator: yielding from nameless temp\n";
	YIELD(Verbose());

	std::cout << "Generator: yielding from stack\n";
	Verbose verbose;
	YIELD(verbose);
}

void test1()
{
	std::cout << "test1: two simple yields\n";
	VerboseGenerator gen;
	std::cout << "main: yield 1\n";
	Verbose v1(*gen.next());
	std::cout << "main: yield 2\n";
	Verbose v2(*gen.next());
}

void test2()
{
	std::cout << "test2: new style for loop\n";
	VerboseGenerator gen;
	for(Verbose& v : gen)
	{
		std::cout << "test2, for-loop: object is " << &v << "\n";
	}
}

int main()
{
	test1();
	std::cout << '\n';
	test2();
	std::cout << "main: exiting\n";
}
