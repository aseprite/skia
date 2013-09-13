/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkBenchmark.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkRandom.h"
#include "SkChunkAlloc.h"
#include "SkString.h"

class ChunkAllocBench : public SkBenchmark {
    SkString    fName;
    size_t      fMinSize;
public:
    ChunkAllocBench(size_t minSize)  {
        fMinSize = minSize;
        fName.printf("chunkalloc_" SK_SIZE_T_SPECIFIER, minSize);
        fIsRendering = false;
    }

protected:
    virtual const char* onGetName() SK_OVERRIDE {
        return fName.c_str();
    }

    virtual void onDraw(SkCanvas*) SK_OVERRIDE {
        size_t inc = fMinSize >> 4;
        SkASSERT(inc > 0);
        size_t total = fMinSize * 64;

        SkChunkAlloc alloc(fMinSize);

        for (int i = 0; i < this->getLoops(); ++i) {
            size_t size = 0;
            int calls = 0;
            while (size < total) {
                alloc.allocThrow(inc);
                size += inc;
                calls += 1;
            }
            alloc.reset();
        }
    }

private:
    typedef SkBenchmark INHERITED;
};

DEF_BENCH( return new ChunkAllocBench(64); )
DEF_BENCH( return new ChunkAllocBench(8*1024); )
