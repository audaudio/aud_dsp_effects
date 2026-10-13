// Macro for killing denormalled numbers
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// Based on IS_DENORMAL macro by Jon Watte
// This code is public domain
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm):
// undenormalise is an inline function that reads the bits of the sample
// through memcpy; the original macro read a float through an unsigned int
// pointer, which breaks the strict aliasing rule and is undefined behaviour
// that modern compilers optimise away. The test is the same: a sample whose
// exponent bits are all zero - a denormal, or zero - becomes zero.

#ifndef _denormals_
#define _denormals_

#include <cstdint>
#include <cstring>

inline void undenormalise(float &sample)
{
	std::uint32_t bits;
	std::memcpy(&bits, &sample, sizeof(bits));
	if((bits&0x7f800000u)==0) sample=0.0f;
}

#endif//_denormals_

//ends
