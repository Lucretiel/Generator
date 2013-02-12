#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <memory>
#include <iterator>

template<class GeneratorType>
class GeneratorIteratorGeneric
{
private:
	//Internal convenience typedefs
	typedef GeneratorType generator_type;
	typedef typename GeneratorType::generator_finished generator_finished;

public:
	//Iterator traits. Manually rolled because there's no difference_type.
	typedef typename GeneratorType::value_type value_type;
	typedef value_type* pointer;
	typedef value_type& reference;
	typedef std::forward_iterator_tag iterator_category;

private:
	generator_type* generator;
	std::shared_ptr<value_type> yield_value;

	void increment()
	{
		if(generator)
		{
			try
			{
				yield_value = make_shared<value_type>(generator->next());
			}
			catch(generator_finished& e)
			{
				yield_value.reset();
				generator = nullptr;
			}
		}
	}
	value_type* get()
	{
		if(!yield_value)
			increment()
		return yield_value.get();
	}
	bool compare(const GeneratorIterator& rhs)
	{
		return generator == rhs.generator && yield_value == rhs.yield_value;
	}

public:
	GeneratorIterator() =default;
	GeneratorIterator(GeneratorIterator& cpy) =default;
	GeneratorIterator& operator=(GeneratorIterator& cpy) =default;

	GeneratorIterator(GeneratorIterator&& mve):
		generator(mve.generator),
		yield_value(std::move(mve.yield_value))
	{}

	GeneratorIterator& operator=(GeneratorIterator&& mve)
	{
		generator = mve.generator;
		yield_value.reset();
		yield_value.swap(mve.generator);
	}

	~GeneratorIterator() {}

	GeneratorIterator(generator_type* gen):
		generator(gen)
	{}

	//POINTER STUFF
	value_type& operator*()
	{
		return *get();
	}
	value_type* operator->()
	{
		return get();
	}
	GeneratorIterator& operator++()
	{
		increment();
	}
	//postfix
	GeneratorIterator operator++(int)
	{
		GeneratorIterator it(*this);
		increment();
		return it;
	}
	bool operator==(const GeneratorIterator& rhs)
	{
		return compare(rhs);
	}
	bool operator!=(const GeneratorIterator& rhs)
	{
		return !compare(rhs);
	}
};

#endif /* GENERATOR_H_ */
