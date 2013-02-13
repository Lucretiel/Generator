#ifndef GENERATORITERATOR_H_
#define GENERATORITERATOR_H_

#include "GeneratorInterface.hpp"
#include "GeneratorIteratorGeneric.hpp"

template<class T>
using GeneratorIterator = GeneratorIteratorGeneric<GeneratorInterface<T> >;

//in C++11, these can be used in place of .begin() and .end()
template<class T>
GeneratorIterator<T> begin(GeneratorInterface<T>& gen)
{
	return ++GeneratorIterator<T>(gen);
}

template<class T>
GeneratorIterator<T> end(GeneratorInterface<T>& gen)
{
	return GeneratorIterator<T>();
}

#endif /* GENERATORITERATOR_H_ */
