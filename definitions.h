/*
 * definitions.h
 *
 *  Created on: Feb 8, 2013
 *      Author: nathan
 */

#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

#define CPPELEVEN ((__cplusplus) > (201100L))
#define NOT_CPPELEVEN (!(CPPELEVEN))

#if NOT_CPPELEVEN
#define nullptr NULL
#endif

#if NOT_CPPELEVEN
#include <boost/noncopyable.hpp>
#endif

#endif /* DEFINITIONS_H_ */
