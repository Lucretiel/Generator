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
#include "GeneratorIterator.hpp"

struct Var
{
	std::string x;
};

class VarGenerator : public Generator<VarGenerator, Var>
{
public:
	void run()
	{
		std::cout << "Generator: begin\n";
		std::cout << "Creating new var with 'Hello'\n";
		Var v{"hello"};
		std::cout << "Contents of var: " << v.x << '\n';
		std::cout << "Yielding var\n";
		yield(v);
		std::cout << "Contents of var: " << v.x << '\n';

		std::cout << "Generator: exit\n";
	}
};

void test1()
{
	VarGenerator gen;
	Var* p_v;

	std::cout << "test1: begin\n";

	do
	{
		std::cout << "Beginning loop\n";
		p_v = gen.next();

		std::cout << "Got " << p_v << " from next\n";
		if(p_v)
		{
			std::cout << "Not empty! contents: " << p_v->x << '\n';
			std::cout << "Setting to 'World'\n";
			p_v->x = "World";
		}
		std::cout << "Ending loop\n";
	} while(p_v);
}

class BasicGenerator : public Generator<BasicGenerator, int>
{
public:
	void run()
	{
		yield(1);
		yield(2);
		yield(3);
		yield(4);
	}
};

void test2()
{
	BasicGenerator gen;
	auto it = begin(gen);
	auto it2 = end(gen);

	while(it != it2)
	{
		std::cout << *it++ << '\n';
	}
}

void test3()
{

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
