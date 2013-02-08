/*
 * main.cpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  Tests
 */

#include <iostream>
#include <vector>

#include "Generator.h"

template<class Container>
class ContainerWrapper : public Generator<typename Container::value_type>
{
private:
	Container* container;
	typedef typename Container::value_type v_t;

	void run() override
	{
		for(v_t& obj : *container)
			this->yield(obj);
	}
public:
	ContainerWrapper(Container& c):
		container(&c)
	{}
};

class Chain : public Generator<int>
{
public:
	Generator<int>* first;
	Generator<int>* second;

	void run() override
	{
		this->yield_from(*first);
		this->yield_from(*second);
	}

	Chain(Generator<int>& first, Generator<int>& second):
		first(&first),
		second(&second)
	{}
};
typedef std::vector<int> VecInt;

int main(int argc, char **argv)
{
	VecInt vec({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

	ContainerWrapper<VecInt> wrap1(vec);
	ContainerWrapper<VecInt> wrap2(vec);

	Chain chain(wrap1, wrap2);

	while(true)
	{
		std::cout << chain.next() << '\n';
	}
}
