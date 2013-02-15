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

class VerboseGenerator : public Generator<VerboseGenerator, Verbose>
{
public:
	void run()
	{
		std::cout << "Generator: yielding from nameless temp\n";
		yield(Verbose());

		std::cout << "Generator: yielding from stack\n";
		Verbose v;
		yield(v);

		std::cout << "Generator: yielding from container of 3 items\n";
		std::deque<Verbose> v_d(3);
		yield_from(v_d);

		std::cout << "Generator: exiting\n";
	}
};

void test1()
{
	std::cout << "test1: five simple yields\n";
	VerboseGenerator gen;
	std::cout << "test1: yield 1\n";
	Verbose v1(*gen.next());
	std::cout << "test1: yield 2\n";
	Verbose v2(*gen.next());
	std::cout << "test1: yield 3\n";
	Verbose v3(*gen.next());
	std::cout << "test1: yield 4\n";
	Verbose v4(*gen.next());
	std::cout << "test1: yield 5\n";
	Verbose v5(*gen.next());
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

void test3()
{
	std::cout << "test3: while loop\n";
	VerboseGenerator gen;
	while(Verbose* p_v = gen.next())
	{
		std::cout << "test3, while-loop: got " << p_v << "from next\n";
	}
}

int main()
{
	test1();
	std::cout << '\n';
	test2();
	std::cout << '\n';
	test3();
	std::cout << "main: exiting\n";
}
