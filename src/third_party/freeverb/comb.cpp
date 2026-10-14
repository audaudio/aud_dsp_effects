// Comb filter implementation
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm): the
// constructor starts without a buffer and with zero feedback and damping,
// so that mute() before setbuffer() touches nothing; setbuffer() and mute()
// rewind the index, and mute() clears the damping filter too.

#include "comb.hpp"

comb::comb()
{
	feedback = 0;
	filterstore = 0;
	damp1 = 0;
	damp2 = 1;
	buffer = 0;
	bufsize = 0;
	bufidx = 0;
}

void comb::setbuffer(float *buf, int size)
{
	buffer = buf;
	bufsize = size;
	bufidx = 0;
}

void comb::mute()
{
	for (int i=0; i<bufsize; i++)
		buffer[i]=0;
	filterstore = 0;
	bufidx = 0;
}

void comb::setdamp(float val)
{
	damp1 = val;
	damp2 = 1-val;
}

float comb::getdamp()
{
	return damp1;
}

void comb::setfeedback(float val)
{
	feedback = val;
}

float comb::getfeedback()
{
	return feedback;
}

// ends
