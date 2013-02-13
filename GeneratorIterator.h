#ifndef MAKE_ITERATOR_H_
#define MAKE_ITERATOR_H_

#include "Generator.h"
#include "GeneratorIteratorGeneric.h"


template<class T>
using GeneratorIterator =
	GeneratorIteratorGeneric<
		Generator<T>,
		typename Generator<T>::value_type>;
		

//in C++11, these can be used in place of .begin() and .end()
template<class T>
GeneratorIterator<T> begin(Generator<T>& gen)
{
	return GeneratorIterator<T>(&gen);
}

template<class T>
GeneratorIterator<T> end(Generator<T>& gen)
{
	return GeneratorIterator<T>();
}

#endif /* MAKE_ITERATOR_H_ */
