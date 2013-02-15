/*
 * GeneratorIterator.hpp
 *
 *  Created on: Feb 15, 2013
 *      Author: nathan
 */

#ifndef GENERATORITERATOR_HPP_
#define GENERATORITERATOR_HPP_

#include "GeneratorIteratorGeneric.hpp"
#include "Generator.hpp"

template<class T>
GeneratorIterator<T> generator_begin(T& gen)
{
	return GeneratorIterator<T>(gen);
}

template<class T>
GeneratorIterator<T> generator_end(T&)
{
	return GeneratorIterator<T>();
}

template<class T>
GeneratorIterator<GeneratorInterface<T> > begin(GeneratorInterface<T>& gen)
{
	return generator_begin(gen);
}

template<class T>
GeneratorIterator<GeneratorInterface<T> > end(GeneratorInterface<T>& gen)
{
	return generator_end(gen);
}



#endif /* GENERATORITERATOR_HPP_ */
