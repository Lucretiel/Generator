/*
 * GeneratorInterface.h
 *
 *  Created on: Feb 13, 2013
 *      Author: nathan
 */

#ifndef GENERATORINTERFACE_H_
#define GENERATORINTERFACE_H_

template<class YieldType>
class GeneratorInterface
{
public:
	typedef YieldType yield_type;

	virtual ~GeneratorInterface() {}

	virtual yield_type* next() =0;
};


#endif /* GENERATORINTERFACE_H_ */
