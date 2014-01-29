/*
 * ManagedStack.h
 *
 *  Created on: Feb 8, 2013
 *      Author: nathan
 *
 *  Simple to manage an allocated stack.
 */

#pragma once

#include <cstdlib>
#include <stdexcept>

class ManagedStack
{
private:
	std::size_t _size;
	char* _stack;

public:
	const static std::size_t min_size = 4096;

	explicit ManagedStack(std::size_t size = min_size):
		_size(size > min_size ? size : min_size),
		_stack(static_cast<char*>(std::calloc(size, sizeof(char))))
	{
		if(!_stack) throw std::bad_alloc();
		_stack += _size;
	}

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
			std::free(_stack - _size);
			_stack = nullptr;
		}
	}

	void* stack() const { return _stack; }
	std::size_t size() const { return _size; }
};
