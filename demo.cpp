/*
 * demo.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  This is just demos. The library itself is header-only.
 */

#include <string>
#include <iostream>
#include <vector>
#include "Generator.hpp"

using namespace generator;

void fibonacci(Yield<unsigned> yield)
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
}

struct FunctionObject
{
	int x;

	void operator()(Yield<int> yield)
	{
		while(true)
		{
			std::cout << "FunctionObject: this->x: " << this->x << '\n';
			std::cout << "FunctionObject: Creating local copy of x\n";
			int x = this->x;
			std::cout << "FunctionObject: yielding x+2\n";
			yield(x+2);
			std::cout << "FunctionObject: yielding x+3\n";
			yield(x+3);
		}
	}
};

int main()
{
	//Function pointer version
	std::cout << "fibonacci demo. All the fibonacci numbers less than 100.\n";
	Generator<unsigned> fib(&fibonacci);
	for(unsigned i : fib)
	{
		if(i >= 100) break;
		std::cout << i << '\n';
	}

	//Object-by-reference version. Also notice the preserved local state in gen.
	std::cout << "Object-by-reference demo. Shows generator control flow and"
			"state preservation.\n";
	std::cout << "Creating FunctionObject with value 10\n";
	FunctionObject obj;
	obj.x = 10;

	std::cout << "Creating generator\n";
	Generator<int> gen(obj);

	auto advance = [&gen](){std::cout << "main: advancing\n"; gen.advance();};
	auto get = [&gen](){std::cout << "main: getting value: " << *gen.get() << '\n';};

	/*
	 * No initial advance- The generator is implicitly started when constructed.
	 * This is modeled after std::thread, and makes the generator behavior more
	 * consistent- it is never in a "pre-started" state. It also makes iterators
	 * make more sense.
	 */
	get(); advance(); get();

	std::cout << "main: Setting x to 20\n";
	obj.x = 20;

	//yield 22
	advance(); get();

	std::cout << "main: Setting x to 30\n";
	obj.x = 30;

	//yield 23, update with 30, yield 32
	advance(); get(); advance(); get();

	//Lambda version
	//Also notice the yield-by-reference
	std::cout << "yield-by-reference demo. Enter text and see it printed.\n";
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
