//
//  SourceToSinkAdapter.cpp
//  afv_native
//
//  Created by Mike Evans on 11/18/20.
//

#include "afv-native/audio/SourceToSinkAdapter.h"
#include <memory>

using namespace afv_native::audio;
using namespace std;

SourceToSinkAdapter::SourceToSinkAdapter(std::shared_ptr<ISampleSource> inSource, std::shared_ptr<ISampleSink> inSink):
    mBuffer(frameSizeSamples, 0), mSink(std::move(inSink)), mSource(std::move(inSource)) {
}

void SourceToSinkAdapter::tick() {
    mSource->getAudioFrame(mBuffer.data());
    mSink->putAudioFrame(mBuffer.data());
}
