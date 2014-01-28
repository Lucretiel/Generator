/*
 * GeneratorIterator.h
 *
 *  Created on: Feb 15, 2013
 *      Author: nathan
 */

#ifndef GENERATORITERATOR_H_
#define GENERATORITERATOR_H_

#include <boost/iterator/iterator_facade.hpp>

template<class Generator>
class GeneratorIterator :
		public boost::iterator_facade<
			GeneratorIterator<Generator>,
			typename Generator::yield_type,
			boost::single_pass_traversal_tag>
{
public:
	typedef Generator generator_type;
	typedef typename generator_type::yield_type yield_type;

	typedef typename GeneratorIterator::value_type value_type;
	typedef typename GeneratorIterator::reference reference;

private:
	generator_type* gen;

	friend class boost::iterator_core_access;

	void increment()
	{
		if(gen)
		{
			gen->advance();
			if(gen->stopped())
				gen = nullptr;
		}
	}
	/*
	 * Note that this behavior for increment invalidates old iterators, meaning
	 * that *it++ doesn't work.
	 */

	reference dereference() const
	{
		return *gen->get();
	}

	bool equal(const GeneratorIterator& rhs) const
	{
		return gen == rhs.gen;
	}

public:

	GeneratorIterator(Generator* gen=nullptr):
		gen(gen)
	{}
};

#endif /* GENERATORITERATOR_H_ */
