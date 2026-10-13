// Reverb model implementation
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm): the
// constructor ties no buffers; setbuffers() ties the filters to memory of
// buffersize() floats with the tunings scaled from 44.1 kHz to the sample
// rate (scaledtuning()) and mutes them. The loops declare their variables in
// C++ scope, which the original's pre-standard compilers did not need.

#include "revmodel.hpp"

namespace
{
	const int combtunings[numcombs][2] = {
		{combtuningL1, combtuningR1}, {combtuningL2, combtuningR2},
		{combtuningL3, combtuningR3}, {combtuningL4, combtuningR4},
		{combtuningL5, combtuningR5}, {combtuningL6, combtuningR6},
		{combtuningL7, combtuningR7}, {combtuningL8, combtuningR8},
	};
	const int allpasstunings[numallpasses][2] = {
		{allpasstuningL1, allpasstuningR1}, {allpasstuningL2, allpasstuningR2},
		{allpasstuningL3, allpasstuningR3}, {allpasstuningL4, allpasstuningR4},
	};
}

revmodel::revmodel()
{
	// Set default values
	allpassL[0].setfeedback(0.5f);
	allpassR[0].setfeedback(0.5f);
	allpassL[1].setfeedback(0.5f);
	allpassR[1].setfeedback(0.5f);
	allpassL[2].setfeedback(0.5f);
	allpassR[2].setfeedback(0.5f);
	allpassL[3].setfeedback(0.5f);
	allpassR[3].setfeedback(0.5f);
	setwet(initialwet);
	setroomsize(initialroom);
	setdry(initialdry);
	setdamp(initialdamp);
	setwidth(initialwidth);
	setmode(initialmode);
}

int revmodel::scaledtuning(int tuning, double samplerate)
{
	const int scaled = int(tuning * samplerate / 44100.0 + 0.5);
	return scaled < 1 ? 1 : scaled;
}

long revmodel::buffersize(double samplerate)
{
	long size = 0;
	for (int i=0; i<numcombs; i++)
		size += scaledtuning(combtunings[i][0], samplerate) + scaledtuning(combtunings[i][1], samplerate);
	for (int i=0; i<numallpasses; i++)
		size += scaledtuning(allpasstunings[i][0], samplerate) + scaledtuning(allpasstunings[i][1], samplerate);
	return size;
}

void revmodel::setbuffers(float *memory, double samplerate)
{
	// Tie the components to their buffers
	for (int i=0; i<numcombs; i++)
	{
		const int sizeL = scaledtuning(combtunings[i][0], samplerate);
		const int sizeR = scaledtuning(combtunings[i][1], samplerate);
		combL[i].setbuffer(memory, sizeL);
		memory += sizeL;
		combR[i].setbuffer(memory, sizeR);
		memory += sizeR;
	}
	for (int i=0; i<numallpasses; i++)
	{
		const int sizeL = scaledtuning(allpasstunings[i][0], samplerate);
		const int sizeR = scaledtuning(allpasstunings[i][1], samplerate);
		allpassL[i].setbuffer(memory, sizeL);
		memory += sizeL;
		allpassR[i].setbuffer(memory, sizeR);
		memory += sizeR;
	}

	// Buffer will be full of rubbish - so we MUST mute them
	mute();
}

void revmodel::mute()
{
	if (getmode() >= freezemode)
		return;

	for (int i=0;i<numcombs;i++)
	{
		combL[i].mute();
		combR[i].mute();
	}
	for (int i=0;i<numallpasses;i++)
	{
		allpassL[i].mute();
		allpassR[i].mute();
	}
}

void revmodel::processreplace(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples, int skip)
{
	float outL,outR,input;

	while(numsamples-- > 0)
	{
		outL = outR = 0;
		input = (*inputL + *inputR) * gain;

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

		// Calculate output REPLACING anything already there
		*outputL = outL*wet1 + outR*wet2 + *inputL*dry;
		*outputR = outR*wet1 + outL*wet2 + *inputR*dry;

		// Increment sample pointers, allowing for interleave (if any)
		inputL += skip;
		inputR += skip;
		outputL += skip;
		outputR += skip;
	}
}

void revmodel::processmix(float *inputL, float *inputR, float *outputL, float *outputR, long numsamples, int skip)
{
	float outL,outR,input;

	while(numsamples-- > 0)
	{
		outL = outR = 0;
		input = (*inputL + *inputR) * gain;

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

		// Calculate output MIXING with anything already there
		*outputL += outL*wet1 + outR*wet2 + *inputL*dry;
		*outputR += outR*wet1 + outL*wet2 + *inputR*dry;

		// Increment sample pointers, allowing for interleave (if any)
		inputL += skip;
		inputR += skip;
		outputL += skip;
		outputR += skip;
	}
}

void revmodel::update()
{
// Recalculate internal values after parameter change

	int i;

	wet1 = wet*(width/2 + 0.5f);
	wet2 = wet*((1-width)/2);

	if (mode >= freezemode)
	{
		roomsize1 = 1;
		damp1 = 0;
		gain = muted;
	}
	else
	{
		roomsize1 = roomsize;
		damp1 = damp;
		gain = fixedgain;
	}

	for(i=0; i<numcombs; i++)
	{
		combL[i].setfeedback(roomsize1);
		combR[i].setfeedback(roomsize1);
	}

	for(i=0; i<numcombs; i++)
	{
		combL[i].setdamp(damp1);
		combR[i].setdamp(damp1);
	}
}

// The following get/set functions are not inlined, because
// speed is never an issue when calling them, and also
// because as you develop the reverb model, you may
// wish to take dynamic action when they are called.

void revmodel::setroomsize(float value)
{
	roomsize = (value*scaleroom) + offsetroom;
	update();
}

float revmodel::getroomsize()
{
	return (roomsize-offsetroom)/scaleroom;
}

void revmodel::setdamp(float value)
{
	damp = value*scaledamp;
	update();
}

float revmodel::getdamp()
{
	return damp/scaledamp;
}

void revmodel::setwet(float value)
{
	wet = value*scalewet;
	update();
}

float revmodel::getwet()
{
	return wet/scalewet;
}

void revmodel::setdry(float value)
{
	dry = value*scaledry;
}

float revmodel::getdry()
{
	return dry/scaledry;
}

void revmodel::setwidth(float value)
{
	width = value;
	update();
}

float revmodel::getwidth()
{
	return width;
}

void revmodel::setmode(float value)
{
	mode = value;
	update();
}

float revmodel::getmode()
{
	if (mode >= freezemode)
		return 1;
	else
		return 0;
}

//ends
