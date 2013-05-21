/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkLuaCanvas.h"
#include "SkPicture.h"
#include "SkCommandLineFlags.h"
#include "SkGraphics.h"
#include "SkStream.h"
#include "SkData.h"
#include "picture_utils.h"
#include "SkOSFile.h"
#include "SkImageDecoder.h"

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

static const char gStartCanvasFunc[] = "sk_scrape_startcanvas";
static const char gEndCanvasFunc[] = "sk_scrape_endcanvas";
static const char gAccumulateFunc[] = "sk_scrape_accumulate";
static const char gSummarizeFunc[] = "sk_scrape_summarize";

// PictureRenderingFlags.cpp
extern bool lazy_decode_bitmap(const void* buffer, size_t size, SkBitmap*);


// Flags used by this file, alphabetically:
DEFINE_string2(skpPath, r, "", "Read .skp files from this dir");
DEFINE_string2(luaFile, l, "", "File containing lua script to run");

static SkPicture* load_picture(const char path[]) {
    SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(path));
    SkPicture* pic = NULL;
    if (stream.get()) {
        bool success;
        pic = SkNEW_ARGS(SkPicture, (stream.get(), &success,
                                     &lazy_decode_bitmap));
        if (!success) {
            SkDELETE(pic);
            pic = NULL;
        }
    }
    return pic;
}

static SkData* read_into_data(const char file[]) {
    SkAutoTUnref<SkStream> stream(SkStream::NewFromFile(file));
    if (!stream.get()) {
        return SkData::NewEmpty();
    }
    size_t len = stream->getLength();
    void* buffer = sk_malloc_throw(len);
    stream->read(buffer, len);
    return SkData::NewFromMalloc(buffer, len);
}

class SkAutoLua {
public:
    SkAutoLua(const char termCode[] = NULL) : fTermCode(termCode) {
        fL = luaL_newstate();
        luaL_openlibs(fL);
    }
    ~SkAutoLua() {
        if (fTermCode.size() > 0) {
            lua_getglobal(fL, fTermCode.c_str());
            if (lua_pcall(fL, 0, 0, 0) != LUA_OK) {
                SkDebugf("lua err: %s\n", lua_tostring(fL, -1));
            }
        }
        lua_close(fL);
    }

    lua_State* get() const { return fL; }
    lua_State* operator*() const { return fL; }
    lua_State* operator->() const { return fL; }

    bool load(const char code[]) {
        int err = luaL_loadstring(fL, code) || lua_pcall(fL, 0, 0, 0);
        if (err) {
            SkDebugf("--- lua failed\n");
            return false;
        }
        return true;
    }
    bool load(const void* code, size_t size) {
        SkString str((const char*)code, size);
        return load(str.c_str());
        int err = luaL_loadbufferx(fL, (const char*)code, size, NULL, NULL)
               || lua_pcall(fL, 0, 0, 0);
        if (err) {
            SkDebugf("--- lua failed\n");
            return false;
        }
        return true;
    }
private:
    lua_State* fL;
    SkString   fTermCode;
};

static void call_canvas(lua_State* L, SkLuaCanvas* canvas,
                        const char pictureFile[], const char funcName[]) {
    lua_getglobal(L, funcName);
    if (!lua_isfunction(L, -1)) {
        int t = lua_type(L, -1);
        SkDebugf("--- expected %s function %d, ignoring.\n", funcName, t);
        lua_settop(L, -2);
    } else {
        canvas->pushThis();
        lua_pushstring(L, pictureFile);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            SkDebugf("lua err: %s\n", lua_tostring(L, -1));
        }
    }
}

int tool_main(int argc, char** argv);
int tool_main(int argc, char** argv) {
    SkCommandLineFlags::SetUsage("apply lua script to .skp files.");
    SkCommandLineFlags::Parse(argc, argv);

    if (FLAGS_skpPath.isEmpty()) {
        SkDebugf(".skp files or directories are required.\n");
        exit(-1);
    }
    if (FLAGS_luaFile.isEmpty()) {
        SkDebugf("missing luaFile(s)\n");
        exit(-1);
    }

    SkAutoGraphics ag;
    SkAutoLua L(gSummarizeFunc);

    for (int i = 0; i < FLAGS_luaFile.count(); ++i) {
        SkAutoDataUnref data(read_into_data(FLAGS_luaFile[i]));
        SkDebugf("loading %s...\n", FLAGS_luaFile[i]);
        if (!L.load(data->data(), data->size())) {
            SkDebugf("failed to load luaFile %s\n", FLAGS_luaFile[i]);
            exit(-1);
        }
    }

    for (int i = 0; i < FLAGS_skpPath.count(); i ++) {
        SkOSFile::Iter iter(FLAGS_skpPath[i], "skp");
        SkString inputFilename;

        while (iter.next(&inputFilename)) {
            SkString inputPath;
            SkString inputAsSkString(FLAGS_skpPath[i]);
            sk_tools::make_filepath(&inputPath, inputAsSkString, inputFilename);

            const char* path = inputPath.c_str();
            SkDebugf("scraping %s\n", path);

            SkAutoTUnref<SkPicture> pic(load_picture(path));
            if (pic.get()) {
                SkAutoTUnref<SkLuaCanvas> canvas(
                                    new SkLuaCanvas(pic->width(), pic->height(),
                                                    L.get(), gAccumulateFunc));

                call_canvas(L.get(), canvas.get(), inputFilename.c_str(), gStartCanvasFunc);
                canvas->drawPicture(*pic);
                call_canvas(L.get(), canvas.get(), inputFilename.c_str(), gEndCanvasFunc);

            } else {
                SkDebugf("failed to load\n");
            }
        }
    }
    return 0;
}

#if !defined SK_BUILD_FOR_IOS
int main(int argc, char * const argv[]) {
    return tool_main(argc, (char**) argv);
}
#endif
