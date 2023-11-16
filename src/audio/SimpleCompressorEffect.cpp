#include "afv-native/audio/SimpleCompressorEffect.h"

using namespace afv_native::audio;

SimpleCompressorEffect::SimpleCompressorEffect()
{
    sf_defaultcomp(&m_simpleCompressor, sampleRateHz);
}

SimpleCompressorEffect::~SimpleCompressorEffect()
{

}

void SimpleCompressorEffect::transformFrame(SampleType *bufferOut, const SampleType bufferIn[])
{
    sf_snd output_snd = sf_snd_new(frameSizeSamples, sampleRateHz, true);
    sf_snd input_snd = sf_snd_new(frameSizeSamples, sampleRateHz, true);

    for(int i = 0; i < frameSizeSamples; i++)
    {
        input_snd->samples[i].L = bufferIn[i];
    }

    sf_compressor_process(&m_simpleCompressor, frameSizeSamples, input_snd->samples, output_snd->samples);

    for(int i = 0; i < frameSizeSamples; i++)
    {
        bufferOut[i] = static_cast<SampleType>(output_snd->samples[i].L);
    }

    sf_snd_free(input_snd);
    sf_snd_free(output_snd);
}
