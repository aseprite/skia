/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPdfNativeDoc.h"
#include "SkPdfNativeTokenizer.h"
#include "SkPdfNativeObject.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

// TODO(edisonn): for some reason on mac these files are found here, but are found from headers
//#include "SkPdfFileTrailerDictionary_autogen.h"
//#include "SkPdfCatalogDictionary_autogen.h"
//#include "SkPdfPageObjectDictionary_autogen.h"
//#include "SkPdfPageTreeNodeDictionary_autogen.h"
#include "SkPdfHeaders_autogen.h"

#include "SkPdfMapper_autogen.h"

#include "SkStream.h"


static long getFileSize(const char* filename)
{
    struct stat stat_buf;
    int rc = stat(filename, &stat_buf);
    return rc == 0 ? (long)stat_buf.st_size : -1;
}

static const unsigned char* lineHome(const unsigned char* start, const unsigned char* current) {
    while (current > start && !isPdfEOL(*(current - 1))) {
        current--;
    }
    return current;
}

static const unsigned char* previousLineHome(const unsigned char* start, const unsigned char* current) {
    if (current > start && isPdfEOL(*(current - 1))) {
        current--;
    }

    // allows CR+LF, LF+CR but not two CR+CR or LF+LF
    if (current > start && isPdfEOL(*(current - 1)) && *current != *(current - 1)) {
        current--;
    }

    while (current > start && !isPdfEOL(*(current - 1))) {
        current--;
    }

    return current;
}

static const unsigned char* ignoreLine(const unsigned char* current, const unsigned char* end) {
    while (current < end && !isPdfEOL(*current)) {
        current++;
    }
    current++;
    if (current < end && isPdfEOL(*current) && *current != *(current - 1)) {
        current++;
    }
    return current;
}

SkPdfNativeDoc* gDoc = NULL;

// TODO(edisonn): NYI
// TODO(edisonn): 3 constructuctors from URL, from stream, from file ...
// TODO(edisonn): write one that accepts errors in the file and ignores/fixis them
// TODO(edisonn): testing:
// 1) run on a lot of file
// 2) recoverable corupt file: remove endobj, endsteam, remove other keywords, use other white spaces, insert comments randomly, ...
// 3) irrecoverable corrupt file

SkPdfNativeDoc::SkPdfNativeDoc(SkStream* stream)
        : fAllocator(new SkPdfAllocator())
        , fFileContent(NULL)
        , fContentLength(0)
        , fRootCatalogRef(NULL)
        , fRootCatalog(NULL) {
    size_t size = stream->getLength();
    void* ptr = sk_malloc_throw(size);
    stream->read(ptr, size);

    init(ptr, size);
}

SkPdfNativeDoc::SkPdfNativeDoc(const char* path)
        : fAllocator(new SkPdfAllocator())
        , fFileContent(NULL)
        , fContentLength(0)
        , fRootCatalogRef(NULL)
        , fRootCatalog(NULL) {
    gDoc = this;
    FILE* file = fopen(path, "r");
    // TODO(edisonn): put this in a function that can return NULL
    if (file) {
        size_t size = getFileSize(path);
        void* content = sk_malloc_throw(size);
        bool ok = (0 != fread(content, size, 1, file));
        fclose(file);
        if (!ok) {
            sk_free(content);
            // TODO(edisonn): report read error
            // TODO(edisonn): not nice to return like this from constructor, create a static
            // function that can report NULL for failures.
            return;  // Doc will have 0 pages
        }

        init(content, size);
    }
}

void SkPdfNativeDoc::init(const void* bytes, size_t length) {
    fFileContent = (const unsigned char*)bytes;
    fContentLength = length;
    const unsigned char* eofLine = lineHome(fFileContent, fFileContent + fContentLength - 1);
    const unsigned char* xrefByteOffsetLine = previousLineHome(fFileContent, eofLine);
    const unsigned char* xrefstartKeywordLine = previousLineHome(fFileContent, xrefByteOffsetLine);

    if (strcmp((char*)xrefstartKeywordLine, "startxref") != 0) {
        // TODO(edisonn): report/issue
    }

    long xrefByteOffset = atol((const char*)xrefByteOffsetLine);

    bool storeCatalog = true;
    while (xrefByteOffset >= 0) {
        const unsigned char* trailerStart = readCrossReferenceSection(fFileContent + xrefByteOffset, xrefstartKeywordLine);
        xrefByteOffset = -1;
        if (trailerStart < xrefstartKeywordLine) {
            readTrailer(trailerStart, xrefstartKeywordLine, storeCatalog, &xrefByteOffset, false);
            storeCatalog = false;
        }
    }

    // TODO(edisonn): warn/error expect fObjects[fRefCatalogId].fGeneration == fRefCatalogGeneration
    // TODO(edisonn): security, verify that SkPdfCatalogDictionary is indeed using mapper
    // load catalog

    if (fRootCatalogRef) {
        fRootCatalog = (SkPdfCatalogDictionary*)resolveReference(fRootCatalogRef);
        if (fRootCatalog->isDictionary() && fRootCatalog->valid()) {
            SkPdfPageTreeNodeDictionary* tree = fRootCatalog->Pages(this);
            if (tree && tree->isDictionary() && tree->valid()) {
                fillPages(tree);
            }
        }
    }

    // TODO(edisonn): clean up this doc, or better, let the caller call again and build a new doc
    // caller should be a static function.
    if (pages() == 0) {
        loadWithoutXRef();
    }

    // TODO(edisonn): corrupted pdf, read it from beginning and rebuild (xref, trailer, or just reall all objects)
    // 0 pages

    // now actually read all objects if we want, or do it lazyly
    // and resolve references?... or not ...
}

void SkPdfNativeDoc::loadWithoutXRef() {
    const unsigned char* current = fFileContent;
    const unsigned char* end = fFileContent + fContentLength;

    // TODO(edisonn): read pdf version
    current = ignoreLine(current, end);

    current = skipPdfWhiteSpaces(0, current, end);
    while (current < end) {
        SkPdfNativeObject token;
        current = nextObject(0, current, end, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
        if (token.isInteger()) {
            int id = (int)token.intValue();

            token.reset();
            current = nextObject(0, current, end, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
            // int generation = (int)token.intValue();  // TODO(edisonn): ignored for now

            token.reset();
            current = nextObject(0, current, end, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
            // TODO(edisonn): must be obj, return error if not? ignore ?
            if (!token.isKeyword("obj")) {
                continue;
            }

            while (fObjects.count() < id + 1) {
                reset(fObjects.append());
            }

            fObjects[id].fOffset = current - fFileContent;

            SkPdfNativeObject* obj = fAllocator->allocObject();
            current = nextObject(0, current, end, obj, fAllocator, this PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));

            fObjects[id].fResolvedReference = obj;
            fObjects[id].fObj = obj;

            // set objects
        } else if (token.isKeyword("trailer")) {
            long dummy;
            current = readTrailer(current, end, true, &dummy, true);
        } else if (token.isKeyword("startxref")) {
            token.reset();
            current = nextObject(0, current, end, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));  // ignore
        }

        current = skipPdfWhiteSpaces(0, current, end);
    }

    // TODO(edisonn): hack, detect root catalog - we need to implement liniarized support, and remove this hack.
    if (!fRootCatalogRef) {
        for (unsigned int i = 0 ; i < objects(); i++) {
            SkPdfNativeObject* obj = object(i);
            SkPdfNativeObject* root = (obj && obj->isDictionary()) ? obj->get("Root") : NULL;
            if (root && root->isReference()) {
                fRootCatalogRef = root;
            }
        }
    }


    if (fRootCatalogRef) {
        fRootCatalog = (SkPdfCatalogDictionary*)resolveReference(fRootCatalogRef);
        if (fRootCatalog->isDictionary() && fRootCatalog->valid()) {
            SkPdfPageTreeNodeDictionary* tree = fRootCatalog->Pages(this);
            if (tree && tree->isDictionary() && tree->valid()) {
                fillPages(tree);
            }
        }
    }


}

// TODO(edisonn): NYI
SkPdfNativeDoc::~SkPdfNativeDoc() {
    sk_free((void*)fFileContent);
    delete fAllocator;
}

const unsigned char* SkPdfNativeDoc::readCrossReferenceSection(const unsigned char* xrefStart, const unsigned char* trailerEnd) {
    SkPdfNativeObject xref;
    const unsigned char* current = nextObject(0, xrefStart, trailerEnd, &xref, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));

    if (!xref.isKeyword("xref")) {
        return trailerEnd;
    }

    SkPdfNativeObject token;
    while (current < trailerEnd) {
        token.reset();
        const unsigned char* previous = current;
        current = nextObject(0, current, trailerEnd, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
        if (!token.isInteger()) {
            return previous;
        }

        int startId = (int)token.intValue();
        token.reset();
        current = nextObject(0, current, trailerEnd, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));

        if (!token.isInteger()) {
            // TODO(edisonn): report/warning
            return current;
        }

        int entries = (int)token.intValue();

        for (int i = 0; i < entries; i++) {
            token.reset();
            current = nextObject(0, current, trailerEnd, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
            if (!token.isInteger()) {
                // TODO(edisonn): report/warning
                return current;
            }
            int offset = (int)token.intValue();

            token.reset();
            current = nextObject(0, current, trailerEnd, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
            if (!token.isInteger()) {
                // TODO(edisonn): report/warning
                return current;
            }
            int generation = (int)token.intValue();

            token.reset();
            current = nextObject(0, current, trailerEnd, &token, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
            if (!token.isKeyword() || token.lenstr() != 1 || (*token.c_str() != 'f' && *token.c_str() != 'n')) {
                // TODO(edisonn): report/warning
                return current;
            }

            addCrossSectionInfo(startId + i, generation, offset, *token.c_str() == 'f');
        }
    }
    // TODO(edisonn): it should never get here? there is no trailer?
    return current;
}

const unsigned char* SkPdfNativeDoc::readTrailer(const unsigned char* trailerStart, const unsigned char* trailerEnd, bool storeCatalog, long* prev, bool skipKeyword) {
    *prev = -1;

    const unsigned char* current = trailerStart;
    if (!skipKeyword) {
        SkPdfNativeObject trailerKeyword;
        // TODO(edisonn): use null allocator, and let it just fail if memory
        // needs allocated (but no crash)!
        current = nextObject(0, current, trailerEnd, &trailerKeyword, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));

        if (!trailerKeyword.isKeyword() || strlen("trailer") != trailerKeyword.lenstr() ||
            strncmp(trailerKeyword.c_str(), "trailer", strlen("trailer")) != 0) {
            // TODO(edisonn): report warning, rebuild trailer from objects.
            return current;
        }
    }

    SkPdfNativeObject token;
    current = nextObject(0, current, trailerEnd, &token, fAllocator, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
    if (!token.isDictionary()) {
        return current;
    }
    SkPdfFileTrailerDictionary* trailer = (SkPdfFileTrailerDictionary*)&token;
    if (!trailer->valid()) {
        return current;
    }

    if (storeCatalog) {
        SkPdfNativeObject* ref = trailer->Root(NULL);
        if (ref == NULL || !ref->isReference()) {
            // TODO(edisonn): oops, we have to fix the corrup pdf file
            return current;
        }
        fRootCatalogRef = ref;
    }

    if (trailer->has_Prev()) {
        *prev = (long)trailer->Prev(NULL);
    }

    return current;
}

void SkPdfNativeDoc::addCrossSectionInfo(int id, int generation, int offset, bool isFreed) {
    // TODO(edisonn): security here
    while (fObjects.count() < id + 1) {
        reset(fObjects.append());
    }

    fObjects[id].fOffset = offset;
    fObjects[id].fObj = NULL;
    fObjects[id].fResolvedReference = NULL;
    fObjects[id].fIsReferenceResolved = false;
}

SkPdfNativeObject* SkPdfNativeDoc::readObject(int id/*, int expectedGeneration*/) {
    long startOffset = fObjects[id].fOffset;
    //long endOffset = fObjects[id].fOffsetEnd;
    // TODO(edisonn): use hinted endOffset
    // TODO(edisonn): current implementation will result in a lot of memory usage
    // to decrease memory usage, we wither need to be smart and know where objects end, and we will
    // alocate only the chancks needed, or the tokenizer will not make copies, but then it needs to
    // cache the results so it does not go twice on the same buffer
    const unsigned char* current = fFileContent + startOffset;
    const unsigned char* end = fFileContent + fContentLength;

    SkPdfNativeTokenizer tokenizer(current, end - current, fAllocator, this);

    SkPdfNativeObject idObj;
    SkPdfNativeObject generationObj;
    SkPdfNativeObject objKeyword;
    SkPdfNativeObject* dict = fAllocator->allocObject();

    current = nextObject(0, current, end, &idObj, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
    if (current >= end) {
        // TODO(edisonn): report warning/error
        return NULL;
    }

    current = nextObject(0, current, end, &generationObj, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
    if (current >= end) {
        // TODO(edisonn): report warning/error
        return NULL;
    }

    current = nextObject(0, current, end, &objKeyword, NULL, NULL PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));
    if (current >= end) {
        // TODO(edisonn): report warning/error
        return NULL;
    }

    if (!idObj.isInteger() || !generationObj.isInteger() || id != idObj.intValue()/* || generation != generationObj.intValue()*/) {
        // TODO(edisonn): report warning/error
    }

    if (!objKeyword.isKeyword() || strcmp(objKeyword.c_str(), "obj") != 0) {
        // TODO(edisonn): report warning/error
    }

    current = nextObject(1, current, end, dict, fAllocator, this PUT_TRACK_STREAM_ARGS_EXPL2(0, fFileContent));

    // TODO(edisonn): report warning/error - verify last token is endobj

    return dict;
}

void SkPdfNativeDoc::fillPages(SkPdfPageTreeNodeDictionary* tree) {
    SkPdfArray* kids = tree->Kids(this);
    if (kids == NULL) {
        *fPages.append() = (SkPdfPageObjectDictionary*)tree;
        return;
    }

    int cnt = kids->size();
    for (int i = 0; i < cnt; i++) {
        SkPdfNativeObject* obj = resolveReference(kids->objAtAIndex(i));
        if (fMapper->mapPageObjectDictionary(obj) != kPageObjectDictionary_SkPdfNativeObjectType) {
            *fPages.append() = (SkPdfPageObjectDictionary*)obj;
        } else {
            // TODO(edisonn): verify that it is a page tree indeed
            fillPages((SkPdfPageTreeNodeDictionary*)obj);
        }
    }
}

int SkPdfNativeDoc::pages() const {
    return fPages.count();
}

SkPdfPageObjectDictionary* SkPdfNativeDoc::page(int page) {
    SkASSERT(page >= 0 && page < fPages.count());
    return fPages[page];
}


SkPdfResourceDictionary* SkPdfNativeDoc::pageResources(int page) {
    SkASSERT(page >= 0 && page < fPages.count());
    return fPages[page]->Resources(this);
}

// TODO(edisonn): Partial implemented. Move the logics directly in the code generator for inheritable and default value?
SkRect SkPdfNativeDoc::MediaBox(int page) {
    SkPdfPageObjectDictionary* current = fPages[page];
    while (!current->has_MediaBox() && current->has_Parent()) {
        current = (SkPdfPageObjectDictionary*)current->Parent(this);
    }
    if (current) {
        return current->MediaBox(this);
    }
    return SkRect::MakeEmpty();
}

// TODO(edisonn): stream or array ... ? for now only array
SkPdfNativeTokenizer* SkPdfNativeDoc::tokenizerOfPage(int page,
                                                         SkPdfAllocator* allocator) {
    if (fPages[page]->isContentsAStream(this)) {
        return tokenizerOfStream(fPages[page]->getContentsAsStream(this), allocator);
    } else {
        // TODO(edisonn): NYI, we need to concatenate all streams in the array or make the tokenizer smart
        // so we don't allocate new memory
        return NULL;
    }
}

SkPdfNativeTokenizer* SkPdfNativeDoc::tokenizerOfStream(SkPdfNativeObject* stream,
                                                           SkPdfAllocator* allocator) {
    if (stream == NULL) {
        return NULL;
    }

    return new SkPdfNativeTokenizer(stream, allocator, this);
}

// TODO(edisonn): NYI
SkPdfNativeTokenizer* SkPdfNativeDoc::tokenizerOfBuffer(const unsigned char* buffer, size_t len,
                                                           SkPdfAllocator* allocator) {
    // warning does not track two calls in the same buffer! the buffer is updated!
    // make a clean copy if needed!
    return new SkPdfNativeTokenizer(buffer, len, allocator, this);
}

size_t SkPdfNativeDoc::objects() const {
    return fObjects.count();
}

SkPdfNativeObject* SkPdfNativeDoc::object(int i) {
    SkASSERT(!(i < 0 || i > fObjects.count()));

    if (i < 0 || i > fObjects.count()) {
        return NULL;
    }

    if (fObjects[i].fObj == NULL) {
        // TODO(edisonn): when we read the cross reference sections, store the start of the next object
        // and fill fOffsetEnd
        fObjects[i].fObj = readObject(i);
    }

    return fObjects[i].fObj;
}

const SkPdfMapper* SkPdfNativeDoc::mapper() const {
    return fMapper;
}

SkPdfReal* SkPdfNativeDoc::createReal(double value) const {
    SkPdfNativeObject* obj = fAllocator->allocObject();
    SkPdfNativeObject::makeReal(value, obj PUT_TRACK_PARAMETERS_SRC);
    return (SkPdfReal*)obj;
}

SkPdfInteger* SkPdfNativeDoc::createInteger(int value) const {
    SkPdfNativeObject* obj = fAllocator->allocObject();
    SkPdfNativeObject::makeInteger(value, obj PUT_TRACK_PARAMETERS_SRC);
    return (SkPdfInteger*)obj;
}

SkPdfString* SkPdfNativeDoc::createString(const unsigned char* sz, size_t len) const {
    SkPdfNativeObject* obj = fAllocator->allocObject();
    SkPdfNativeObject::makeString(sz, len, obj PUT_TRACK_PARAMETERS_SRC);
    return (SkPdfString*)obj;
}

SkPdfAllocator* SkPdfNativeDoc::allocator() const {
    return fAllocator;
}

// TODO(edisonn): fix infinite loop if ref to itself!
// TODO(edisonn): perf, fix refs at load, and resolve will simply return fResolvedReference?
SkPdfNativeObject* SkPdfNativeDoc::resolveReference(SkPdfNativeObject* ref) {
    if (ref && ref->isReference()) {
        int id = ref->referenceId();
        // TODO(edisonn): generation/updates not supported now
        //int gen = ref->referenceGeneration();

        // TODO(edisonn): verify id and gen expected
        if (id < 0 || id >= fObjects.count()) {
            // TODO(edisonn): report error/warning
            return NULL;
        }

        if (fObjects[id].fIsReferenceResolved) {

#ifdef PDF_TRACE
            printf("\nresolve(%s) = %s\n", ref->toString(0).c_str(), fObjects[id].fResolvedReference->toString(0, ref->toString().size() + 13).c_str());
#endif

            // TODO(edisonn): for known good documents, assert here THAT THE REFERENCE IS NOT null
            return fObjects[id].fResolvedReference;
        }

        // TODO(edisonn): there are pdfs in the crashing suite that cause a stack overflow here unless we check for resolved reference on next line
        // determine if the pdf is corrupted, or we have a bug here

        // avoids recursive calls
        fObjects[id].fIsReferenceResolved = true;

        if (fObjects[id].fObj == NULL) {
            fObjects[id].fObj = readObject(id);
        }

        if (fObjects[id].fResolvedReference == NULL) {
            if (!fObjects[id].fObj->isReference()) {
                fObjects[id].fResolvedReference = fObjects[id].fObj;
            } else {
                fObjects[id].fResolvedReference = resolveReference(fObjects[id].fObj);
            }
        }

#ifdef PDF_TRACE
        printf("\nresolve(%s) = %s\n", ref->toString(0).c_str(), fObjects[id].fResolvedReference->toString(0, ref->toString().size() + 13).c_str());
#endif
        return fObjects[id].fResolvedReference;
    }

    // TODO(edisonn): fix the mess with const, probably we need to remove it pretty much everywhere
    return (SkPdfNativeObject*)ref;
}

size_t SkPdfNativeDoc::bytesUsed() const {
    return fAllocator->bytesUsed() +
           fContentLength +
           fObjects.count() * sizeof(PublicObjectEntry) +
           fPages.count() * sizeof(SkPdfPageObjectDictionary*) +
           sizeof(*this);
}
