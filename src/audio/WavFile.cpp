/* WavFile.cpp
 *
 * Class(es) for reading audio samples from WAV/RIFF files
 *
 * Copyright (C) 2018, Christopher Collins
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

#include "afv-native/audio/WavFile.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace std;
using namespace afv_native::audio;
/*
 * There is a HUGE assumption in here right now that everything is
 * Little Endian - this is now true for X-Plane. It will need to be fixed
 * if this ever changes.
 *
 * - CC.
 */

/** minimumBlockAlignment calculates the minimum permissible block size for the
 * specified sampleSize & channel count.
 *
 * It does this obeying the rules established by Microsoft for multichannel
 * audio data & WAVE file, which in short, it is required that individual
 * samples are padded up to a full byte, with channels in sequential order.
 *
 * There's nothing to say that a larger block alignment can't be used, which
 * results in with wasted space.
 *
 * If a file reports a block alignment smaller than this, it must be invalid.
 *
 * We will repack the audio down to this size on load as not to waste memory.
 *
 * @param sampleSize size of each sample in bits.
 * @param numChannels total number of channels per sample
 */
inline int minimumBlockAlignment(int sampleSize, int numChannels) {
    return ((sampleSize + 7) / 8) * numChannels;
}

AudioSampleData::AudioSampleData(int numChannels, int bitsPerSample, int sampleRate, bool isFloat):
    mNumChannels(numChannels), mBitsPerSample(bitsPerSample), mSampleRate(sampleRate), mSampleCount(0), mIsFloat(isFloat) {
    mSampleAlignment = minimumBlockAlignment(mBitsPerSample, mNumChannels);
}

void AudioSampleData::AppendSamples(uint8_t blockSize, unsigned count, void *data) {
    const size_t oldByteSize = mSampleData.size();
    const size_t newByteSize = (mSampleCount + count) * mSampleAlignment;
    mSampleData.resize(newByteSize);
    auto *dptr = mSampleData.data() + oldByteSize;
    if (blockSize == mSampleAlignment) {
        memcpy(dptr, data, count * blockSize);
    } else {
        auto *sptr = reinterpret_cast<uint8_t *>(data);
        for (unsigned c = 0; c < count; c++) {
            memcpy(dptr, sptr, mSampleAlignment);
            sptr += blockSize;
            dptr += mSampleAlignment;
        }
    }
    mSampleCount += count;
}

/* ==== Getters ==== */

int8_t AudioSampleData::getNumChannels() const {
    return mNumChannels;
}

int8_t AudioSampleData::getBitsPerSample() const {
    return mBitsPerSample;
}

uint8_t AudioSampleData::getSampleAlignment() const {
    return mSampleAlignment;
}

int AudioSampleData::getSampleRate() const {
    return mSampleRate;
}

size_t AudioSampleData::getSampleCount() const {
    return mSampleCount;
}

const void *AudioSampleData::getSampleData() const {
    return mSampleData.data();
}

bool AudioSampleData::isFloat() const {
    return mIsFloat;
}

/* ==== WAVE Loading Logic Below ==== */

static const char *WavTopChunkID    = "RIFF";
static const char *WavFormatChunkID = "fmt ";
static const char *WavDataChunkID   = "data";

#pragma pack(push, 1)
struct ChunkHeader {
    char     chunkID[4];
    uint32_t chunkSize;
};

/* names below from the Microsoft headers/documentation for consistency.
 */
struct WavFormatChunk {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    /* 18 byte body... */
    uint16_t cbSize;
    /* 40 byte body */
    uint16_t wValidBitsPerSample;
    uint32_t dwChannelMask;
    char     SubFormat[16];
};
#pragma pack(pop)

const uint16_t WAV_FORMAT_PCM        = 1;
const uint16_t WAV_FORMAT_IEEE_FLOAT = 3;

typedef struct WavTOCEntry {
    struct ChunkHeader ch;
    uint32_t           offset;
} WavTOCEntry;

static vector<WavTOCEntry>::const_iterator FindTOCFor(const vector<WavTOCEntry> &toc, const char *chunk_id) {
    return std::find_if(toc.cbegin(), toc.cend(), [chunk_id](const WavTOCEntry &x) -> bool {
        return !memcmp(x.ch.chunkID, chunk_id, 4);
    });
}

static bool readHeader(FILE *srcFile, const struct ChunkHeader &ch, vector<WavTOCEntry> &o_HeaderItems) {
    uint32_t expectedBytesLeft = ch.chunkSize - 4;

    while (expectedBytesLeft > 0) {
        /* if there's less than a chunkheader less, there's a problem. */
        if (expectedBytesLeft < sizeof(struct ChunkHeader)) {
            return false;
        }

        WavTOCEntry e;
        if (1 != fread(&e.ch, sizeof(e.ch), 1, srcFile)) {
            return false;
        }
        expectedBytesLeft -= sizeof(e.ch);

        /* make sure the expected file size is large enough */
        if (e.ch.chunkSize > expectedBytesLeft) {
            return false;
        }
        e.offset = ftell(srcFile);
        o_HeaderItems.emplace_back(e);

        int pad = 0;
        if ((e.ch.chunkSize % 2) != 0) {
            pad = 1;
        }

        /* skip forward to the next chunk */
        fseek(srcFile, e.ch.chunkSize + pad, SEEK_CUR);
        expectedBytesLeft -= e.ch.chunkSize + pad;
    }
    return true;
}

static std::unique_ptr<AudioSampleData> extractData(FILE *fh, const vector<WavTOCEntry> &toc) {
    /* if we get here without an error, it means the file was well formed and
     * we now have a ToC for the file chunks
     *
     * Find the format header.
     */
    auto ti = FindTOCFor(toc, WavFormatChunkID);
    if (ti == toc.end()) {
        // couldn't find the format chunk
        return nullptr;
    }
    fseek(fh, ti->offset, SEEK_SET);
    int chunkSize = ti->ch.chunkSize;

    /* safety check - the chunk can't be smaller than 16 bytes */
    if (chunkSize < 16) {
        return nullptr;
    }
    /* and cap it at the size of the header struct so we don't buffer overflow */
    if (chunkSize > sizeof(WavFormatChunk)) {
        chunkSize = sizeof(WavFormatChunk);
    }

    struct WavFormatChunk fc;
    if (1 != fread(&fc, chunkSize, 1, fh)) {
        return nullptr;
    }

    /* now that we have the header, verify that we can support the format. */
    if (fc.wFormatTag != WAV_FORMAT_PCM && fc.wFormatTag != WAV_FORMAT_IEEE_FLOAT) {
        return nullptr;
    }
    /* sanity check the block stride */
    if (fc.nBlockAlign < minimumBlockAlignment(fc.wBitsPerSample, fc.nChannels)) {
        return nullptr;
    }

    auto asd = std::make_unique<AudioSampleData>(fc.nChannels, fc.wBitsPerSample, fc.nSamplesPerSec, fc.wFormatTag == WAV_FORMAT_IEEE_FLOAT);

    /* now to check the data block */
    ti = FindTOCFor(toc, WavDataChunkID);
    if (ti == toc.end()) {
        return nullptr;
    }

    /* we can live with short reads here, so we use a slightly different strategy */
    fseek(fh, ti->offset, SEEK_SET);

    std::vector<uint8_t> dbuf(ti->ch.chunkSize);
    size_t samplesRead = fread(dbuf.data(), fc.nBlockAlign, ti->ch.chunkSize / fc.nBlockAlign, fh);
    if (samplesRead <= 0) {
        return nullptr;
    }
    asd->AppendSamples(fc.nBlockAlign, static_cast<unsigned>(samplesRead), dbuf.data());
    return asd;
}

struct FileCloser {
    void operator()(FILE *f) const { fclose(f); }
};

std::unique_ptr<AudioSampleData> afv_native::audio::LoadWav(const char *fileName) {
    std::unique_ptr<FILE, FileCloser> fh(fopen(fileName, "rb"));
    if (!fh) {
        return nullptr;
    }

    /* first of all, verify that we have an actual wavfile */
    struct ChunkHeader ch;

    if (1 != fread(&ch, sizeof(ch), 1, fh.get())) {
        /* short read or error */
        return nullptr;
    }
    /* check the top level header. */
    if (memcmp(WavTopChunkID, ch.chunkID, 4)) {
        return nullptr;
    }
    /* minimum sensible size is the WAVE identifier, 2 chunk headers
     * and the minimum legal WAVFormatChunk body
     */
    if (ch.chunkSize < (4 + 16 + 8 + 8)) {
        return nullptr;
    }
    /* check the WAVE magic */
    char waveMagic[4];
    if (1 != fread(&waveMagic, 4, 1, fh.get())) {
        return nullptr;
    }
    if (memcmp("WAVE", waveMagic, 4)) {
        return nullptr;
    }

    /* if were're still here, the file is valid so far, now we need to read
     * scan the wave file to find the blocks we need */
    vector<WavTOCEntry> toc;
    if (!readHeader(fh.get(), ch, toc)) {
        return nullptr;
    }

    return extractData(fh.get(), toc);
}
