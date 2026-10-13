// Reverb model declaration
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm): the
// buffers are no fixed arrays of the class any more; buffersize() tells the
// floats the tunings need at a sample rate and setbuffers() ties the filters
// to memory the caller allocated, with the tunings of tuning.h scaled from
// 44.1 kHz to the sample rate. processframe() renders the wet signal of one
// frame, without the dry one, so that the caller crossfades them; it takes
// a mono input as the same sample on both sides.

#ifndef _revmodel_
#define _revmodel_

#include "comb.hpp"
#include "allpass.hpp"
#include "tuning.h"

class revmodel
{
public:
					revmodel();
	static	long	buffersize(double samplerate);
			void	setbuffers(float *memory, double samplerate);
			void	mute();
	inline	void	processframe(float inputL, float inputR, float &outputL, float &outputR);
			void	processmix(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples, int skip);
			void	processreplace(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples, int skip);
			void	setroomsize(float value);
			float	getroomsize();
			void	setdamp(float value);
			float	getdamp();
			void	setwet(float value);
			float	getwet();
			void	setdry(float value);
			float	getdry();
			void	setwidth(float value);
			float	getwidth();
			void	setmode(float value);
			float	getmode();
	static	int		scaledtuning(int tuning, double samplerate);
private:
			void	update();
private:
	float	gain;
	float	roomsize,roomsize1;
	float	damp,damp1;
	float	wet,wet1,wet2;
	float	dry;
	float	width;
	float	mode;

	// Comb filters
	comb	combL[numcombs];
	comb	combR[numcombs];

	// Allpass filters
	allpass	allpassL[numallpasses];
	allpass	allpassR[numallpasses];
};


// Renders the wet signal of one frame - inline for speed

inline void revmodel::processframe(float inputL, float inputR, float &outputL, float &outputR)
{
	float outL = 0;
	float outR = 0;
	float input = (inputL + inputR) * gain;

	// Accumulate comb filters in parallel
	for(int i=0; i<numcombs; i++)
	{
		outL += combL[i].process(input);
		outR += combR[i].process(input);
	}

	// Feed through allpasses in series
	for(int i=0; i<numallpasses; i++)
	{
		outL = allpassL[i].process(outL);
		outR = allpassR[i].process(outR);
	}

	outputL = outL*wet1 + outR*wet2;
	outputR = outR*wet1 + outL*wet2;
}

#endif//_revmodel_

//ends
