/*
 * GeneratorIterator.h
 *
 *  Created on: Feb 15, 2013
 *      Author: nathan
 */

#ifndef GENERATORITERATOR_H_
#define GENERATORITERATOR_H_

#include <boost/iterator/iterator_facade.hpp>

struct io_iterator_tag :
		public std::input_iterator_tag,
		public std::output_iterator_tag
{};

template<class Generator>
class GeneratorIterator :
		public boost::iterator_facade<
			GeneratorIterator<Generator>,
			typename Generator::yield_type,
			io_iterator_tag>
{
public:
	typedef Generator generator_type;
	typedef typename generator_type::yield_type yield_type;

	typedef typename GeneratorIterator::value_type value_type;
	typedef typename GeneratorIterator::reference reference;

private:
	yield_type* current;
	generator_type* gen;
	unsigned long increments;

	friend class boost::iterator_core_access;

	void increment()
	{
		if(gen)
			++increments;
	}

	void non_const_update()
	{
		while(increments && gen)
		{
			--increments;
			current = gen->next();
			if(!current)
				gen = nullptr;
		}
	}

	void update() const
	{
		const_cast<GeneratorIterator*>(this)->non_const_update();
	}

	reference dereference() const
	{
		update();
		return *current;
	}

	bool equal(const GeneratorIterator& rhs) const
	{
		update();
		rhs.update();
		return gen == rhs.gen && current == rhs.current;
	}

public:
	GeneratorIterator():
		current(nullptr),
		gen(nullptr),
		increments(0)
	{}

	GeneratorIterator(Generator& gen):
		current(nullptr),
		gen(&gen),
		increments(1)
	{}
};

#endif /* GENERATORITERATOR_H_ */
