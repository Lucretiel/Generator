#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <memory>
#include <boost/iterator/iterator_facade.hpp>

template<class GeneratorType, typename YieldType>
class GeneratorIteratorGeneric :
	public boost::iterator_facade<
		GeneratorIteratorGeneric,
		YieldType,
		boost::single_pass_traversal_tag
{
private:
	//Internal convenience typedefs
	typedef GeneratorType generator_type;
	typedef typename generator_type::generator_finished generator_finished;
	typedef YieldType yield_type;

private:
	generator_type* generator;
	std::shared_ptr<yield_type> yield_value;

private:
	//iterator_facade implementations
	
	friend class boost::iterator_core_access;
	void increment()
	{
		if(generator)
		{
			try
			{
				yield_value = make_shared<yield_type>(generator->next());
			}
			catch(generator_finished& e)
			{
				yield_value.reset();
				generator = nullptr;
			}
		}
	}
	yield_type& dereference()
	{
		if(!yield_value)
			increment()
		return yield_value.get();
	}
	bool equal(const GeneratorIteratorGeneric& rhs) const
	{
		return generator == rhs.generator && yield_value == rhs.yield_value;
	}

public:
	GeneratorIteratorGeneric() =default;
	GeneratorIteratorGeneric(GeneratorIteratorGeneric& cpy) =default;
	GeneratorIteratorGeneric& operator=(GeneratorIteratorGeneric& cpy) =default;
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
	}

	explicit GeneratorIteratorGeneric(generator_type* gen):
		generator(gen)
	{}
};

#endif /* GENERATOR_H_ */
