#ifndef INCLUDE_REAL_TIME_SYNTHESIZER_H
#define INCLUDE_REAL_TIME_SYNTHESIZER_H
/*
 * This is the Loris C++ Class Library, implementing analysis,
 * manipulation, and synthesis of digitized sounds using the Reassigned
 * Bandwidth-Enhanced Additive Sound Model.
 *
 * Loris is Copyright (c) 1999-2010 by Kelly Fitz and Lippold Haken
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

#include <vector>
#include <queue>

//	begin namespace
namespace Loris {
    
struct PartialStruct
{
    enum { NoBreakpointProcessed = -1, FirstBreakpoint };
    
    double duration = 0.0;
    double startTime = 0.0;
    double endTime = 0.0;
    int numBreakpoints = 0;

    std::vector<std::pair<double, Breakpoint>> breakpoints;
    
    struct SynthesizerState
    {
        int currentSamp = 0;
        int endSamp = 0;
        int lastBreakpoint = NoBreakpointProcessed;
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
//!	bandwidth-enhanced Partials as floating point (double) samples at a
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
	//!	buffer (a standard library vector). Since Partials generated by 
	//! the Loris Analyzer generally begin and end at non-zero amplitude,
	//! zero-amplitude Breakpoints are inserted at either end of the Partial, 
	//!	at a temporal distance equal to the fade time, to reduce turn-on and 
	//!	turn-off artifacts.
	//!
	//! \sa RealTimeSynthesizer::Parameters
	//!
	//!	\param	buffer The vector (of doubles) into which rendered samples
	//!			   should be accumulated.
	//!	\throw	InvalidArgument if any of the parameters is invalid.
	RealTimeSynthesizer( std::vector<double> & buffer );	
	//!	Construct a RealTimeSynthesizer using the specified parameters and sample
	//!	buffer (a standard library vector). Since Partials generated by the 
	//! Loris Analyzer generally begin and end at non-zero amplitude, zero-amplitude
	//!	Breakpoints are inserted at either end of the Partial, at a temporal
	//!	distance equal to the fade time, to reduce turn-on and turn-off
	//!	artifacts.
	//!
	//!	\param	params A Parameters struct storing the configuration of 
	//!             RealTimeSynthesizer parameters.
	//!	\param	buffer The vector (of doubles) into which rendered samples
	//!			   should be accumulated.
	//!	\throw	InvalidArgument if any of the parameters is invalid.
	RealTimeSynthesizer( Parameters params, std::vector<double> & buffer );	
	
	//!	Construct a RealTimeSynthesizer using the specified sampling rate, sample
	//!	buffer (a standard library vector), and the default fade time 
	//!	stored in the DefaultParameters. Since Partials generated by the Loris 
	//!	Analyzer generally begin and end at non-zero amplitude, zero-amplitude
	//!	Breakpoints are inserted at either end of the Partial, at a temporal
	//!	distance equal to the fade time, to reduce turn-on and turn-off
	//!	artifacts.
	//!
	//!	\param	srate The rate (Hz) at which to synthesize samples
	//!			   (must be positive).
	//!	\param	buffer The vector (of doubles) into which rendered samples
	//!			   should be accumulated.
	//!	\throw	InvalidArgument if the specfied sample rate is non-positive.
	RealTimeSynthesizer( double srate, std::vector<double> & buffer );
	             
    //!	Construct a RealTimeSynthesizer using the specified sampling rate, sample
	//!	buffer (a standard library vector), and Partial fade time 
	//!	(in seconds). Since Partials generated by the Loris Analyzer
	//!	generally begin and end at non-zero amplitude, zero-amplitude
	//!	Breakpoints are inserted at either end of the Partial, at a temporal
	//!	distance equal to the fade time, to reduce turn-on and turn-off
	//!	artifacts.
	//!
	//!	\param	srate The rate (Hz) at which to synthesize samples
	//!			   (must be positive).
	//!	\param	buffer The vector (of doubles) into which rendered samples
	//!			   should be accumulated.
	//!	\param	fadeTime The Partial fade time in seconds (must be non-negative).
	//!	\throw	InvalidArgument if the specfied sample rate is non-positive.
	//!	\throw	InvalidArgument if the specified fade time is negative.
	RealTimeSynthesizer( double srate, std::vector<double> & buffer, double fadeTime );
	
	// 	Compiler can generate copy, assign, and destroy.
	//	RealTimeSynthesizer( const RealTimeSynthesizer & other );
	//	~RealTimeSynthesizer( void );
	//	RealTimeSynthesizer & operator= ( const RealTimeSynthesizer & other );
    
    void setupRealtime(PartialList & partials);
    
    void synthesizeNext(int samples);
    
    void resetSynth()
    {
        partialIdx = 0;
        
        std::fill (m_sampleBuffer->begin(), m_sampleBuffer->end(), 0);
        processedSamples = 0;
        
        int size = partials.size();
        for (int i = 0; i < size; i++)
        {
            //  compute the starting time for synthesis of this Partial,
            //  m_fadeTimeSec before the Partial's startTime, but not before 0:

            partials[i].state.currentSamp = index_type( (partials[i].startTime * m_srateHz) + 0.5 );   //  cheap rounding
            partials[i].state.lastBreakpoint = PartialStruct::NoBreakpointProcessed;
            
            //  cache the previous frequency (in Hz) so that it
            //  can be used to reset the phase when necessary
            //  in the sample computation loop below (this saves
            //  having to recompute from the oscillator's radian
            //  frequency):
            partials[i].state.prevFrequency = partials[i].breakpoints[1].second.frequency();// 0 is null breakpoint
        }
        
    }
	 	
//	-- parameter access and mutation --
//	-- implementation --
private:
    
    //	-- synthesis --
	//!	Synthesize a bandwidth-enhanced sinusoidal Partial. Zero-amplitude
	//!	Breakpoints are inserted at either end of the Partial to reduce
	//!	turn-on and turn-off artifacts, as described above. The synthesizer
	//!	wont(!) resize the buffer.  This must be done in setupRealtime().
    //! Previous contents of the buffer is (!)
	//!	overwritten. Partials with start times earlier than the Partial fade
	//!	time will have shorter onset fades. Partials are not rendered at
	//!   frequencies above the half-sample rate.
	//!
	//! \param  p The Partial to synthesize.
	//! \return Nothing.
	//!	\pre    The partial must have non-negative start time.
	//! \post   This RealTimeSynthesizer's sample buffer (vector) has been
	//!         resized to accommodate the entire duration of the
	//!         Partial, p, including fade out at the end.
	//!	\throw	InvalidPartial if the Partial has negative start time.
	void synthesize( PartialStruct & p, const double endTime);
    
    void initInstance();
    
    
    double OneOverSrate;
    typedef unsigned long index_type;
    index_type tgtSamp;
    double * bufferBegin;
    double dphase;
    
    std::vector<PartialStruct> partials;
    int partialIdx;
    double processedSamples = 0;
    std::queue<PartialStruct* > partialsBeingProcessed;
};	//	end of class RealTimeSynthesizer


}

#endif /* ndef INCLUDE_REAL_TIME_SYNTHESIZER_H */
