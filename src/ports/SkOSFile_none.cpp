/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkOSFile.h"

bool sk_fidentical(SkFILE* a, SkFILE* b) {
    return false;
}

void sk_fmunmap(const void* addr, size_t length) { }

void* sk_fmmap(SkFILE* f, size_t* size) {
    return NULL;
}
