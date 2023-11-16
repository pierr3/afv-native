#ifndef SIMPLECOMPRESSOREFFECT_H
#define SIMPLECOMPRESSOREFFECT_H

#include <afv-native/audio/ISampleSource.h>
#include <afv-native/extern/compressor/compressor.h>

namespace afv_native::audio
{
    class SimpleCompressorEffect
    {
    public:
        explicit SimpleCompressorEffect();
        virtual ~SimpleCompressorEffect();

        void transformFrame(SampleType *bufferOut, SampleType const bufferIn[]);

    private:
        sf_compressor_state_st m_simpleCompressor;
        sf_snd m_inputSound;
        sf_snd m_outputSound;
    };
}

#endif // SIMPLECOMPRESSOREFFECT_H
