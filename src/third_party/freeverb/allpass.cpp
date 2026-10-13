// Allpass filter implementation
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm): the
// constructor starts without a buffer and with zero feedback, so that
// mute() before setbuffer() touches nothing; setbuffer() and mute() rewind
// the index.

#include "allpass.hpp"

allpass::allpass()
{
	feedback = 0;
	buffer = 0;
	bufsize = 0;
	bufidx = 0;
}

void allpass::setbuffer(float *buf, int size)
{
	buffer = buf;
	bufsize = size;
	bufidx = 0;
}

void allpass::mute()
{
	for (int i=0; i<bufsize; i++)
		buffer[i]=0;
	bufidx = 0;
}

void allpass::setfeedback(float val)
{
	feedback = val;
}

float allpass::getfeedback()
{
	return feedback;
}

//ends
