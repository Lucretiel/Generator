/*
 * Generator.hpp
 *
 *  Created on: Feb 7, 2013
 *      Author: nathan
 *
 *  TODO: put something here
 *  TODO: Documentation, examples (besides main.cpp demo)
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

/*
 * Simple class to add RAII semantics to stack allocation.
 *
 * Implementation note: currently, this callocates a block, then returns a
 * pointer to the other side of the block, because stacks grow down. In the
 * future this will be done in more platform-specific ways.
 */
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
	//TODO: platform specific allocation, to ensure the stack growth is correct
	//See mprotect(2), sysconf(2), mmap(2)
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

	//Clear the stack
	void clear()
	{
		std::free(_stack - _size);
		_stack = nullptr;
	}

	//Get stack pointer
	void* stack() const { return _stack; }

	//Get the allocated size of the stack
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

	explicit Holder(const T& obj):
			object(obj)
	{}

	explicit Holder(T&& obj):
			object(std::move(obj))
	{}

	Holder(Holder&&) =default;
	Holder& operator=(Holder&&) =default;
}; //class Holder

} //namespace detail

//TODO: const GeneratorFunc overloads (?)
/*
 * Standard Generator. Creates and manages a context. Execution of this context
 * can be paused and resumed, and the context can send values by reference out
 * of the generator.
 *
 * Generators do NOT own the client functor that is passed to its constructor.
 * This is to allow all generators with the same YieldType to have the same
 * type; this makes many things easier- particularly the type complete of
 * Generator::Yield, which is passed to the functor and doesn't especially need
 * to know the functor type.
 */
template<class YieldType>
class Generator
{
private:
	//Internal typedefs and types

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

	/*
	 * iterator
	 *
	 * See http://www.boost.org/doc/libs/release/libs/iterator/doc/iterator_facade.html
	 * for details on the iterator_facade class. The basic idea is that it
	 * implements all the necessary iterator operations and operators in terms
	 * of simple internal methods. It handles all the iterator tagging used by
	 * STL algorithms, and it even creates a proxy handler for postfix
	 * increment.
	 */
	class iterator :
		public boost::iterator_facade<
			iterator, YieldType, boost::single_pass_traversal_tag>
	{
	public:
		typedef Generator generator_type;

	private:
		Generator* gen;

		friend class boost::iterator_core_access;

		//increment, dereference, and equal are used by iterator_facade
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

	/*
	 * The Yielder class is passed into the client context to allow it to
	 * perform yielding. This design helps to prevent accidentally switching
	 * into the context while already inside it, or vice versa.
	 *
	 * TODO: define explicit behavior for what happens if this happens
	 */
	friend class Yielder;

	class Yield
	{
	private:
		friend class Generator;

		Generator* const gen;

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

		/*
		 * Safely exit the context, as though from a Generator::Stop. This
		 * throws an exception to unwind the stack and call destructors; this
		 * exception is caught by the base method of the context.
		 */
		void done()
		{
			throw ImmediateStop();
		}

		/*
		 * Unsafely exit the context. This exits the context immediately, and
		 * signals the generator to wipe the context (stack, etc). Use this if
		 * you have no destructors or other scope-cleanup you need to do.
		 */
		void exit()
		{
			gen->exit_generator(GeneratorState::Done);
		}
	};

private:
	//State!

	//The stack that the context lives on
	detail::ScopedStack inner_stack;

	//Pointer to the most recently yielded value, which lives in the context
	YieldType* current_value;

	//See the boost context docs for detailed info on these types.
	//data for the execution point of the inner context
	boost::context::fcontext_t* inner_context;

	//data for the execution point of the outer context
	boost::context::fcontext_t outer_context;

private:
	//Wrappers for launching the generator

	//Data to pass to the context
	template<class GeneratorFunc>
	struct ContextBootstrap
	{
		Generator* self;
		GeneratorFunc* func;
	};

	//Type deduction helper
	template<class GeneratorFunc>
	ContextBootstrap<GeneratorFunc> make_bootstrap(
			Generator* self, GeneratorFunc* func)
	{
		return ContextBootstrap<GeneratorFunc>{self, func};
	}

	//initiator function to fit into make_fcontext
	template<class GeneratorFunc>
	static void static_context_base(intptr_t p)
	{
		//Copy the pointers to this stack, just to be safe
		auto bootstrap = *reinterpret_cast<ContextBootstrap<GeneratorFunc>*>(p);
		bootstrap.self->context_base(bootstrap.func);
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
	//Methods for exiting the context

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

	//Collect value, then jump out
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

	/*
	 * Checked jump into generator. checks that the context exists before
	 * entering it, and clears the context if it receives a
	 * GeneratorState::Done
	 */
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

	//Wipe the generator context. Doesn't perform any cleanup in the context.
	void clear_generator_context()
	{
		current_value = nullptr;
		inner_context = nullptr;
		inner_stack.clear();
	}
public:
	/*
	 * Create and launch a new generator context.
	 *
	 * Args:
	 *   GeneratorFunc& func: a reference to a callable object, which should
	 *   have the signature void func(Yield<YieldType>).
	 *
	 *   std::size_t stack_size: The requested size of the stack. The actual
	 *   allocated stack size will not be smaller than some implementation
	 *   defined minimum_size; use the stack_size method to check the actual
	 *   allocated size, and see the detai::ScopedStack class for details on
	 *   this minimum size
	 */
	template<class GeneratorFunc>
	explicit Generator(GeneratorFunc& func, std::size_t stack_size = 0):

		inner_stack(stack_size),
		current_value(nullptr),
		inner_context(boost::context::make_fcontext(
			inner_stack.stack(),
			inner_stack.size(),
			&Generator::static_context_base<GeneratorFunc>))
	{
		auto bootstrap(make_bootstrap(this, &func));
		enter_generator(reinterpret_cast<intptr_t>(&bootstrap));
	}

	//TODO: delegate this call to the template version
	//Function pointer version
	explicit Generator(void (*func_ptr)(Yield),
			std::size_t stack_size = 0):

		inner_stack(stack_size),
		current_value(nullptr),
		inner_context(boost::context::make_fcontext(
			inner_stack.stack(),
			inner_stack.size(),
			&Generator::static_context_base<void(*)(Yield)>))
	{
		auto bootstrap(make_bootstrap(this, &func_ptr));
		enter_generator(reinterpret_cast<intptr_t>(&bootstrap));
	}

	/*
	 * Kill a context. This calls stop().
	 */
	~Generator()
	{
		stop();
	}

	//No copying generators
	Generator(const Generator&) =delete;
	Generator& operator=(const Generator&) =delete;

	Generator(Generator&&) =delete;
	Generator& operator=(Generator&&) =delete;
	/*
	 * TODO: FIX MOVES. Right now there is a potential bug where, when a move
	 * happens, the old this pointer might still be used in some places- on the
	 * context's stack, and in the Yield object passed to the functor.
	 *
	 * Potential solutions:
	 * unique_ptr to some internal shared state
	 * return reference class from functions
	 * delay generator launch; prevent move once launched
	 * RADICAL: prevent moves, but give shared_ptrs to iterators.
	 */

	//Resume the context until the next yield
	void advance()
	{
		resume_generator(YieldBack::Resume);
	}

	/*
	 * Resume the context, then return a pointer to the yielded value. Returns
	 * nullptr if the generator finishes.
	 */
	YieldType* next()
	{
		advance();
		return get();
	}

	/*
	 * Attempt to gracefully stop the generator. This resumes the generator,
	 * but causes an exception to be immediately raised in the context that is
	 * caught by the function at the base function of the context, allowing
	 * destructors and other scope-cleanup to happen. If the context prevents
	 * this exception from reaching the base call, it is still destroyed.
	 */
	void stop()
	{
		resume_generator(YieldBack::Stop);
		clear_generator_context();
	}

	/*
	 * Ungracefully stop the generator. This immediately wipes the inner
	 * context, without giving it the opportunity to run cleanup functions.
	 */
	void kill()
	{
		clear_generator_context();
	}

	//Get a pointer to the most recently yielded object.
	YieldType* get() const
	{
		return current_value;
	}

	//Get a const pointer to the most recently yielded object.
	const YieldType* cget() const
	{
		return current_value;
	}

	//Returns true if this context has stopped.
	bool stopped() const
	{
		return inner_context == nullptr;
	}

	//Iterators
	iterator begin()
	{
		return iterator(this);
	}

	iterator end()
	{
		return iterator();
	}

	//Actual size of the allocated stack
	std::size_t stack_size() const
	{
		return inner_stack.size();
	}
}; // class Generator

/*
 * OwningGenerator is a generator variant that owns the Functor it uses. It
 * has the same interface as Generator, with the addition of the get_functor
 * method
 */
template<class YieldType, class GeneratorFunc>
class OwningGenerator:
		private detail::Holder<GeneratorFunc>,
		public Generator<YieldType>
{
public:
	/*
	 * Copy the functor into internal storage, then create and launch a
	 * generator using it.
	 */
	OwningGenerator(const GeneratorFunc& func, std::size_t stack_size = 0):
		detail::Holder<GeneratorFunc>(func),
		Generator<YieldType>(this->object, stack_size)
	{}

	/*
	 * Move the functor into internal storage, then create and launch a
	 * generator using it.
	 */
	OwningGenerator(GeneratorFunc&& func, std::size_t stack_size = 0):
		detail::Holder<GeneratorFunc>(std::move(func)),
		Generator<YieldType>(this->object, stack_size)
	{}

	OwningGenerator(const OwningGenerator&) =delete;
	OwningGenerator(OwningGenerator&& mve) =default;
	OwningGenerator& operator=(const OwningGenerator&) =delete;
	OwningGenerator& operator=(OwningGenerator&&) =delete;

	/*
	 * Get the underlying functor object.
	 */
	GeneratorFunc& get_func() const
	{
		return this->object;
	}
}; //class OwningGenerator

template<class YieldType>
using Yield = typename Generator<YieldType>::Yield;

//Disabled until move issues are resolved

/*
 * make_owning_generator- type deduction for OwningGenerator. Stick a lambda in
 * there!
 */
//template<class YieldType, class GeneratorFunc>
//OwningGenerator<YieldType, GeneratorFunc> make_owning_generator(
//		GeneratorFunc&& func, std::size_t stack_size = 0)
//{
//	return OwningGenerator<YieldType, GeneratorFunc>(
//		std::forward<GeneratorFunc>(func), stack_size);
//}

} // namespace generator
