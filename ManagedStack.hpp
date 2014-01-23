/*
 * ManagedStack.h
 *
 *  Created on: Feb 8, 2013
 *      Author: nathan
 *
 *  Simple class to add RAII to a stack allocator.
 */

#ifndef MANAGEDSTACK_H_
#define MANAGEDSTACK_H_

#include <cstdlib>

class ManagedStack
{
private:
	std::size_t _size;
	void* _stack;

public:
	ManagedStack(std::size_t size):
		_size(size),
		_stack(std::malloc(size))
	{}

	~ManagedStack()
	{
		clear();
	}

	//No copying
	ManagedStack(const ManagedStack&) =delete;
	ManagedStack& operator=(const ManagedStack&) =delete;

	//Moving
	ManagedStack(ManagedStack&& mve):
		_size(mve._size),
		_stack(mve._stack)
	{
		mve._stack = nullptr;
	}
	ManagedStack& operator=(ManagedStack&& mve)
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
		if(_stack)
		{
			std::free(_stack);
			_stack = nullptr;
		}
	}

	void* stack() const { return _stack; }
	std::size_t size() const { return _size; }
};

#endif /* MANAGEDSTACK_H_ */
