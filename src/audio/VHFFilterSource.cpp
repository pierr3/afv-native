/* audio/VHFFilterSource.cpp
 *
 * This file is part of AFV-Native.
 *
 * Copyright (c) 2020 Christopher Collins
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include "afv-native/audio/VHFFilterSource.h"
#include <SimpleComp.h>
#include <SimpleLimit.h>

using namespace afv_native::audio;

VHFFilterSource::VHFFilterSource(HardwareType hd):
    compressor(new chunkware_simple::SimpleComp()),
    limiter(new chunkware_simple::SimpleLimit())
{
    compressor->setSampleRate(sampleRateHz);
    compressor->setAttack(0.8);
    compressor->setRelease(40.0);
    compressor->setThresh(8.0);
    compressor->setRatio(2);
    //compressor->initRuntime();
    compressorPostGain = pow(10.0f, (-5.5/20.0));

    limiter->setAttack(0.8);
    limiter->setSampleRate(sampleRateHz);
    limiter->setRelease(40.0);
    limiter->setThresh(8.0);
    limiter->initRuntime();

    this->hardware = hd;
    
    setupPresets();
}

VHFFilterSource::~VHFFilterSource()
{
    delete compressor;
};

void VHFFilterSource::setupPresets()
{
    if (hardware == HardwareType::Schmid_ED_137B) 
    {
        mFilters.push_back(BiQuadFilter::highPassFilter(sampleRateHz, 310, 0.25));
        mFilters.push_back(BiQuadFilter::peakingEQ(sampleRateHz, 450, 0.75, 17.0));
        mFilters.push_back(BiQuadFilter::peakingEQ(sampleRateHz, 1450, 1.0, 25.0));
        mFilters.push_back(BiQuadFilter::peakingEQ(sampleRateHz, 2000, 1.0, 25.0));
        mFilters.push_back(BiQuadFilter::lowPassFilter(sampleRateHz, 2500, 0.25));
    }

    if (hardware == HardwareType::Garex_220) 
    {
        mFilters.push_back(BiQuadFilter::highPassFilter(sampleRateHz, 300, 0.25));
        mFilters.push_back(BiQuadFilter::highShelfFilter(sampleRateHz, 400, 1.0, 8.0));
        mFilters.push_back(BiQuadFilter::highShelfFilter(sampleRateHz, 600, 1.0, 4.0));
        mFilters.push_back(BiQuadFilter::lowShelfFilter(sampleRateHz, 2000, 1.0, 1.0));
        mFilters.push_back(BiQuadFilter::lowShelfFilter(sampleRateHz, 2400, 1.0, 3.0));
        mFilters.push_back(BiQuadFilter::lowShelfFilter(sampleRateHz, 3000, 1.0, 10.0));
        mFilters.push_back(BiQuadFilter::lowPassFilter(sampleRateHz, 3400, 0.25));
    }

    if (hardware == HardwareType::Rockwell_Collins_2100)
    {
        mFilters.push_back(BiQuadFilter::customBuild(1.0, 0.0, 0.0, -0.01, 0.0, 0.0));
        mFilters.push_back(BiQuadFilter::customBuild(1.0, -1.7152995098277, 0.761385315196423, 0.0, 1.0, 0.753162969638192));
        mFilters.push_back(BiQuadFilter::customBuild(1.0, -1.71626681678914, 0.762433947105989, 1.0, -2.29278115712509, 1.000336632935775));
        mFilters.push_back(BiQuadFilter::customBuild(1.0, -1.79384214686345, 0.909678364879526, 1.0, -2.05042803669041, 1.05048374237779));
        mFilters.push_back(BiQuadFilter::customBuild(1.0, -1.79409285259567, 0.909822671281377, 1.0, -1.95188929743297, 0.951942325888074));
        mFilters.push_back(BiQuadFilter::customBuild(1.0, -1.9390093095185, 0.9411847259142, 1.0, -1.82547932903698, 1.09157529229851));
        mFilters.push_back(BiQuadFilter::customBuild(1.0, -1.94022767750807, 0.942630574503006, 1.0, -1.67241244173042, 0.916184578658119));
    }

}

/** transformFrame lets use apply this filter to a normal buffer, without following the sink/source flow.
 *
 * It always performs a copy of the data from In to Out at the very least.
 * 
 */
void VHFFilterSource::transformFrame(SampleType *bufferOut, SampleType const bufferIn[]) {
    double sl, sr;
    for (unsigned i = 0; i < frameSizeSamples; i++) {
        sl = bufferIn[i];
        sr = sl;
        limiter->process(sl, sr);
        for (int band = 0; band < mFilters.size(); band++)
        {
            sl = mFilters[band].TransformOne(sl);
        }

        sl *= static_cast<float>(compressorPostGain);

        bufferOut[i] = sl;
    }
}
