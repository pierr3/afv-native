/* audio/RmsAgc.cpp
 *
 * This file is part of AFV-Native.
 *
 * Copyright (c) 2026 Pierre Ferran
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

#include "afv-native/audio/RmsAgc.h"
#include <algorithm>
#include <cmath>

using namespace afv_native::audio;

static double timeConstantToCoeff(double ms) {
    // Coefficient for per-frame envelope smoothing.
    // Applied once per frame (every frameLengthMs), not per-sample.
    return exp(-static_cast<double>(frameLengthMs) / ms);
}

RmsAgc::RmsAgc(double targetLevelDb, double maxGainDb, double minGainDb,
               double attackMs, double releaseMs):
    mEnabled(true),
    mTargetLevelDb(targetLevelDb),
    mTargetLevel(pow(10.0, targetLevelDb / 20.0)),
    mMaxGain(pow(10.0, maxGainDb / 20.0)),
    mMinGain(pow(10.0, minGainDb / 20.0)),
    mAttackCoeff(timeConstantToCoeff(attackMs)),
    mReleaseCoeff(timeConstantToCoeff(releaseMs)),
    mEnvelopeState(0.0),
    mGainState(1.0) {
}

void RmsAgc::setTargetLevelDb(double targetDb) {
    mTargetLevelDb = targetDb;
    mTargetLevel   = pow(10.0, targetDb / 20.0);
}

double RmsAgc::getTargetLevelDb() const {
    return mTargetLevelDb;
}

void RmsAgc::setEnabled(bool enabled) {
    mEnabled = enabled;
    if (!enabled) {
        reset();
    }
}

bool RmsAgc::getEnabled() const {
    return mEnabled;
}

void RmsAgc::reset() {
    mEnvelopeState = 0.0;
    mGainState     = 1.0;
}

void RmsAgc::transformFrame(SampleType *bufferOut, SampleType const bufferIn[]) {
    if (!mEnabled) {
        if (bufferOut != bufferIn) {
            for (int i = 0; i < frameSizeSamples; i++) {
                bufferOut[i] = bufferIn[i];
            }
        }
        return;
    }

    // Compute frame RMS
    double sumSquares = 0.0;
    for (int i = 0; i < frameSizeSamples; i++) {
        double s = static_cast<double>(bufferIn[i]);
        sumSquares += s * s;
    }
    double frameRms = sqrt(sumSquares / static_cast<double>(frameSizeSamples));

    // Smooth the RMS envelope with attack/release
    double coeff = (frameRms > mEnvelopeState) ? mAttackCoeff : mReleaseCoeff;
    mEnvelopeState = frameRms + coeff * (mEnvelopeState - frameRms);

    // Compute desired gain, guarding against silence
    const double silenceThreshold = 1e-6;
    double       desiredGain      = 1.0;
    if (mEnvelopeState > silenceThreshold) {
        desiredGain = mTargetLevel / mEnvelopeState;
    }

    // Clamp to allowed range
    desiredGain = std::max(mMinGain, std::min(mMaxGain, desiredGain));

    // Linearly interpolate gain across the frame to avoid clicks
    double gainStep    = (desiredGain - mGainState) / static_cast<double>(frameSizeSamples);
    double currentGain = mGainState;

    for (int i = 0; i < frameSizeSamples; i++) {
        bufferOut[i] = static_cast<SampleType>(bufferIn[i] * currentGain);
        currentGain += gainStep;
    }

    mGainState = currentGain;
}
