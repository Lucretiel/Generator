/*
 * Generator.h
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 */

#pragma once

#include <cstdlib>
#include <stdexcept>
#include <utility>

#include <boost/context/all.hpp>
#include <boost/iterator/iterator_facade.hpp>

namespace generator
{
namespace detail
{

//Simple class to add RAII semantics to stack allocation
class ScopedStack
{
private:
	std::size_t _size;
	char* _stack;

public:
	//TODO: pick this value more intelligently
	const static std::size_t min_size = 1024 * 8; //8KB, 2 pages on my system

	//TODO: allocate at page boundary
	//TODO: guard page
	explicit ScopedStack(std::size_t size = 0):
		_size(size > min_size ? size : min_size),
		_stack(static_cast<char*>(std::calloc(_size, sizeof(char))))
	{
		if(!_stack) throw std::bad_alloc();
		_stack += _size;
	}

	~ScopedStack()
	{
		clear();
	}

	//No copying
	ScopedStack(const ScopedStack&) =delete;
	ScopedStack& operator=(const ScopedStack&) =delete;

	//Moving
	ScopedStack(ScopedStack&& mve):
		_size(mve._size),
		_stack(mve._stack)
	{
		mve._stack = nullptr;
	}
	ScopedStack& operator=(ScopedStack&& mve)
	{
		if(&mve == this) return *this;

		clear();
		_size = mve._size;
		_stack = mve._stack;

		mve._stack = nullptr;
		return *this;
	}

	void clear()
	{
		std::free(_stack - _size);
		_stack = nullptr;
	}

	void* stack() const { return _stack; }
	std::size_t size() const { return _size; }
}; // class ManagedStack

/*
 * Needed for OwningGenerator to ensure correct order of initialization. To keep
 * the OwningGenerator implementation simple, it inherits from Generator-
 * otherwise we'd need to reimplement Generator's public interface. However,
 * base classes are always initialzied before members, and we need to make sure
 * the owned functor is initialized before the generator.
 */
template<class T>
class Holder
{
public:
	T object;

	explicit Holder(T& obj):
			object(obj)
	{}

	explicit Holder(T&& obj):
			object(std::move(obj))
	{}

	Holder(Holder&&) =default;
	Holder& operator=(Holder&&) =default;
}; //class Holder

} //namespace detail

//TODO: const GeneratorFunc overloads
/*
 * Standard Generator. Creates and manages a context. Execution of this context
 * can be paused and resumed, and the context can send values by reference out
 * of the generator.
 *
 * Generators do NOT own the client functor that is passed to it. This is to
 * allow all generators
 */
template<class YieldType>
class Generator
{
private:
	//Internal typedefs and types
	typedef boost::context::fcontext_t context_type;

	//Return states to give to the generator
	enum class YieldBack : intptr_t {Resume, Stop};

	//Return states to get from the generator
	enum class GeneratorState : intptr_t {Continue, Done};

	//thrown from yield() when the context must be immediately destroyed.
	class ImmediateStop {};

public:
	//public typedefs and types.
	typedef YieldType yield_type;
	typedef YieldType* yield_ptr_type;

	//Iterator
	class iterator :
		public boost::iterator_facade<
			iterator, YieldType, boost::single_pass_traversal_tag>
	{
	public:
		typedef Generator generator_type;

	private:
		Generator* gen;

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

		YieldType& dereference() const
		{
			return *gen->get();
		}

		bool equal(const iterator& rhs) const
		{
			return gen == rhs.gen;
		}

	public:
		iterator(Generator* gen=nullptr):
			gen(gen)
		{}
	};

	//Yielder. Passed into contexts to enable yielding
	friend class Yielder;

	class Yield
	{
	private:
		friend class Generator;

		//TODO: determine if a reference is appropriate here
		Generator* gen;

		explicit Yield(Generator* generator):
			gen(generator)
		{}

	public:
		//Yield an object
		template<class T>
		void operator()(T&& object)
		{
			gen->yield(&object);
		}

		//
		void exit()
		{
			gen->exit_generator(GeneratorState::Done);
		}

		//SAFE. will throw an exception that will be caught by the base of the context
		void done()
		{
			throw ImmediateStop();
		}
	};

private:
	//State!
	detail::ScopedStack inner_stack;
	YieldType* current_value;
	context_type* inner_context;
	context_type outer_context;

private:
	//Wrappers for launching the generator

	//Data to pass to the context
	template<class GeneratorFunc>
	struct ContextBootstrap
	{
		Generator* self;
		GeneratorFunc* func;
	};

	//initiator function to fit into make_fcontext
	template<class GeneratorFunc>
	static void static_context_base(intptr_t p)
	{
		auto bootstrap = reinterpret_cast<ContextBootstrap<GeneratorFunc>*>(p);
		bootstrap->self->context_base(bootstrap->func);
	}

	/*
	 * Primary wrapper. Launches function, handles ImmediateStop exceptions,
	 * and cleans up on an exit.
	 */
	template<class GeneratorFunc>
	void context_base(GeneratorFunc* func)
	{
		try { (*func)(Yield(this)); }
		catch(ImmediateStop&) {}

		current_value = nullptr;
		exit_generator(GeneratorState::Done);
	}

private:
	//Context Exit

	//core wrapper for jump_fcontext call out of the generator
	YieldBack jump_out_of_generator(GeneratorState state)
	{
		return static_cast<YieldBack>(
			boost::context::jump_fcontext(
				inner_context, &outer_context, static_cast<intptr_t>(
					state)));
	}

	//Jump out of a generator. Checks for a stop instruction
	void exit_generator(GeneratorState state)
	{
		if(jump_out_of_generator(state) == YieldBack::Stop)
		{
			throw ImmediateStop();
		}
	}

	//Collect value and exit
	void yield(YieldType* obj)
	{
		current_value = obj;
		exit_generator(GeneratorState::Continue);
	}

private:
	//Context Entry

	//core wrapper for jump_fcontext call into the generator
	GeneratorState jump_into_generator(intptr_t val)
	{
		return static_cast<GeneratorState>(
			boost::context::jump_fcontext(
				&outer_context, inner_context, val));
	}

	//Checked jump into generator
	void enter_generator(intptr_t val)
	{
		//If the context exists, enter it
		if(inner_context)
		{
			//If the generator finishes, clean it up
			if(jump_into_generator(val) == GeneratorState::Done)
			{
				clear_generator_context();
			}
		}
	}

	//Checked jump into the generator with a return state
	void resume_generator(YieldBack yield_back)
	{
		enter_generator(static_cast<intptr_t>(yield_back));
	}

	//Wipe the generator context. UNSAFE
	void clear_generator_context()
	{
		current_value = nullptr;
		inner_context = nullptr;
		inner_stack.clear();
	}
public:
	template<class GeneratorFunc>
	explicit Generator(GeneratorFunc& func, std::size_t stack_size = 0):

		inner_stack(stack_size),
		current_value(nullptr),
		inner_context(boost::context::make_fcontext(
			inner_stack.stack(),
			inner_stack.size(),
			&Generator::static_context_base<GeneratorFunc>))
	{
		ContextBootstrap<GeneratorFunc> bootstrap = {this, &func};
		enter_generator(reinterpret_cast<intptr_t>(&bootstrap));
	}

	~Generator()
	{
		stop();
	}

	Generator(const Generator&) =delete;
	Generator& operator=(const Generator&) =delete;

	Generator(Generator&&) =default;
	Generator& operator=(Generator&&) =default;

	void advance()
	{
		resume_generator(YieldBack::Resume);
	}

	YieldType* next()
	{
		advance();
		return get();
	}

	void stop()
	{
		resume_generator(YieldBack::Stop);

		/*
		 * Normally resume_generator will handle this. However, if the context
		 * is naughty and prevents the exception from reaching the base, we
		 * still have to kill it.
		 */
		clear_generator_context();
	}

	YieldType* get()
	{
		return current_value;
	}

	const YieldType* get() const
	{
		return cget();
	}

	const YieldType* cget() const
	{
		return current_value;
	}

	bool stopped() const
	{
		return inner_context == nullptr;
	}

	iterator begin()
	{
		return iterator(this);
	}

	iterator end()
	{
		return iterator();
	}
}; // class Generator

/*
 * OwningGenerator is a generator variant that owns the Functor it uses.
 */
template<class YieldType, class GeneratorFunc>
class OwningGenerator:
		private detail::Holder<GeneratorFunc>,
		public Generator<YieldType>
{
public:
	OwningGenerator(GeneratorFunc& func, std::size_t stack_size = 0):
		detail::Holder<GeneratorFunc>(func),
		Generator<YieldType>(this->object, stack_size)
	{}

	OwningGenerator(GeneratorFunc&& func, std::size_t stack_size = 0):
		detail::Holder<GeneratorFunc>(std::move(func)),
		Generator<YieldType>(this->object, stack_size)
	{}

	OwningGenerator(const OwningGenerator&) =delete;
	OwningGenerator(OwningGenerator&& mve) =default;
	OwningGenerator& operator=(const OwningGenerator&) =delete;
	OwningGenerator& operator=(OwningGenerator&&) =default;
}; //class OwningGenerator

template<class YieldType>
using Yield = typename Generator<YieldType>::Yield;

template<class YieldType, class GeneratorFunc>
OwningGenerator<YieldType, GeneratorFunc> make_owning_generator(
		GeneratorFunc&& func, std::size_t stack_size = 0)
{
	return OwningGenerator<YieldType, GeneratorFunc>(
		std::forward<GeneratorFunc>(func), stack_size);
}

} // namespace generator
