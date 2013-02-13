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
#include "Generator.h"
#include "GeneratorIterator.h"

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

class VerboseGenerator : public Generator<Verbose>
{
private:
	void run()
	{
		std::cout << "Generator: yielding from stack\n";
		Verbose verbose;
		yield(verbose);

		std::cout << "Generator: yielding from nameless temp\n";
		yield(Verbose());
	}
};

int main()
{
	VerboseGenerator gen1;

	std::cout << "main: two simple yields\n";
	std::cout << "main: yield 1\n";
	Verbose v1(*gen1.next());
	std::cout << "main: yield 2\n";
	Verbose v2(*gen1.next());

	{
		VerboseGenerator gen2;
		std::cout << "main, subscope: only one yield, followed by out of scope\n";
		std::cout << "main, subscope: yield 1\n";
		Verbose v(*gen2.next());
		std::cout << "main, subscope: exiting\n";
	}
	std::cout << "main: exiting\n";
}
