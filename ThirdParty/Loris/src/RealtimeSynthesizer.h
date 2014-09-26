#ifndef INCLUDE_REAL_TIME_SYNTHESIZER_H
#define INCLUDE_REAL_TIME_SYNTHESIZER_H
/*
 * This is the Loris C++ Class Library, implementing analysis,
 * manipulation, and synthesis of digitized sounds using the Reassigned
 * Bandwidth-Enhanced Additive Sound Model.
 *
 * Loris is Copyright (c) 1999-2010, 2014 by Kelly Fitz, Lippold Haken and Tomas Medek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * Synthesizer.C
 *
 * Implementation of class Loris::RealTimeSynthesizer, a synthesizer of
 * bandwidth-enhanced Partials.
 *
 * Tomas Medek, 24 Jul 20149
 * tom@virtualanalogy.org
 *
 *
 */
 
#include "Synthesizer.h"
#include "RealtimeOscillator.h"

#include <vector>
#include <queue>
#include <cmath>

#if defined(HAVE_M_PI) && (HAVE_M_PI)
const double Pi = M_PI;
#else
const double Pi = 3.14159265358979324;
#endif

//	begin namespace
namespace Loris {
// Using this struct because iterating over partials and especially Breakpoint is very
// expensive. So I wrote this data container.
struct PartialStruct
{
    enum { NoBreakpointProcessed = 0, FirstBreakpoint };
    
    double startTime = 0.0;
    double endTime = 0.0;
    int numBreakpoints = 0;

    std::vector<std::pair<double, Breakpoint>> breakpoints;
    
    struct SynthesizerState
    {
        int currentSamp = 0;
        int lastBreakpointIdx = NoBreakpointProcessed;
        Breakpoint envelope;
        double prevFrequency;
    } state;
};
// ---------------------------------------------------------------------------
//	class RealTimeSynthesizer
//
//! A RealTimeSynthesizer renders bandwidth-enhanced Partials into a buffer
//! of samples. 
//!	
//!	Class Synthesizer represents an algorithm for rendering
//!	bandwidth-enhanced Partials as floating point samples at a
//!	specified sampling rate, and accumulating them into a buffer. 
//!
//!	The Synthesizer does not own the sample buffer, the client is responsible
//!	for its construction and destruction, and many Synthesizers may share
//!	a buffer.
//
class RealTimeSynthesizer : public Synthesizer
{
//	-- public interface --
public:
//	-- construction --
    //!	Construct a Synthesizer using the default parameters and sample
    //!	buffer (a standard library vector). Since Partials generated by the
    //! Loris Analyzer generally begin and end at non-zero amplitude, zero-amplitude
    //!	Breakpoints are inserted at either end of the Partial, at a temporal
    //!	distance equal to the fade time, to reduce turn-on and turn-off
    //!	artifacts.
    //!
    //! \sa Synthesizer::Parameters
    //!
    //!	\param	buffer The vector (of floats) into which rendered samples
    //!			   should be accumulated.
    //!	\throw	InvalidArgument if any of the parameters is invalid.
	RealTimeSynthesizer(std::vector<float> & buffer);
	
	// 	Compiler can generate copy, assign, and destroy.
	//	RealTimeSynthesizer( const RealTimeSynthesizer & other );
	//	~RealTimeSynthesizer( void );
	//	RealTimeSynthesizer & operator= ( const RealTimeSynthesizer & other );
    
    //!	Prepare internal structures for synthesis. PartialList is transformed to
    //! to more conveniant structure for real-time processing. reset() is also called.
    //! Fade in/out Breakpoints are inserted at either end of the Partial.
    //! Partials with start times earlier than the Partial fade
    //! time will have shorter onset fades. Previous data is overwritten.
    //!
    //! \param  partials The Partials to synthesize.
    //! \param  pitch original pitch of the partials
    //! \return Nothing.
    //! \post   This RealTimeSynthesizer's is ready for synthesise the sound specified
    //!         by given partials.
    void setup(PartialList & partials, double pitch) noexcept;

    //!	Set sample rate.
    //!
    //! \param  rate new sample rate
    //! \return Nothing.
    void setSampleRate(double rate) override;
    
    //!	Synthesize next block of samples of the partials. The synthesizer
    //! will resize the inner buffer as necessary. Previous contents of the buffer
    //! are overwritten.
    //!
    //! \param  sample Number of samples to synthesize.
    //! \return Nothing.
    //! \post   Internal state of synthesizer changes - it is ready to synthesize
    //!         next block of samples starting at 'previous count of samples' + samples.
    void synthesizeNext(int samples) noexcept;
    
    //!	Reset RealtimeSynthesizer to render sound from the beging.
    //!
    //! \post   Sound is rendered in original pitch.
    //! \return Nothing.
    void reset() noexcept;
    
    //!	Change pitch of sound.
    //!
    //! \param  New pitch in frequency of the sound.
    //! \return Nothing.
    void setPitch(double frequency) noexcept;
    
 	
//	-- parameter access and mutation --
//	-- implementation --
private:
    
    //	-- synthesis --
    //! Synthesize a bandwidth-enhanced sinusoidal Partial.
    //!
    //! \param  buffer  The samples buffer.
    //! \param  samples Number of samples to be synthesized.
    //! \param  p       The Partial to synthesize.
    //! \return Nothing.
    //! \pre    The buffer has to have capacity to contain all samples.
    //! \post   This RealTimeSynthesizer's sample buffer (vector) contain synthesised
    //!         partials and storeed inner state of synthesiser.
    //!
    void synthesize( PartialStruct &p, float * buffer, const int samples) noexcept;
    
    void clearPartialsBeingProcessed() noexcept
	{
		while (!partialsBeingProcessed.empty())
			partialsBeingProcessed.pop();
	}
    
    RealtimeOscillator m_osc; 	//  the Synthesizer has-a Oscillator that it uses to render
                                //  all the Partials one by one.
    
    double OneOverSrate = 0;
    typedef unsigned long index_type;
    
    double pitch = 0.;                      // original pitch of partial data
    
    std::vector<PartialStruct> partials;
    int partialIdx;                         // last loaded partial
    int processedSamples = 0;               // internal sample position counter
    std::queue<PartialStruct* > partialsBeingProcessed; // partials not finished yet
    std::vector<float> *buffer;             // sample buffer
    std::vector<double> xxx;                // buffer to satisfy Synthesizer constructor
    
};	//	end of class RealTimeSynthesizer


}

#endif /* ndef INCLUDE_REAL_TIME_SYNTHESIZER_H */
