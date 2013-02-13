/*
 * GeneratorIteratorGeneric.h
 *
 *  Created on: Feb 13, 2013
 *      Author: nathan
 */

#ifndef GENERATORITERATORGENERIC_H_
#define GENERATORITERATORGENERIC_H_

#include <memory>
#include <boost/iterator/iterator_facade.hpp>

template<class GeneratorType>
class GeneratorIteratorGeneric :
	public boost::iterator_facade<
		GeneratorIteratorGeneric<GeneratorType>,
		typename GeneratorType::yield_type,
		boost::single_pass_traversal_tag>
{
private:
	//Internal convenience typedefs
	typedef GeneratorType generator_type;
	typedef typename generator_type::yield_type yield_type;
	typedef std::shared_ptr<yield_type> yield_value_storage_type;

private:
	generator_type* generator;
	yield_value_storage_type yield_value;

private:
	//iterator_facade implementations

	friend class boost::iterator_core_access;
	void increment()
	{
		if(yield_type* p_next = generator->next())
		{
			yield_value = yield_value_storage_type(
					new yield_type(std::move(*p_next)));
		}
		else
		{
			yield_value.reset();
			generator = nullptr;
		}
	}
	yield_type& dereference() const
	{
		return *yield_value;
	}
	bool equal(const GeneratorIteratorGeneric& rhs) const
	{
		return generator == rhs.generator && yield_value == rhs.yield_value;
	}

public:
	GeneratorIteratorGeneric() =default;
	GeneratorIteratorGeneric(const GeneratorIteratorGeneric& cpy) =default;
	GeneratorIteratorGeneric& operator=(const GeneratorIteratorGeneric& cpy) =default;
	~GeneratorIteratorGeneric() =default;

	GeneratorIteratorGeneric(GeneratorIteratorGeneric&& mve):
		generator(mve.generator),
		yield_value(std::move(mve.yield_value))
	{}

	GeneratorIteratorGeneric& operator=(GeneratorIteratorGeneric&& mve)
	{
		generator = mve.generator;
		yield_value.reset();
		yield_value.swap(mve.yield_value);

		return *this;
	}

	/*
	 * Note that this is an invalid iterator. It must be incremented before the
	 * first dereference.
	 */
	explicit GeneratorIteratorGeneric(generator_type& gen):
		generator(&gen)
	{}
};

#endif /* GENERATORITERATORGENERIC_H_ */
