/*
 * GeneratorInterface.h
 *
 *  Created on: Feb 13, 2013
 *      Author: nathan
 *
 *  GeneratorInterface: For when you don't need the overhead of a context
 *  switch.
 */

#ifndef GENERATORINTERFACE_H_
#define GENERATORINTERFACE_H_

template<class YieldType>
class GeneratorInterface
{
public:
	typedef YieldType yield_type;
	typedef yield_type* yield_ptr_type;

	virtual ~GeneratorInterface() {}

	virtual yield_type* next() =0;
};


#endif /* GENERATORINTERFACE_H_ */
