/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkPicture.h"
#include "include/core/SkSurface.h"
#include "include/utils/SkBase64.h"
#include "src/utils/SkJSONWriter.h"
#include "src/utils/SkMultiPictureDocument.h"
#include "tools/SkSharingProc.h"
#include "tools/UrlDataManager.h"
#include "tools/debugger/DebugCanvas.h"
#include "tools/debugger/DebugLayerManager.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <emscripten.h>
#include <emscripten/bind.h>

#if SK_SUPPORT_GPU
#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrContext.h"
#include "include/gpu/gl/GrGLInterface.h"
#include "include/gpu/gl/GrGLTypes.h"

#include <GL/gl.h>
#include <emscripten/html5.h>
#endif

using JSColor = int32_t;
using Uint8Array = emscripten::val;

// file signature for SkMultiPictureDocument
// TODO(nifong): make public and include from SkMultiPictureDocument.h
static constexpr char kMultiMagic[] = "Skia Multi-Picture Doc\n\n";

struct SimpleImageInfo {
  int width;
  int height;
  SkColorType colorType;
  SkAlphaType alphaType;
};

SkImageInfo toSkImageInfo(const SimpleImageInfo& sii) {
  return SkImageInfo::Make(sii.width, sii.height, sii.colorType, sii.alphaType);
}

SimpleImageInfo toSimpleImageInfo(const SkImageInfo& ii) {
  return (SimpleImageInfo){ii.width(), ii.height(), ii.colorType(), ii.alphaType()};
}

class SkpDebugPlayer {
  public:
    SkpDebugPlayer() :
    udm(UrlDataManager(SkString("/data"))){}

    /* loadSkp deserializes a skp file that has been copied into the shared WASM memory.
     * cptr - a pointer to the data to deserialize.
     * length - length of the data in bytes.
     * The caller must allocate the memory with M._malloc where M is the wasm module in javascript
     * and copy the data into M.buffer at the pointer returned by malloc.
     *
     * uintptr_t is used here because emscripten will not allow binding of functions with pointers
     * to primitive types. We can instead pass a number and cast it to whatever kind of
     * pointer we're expecting.
     */
    void loadSkp(uintptr_t cptr, int length) {
      const uint8_t* data = reinterpret_cast<const uint8_t*>(cptr);
      char magic[8];
      // Both traditional and multi-frame skp files have a magic word
      SkMemoryStream stream(data, length);
      SkDebugf("make stream at %p, with %d bytes\n",data, length);
      // Why -1? I think it's got to do with using a constexpr, just a guess.
      const size_t magicsize = sizeof(kMultiMagic) - 1;
      if (memcmp(data, kMultiMagic, magicsize) == 0) {
        SkDebugf("Try reading as a multi-frame skp\n");
        loadMultiFrame(&stream);
      } else {
        SkDebugf("Try reading as single-frame skp\n");
        frames.push_back(loadSingleFrame(&stream));
      }
    }

    /* drawTo asks the debug canvas to draw from the beginning of the picture
     * to the given command and flush the canvas.
     */
    void drawTo(SkSurface* surface, int32_t index) {
      // Set the command within the frame or layer event being drawn.
      if (fInspectedLayer >= 0) {
        fLayerManager->setCommand(fInspectedLayer, fp, index);
      } else {
        index = constrainFrameCommand(index);
      }

      auto* canvas = surface->getCanvas();
      canvas->clear(SK_ColorTRANSPARENT);
      if (fInspectedLayer >= 0) {
        // when it's a layer event we're viewing, we use the layer manager to render it.
        fLayerManager->drawLayerEventTo(canvas, fInspectedLayer, fp);
      } else {
        // otherwise, its a frame at the top level.
        frames[fp]->drawTo(surface->getCanvas(), index);
      }
      surface->getCanvas()->flush();
    }

    // Draws to the end of the current frame.
    void draw(SkSurface* surface) {
      auto* canvas = surface->getCanvas();
      canvas->clear(SK_ColorTRANSPARENT);
      frames[fp]->draw(surface->getCanvas());
      surface->getCanvas()->flush();
    }

    const SkIRect getBounds() {
      if (fInspectedLayer < 0) {
        return fBounds;
      }
      auto summary = fLayerManager->event(fInspectedLayer, fp);
      return SkIRect::MakeWH(summary.layerWidth, summary.layerHeight);
    }

    // returns the debugcanvas of the current frame, or the current draw event when inspecting
    // a layer.
    DebugCanvas* visibleCanvas() {
      if (fInspectedLayer >=0) {
        return fLayerManager->getEventDebugCanvas(fInspectedLayer, fp);
      } else {
        return frames[fp].get();
      }
    }

    // The following three operations apply to every debugcanvas because they are overdraw features.
    // There is only one toggle for them on the app, they are global settings.
    // However, there's not a simple way to make the debugcanvases pull settings from a central
    // location so we set it on all of them at once.
    void setOverdrawVis(bool on) {
      for (int i=0; i < frames.size(); i++) {
        frames[i]->setOverdrawViz(on);
      }
      fLayerManager->setOverdrawViz(on);
    }
    void setGpuOpBounds(bool on) {
      for (int i=0; i < frames.size(); i++) {
        frames[i]->setDrawGpuOpBounds(on);
      }
      fLayerManager->setDrawGpuOpBounds(on);
    }
    void setClipVizColor(JSColor color) {
      for (int i=0; i < frames.size(); i++) {
        frames[i]->setClipVizColor(SkColor(color));
      }
      fLayerManager->setClipVizColor(SkColor(color));
    }
    void setAndroidClipViz(bool on) {
      for (int i=0; i < frames.size(); i++) {
        frames[i]->setAndroidClipViz(on);
      }
      // doesn't matter in layers
    }
    // The two operations below only apply to the current frame, because they concern the command
    // list, which is unique to each frame.
    void deleteCommand(int index) {
      visibleCanvas()->deleteDrawCommandAt(index);
    }
    void setCommandVisibility(int index, bool visible) {
      visibleCanvas()->toggleCommand(index, visible);
    }
    int getSize() const {
      if (fInspectedLayer >=0) {
        return fLayerManager->event(fInspectedLayer, fp).commandCount;
      } else {
        return frames[fp]->getSize();
      }
    }
    int getFrameCount() const {
      return frames.size();
    }

    // Return the command list in JSON representation as a string
    std::string jsonCommandList(sk_sp<SkSurface> surface) {
      SkDynamicMemoryWStream stream;
      SkJSONWriter writer(&stream, SkJSONWriter::Mode::kFast);
      writer.beginObject(); // root
      visibleCanvas()->toJSON(writer, udm, surface->getCanvas());
      writer.endObject(); // root
      writer.flush();
      auto skdata = stream.detachAsData();
      // Convert skdata to string_view, which accepts a length
      std::string_view data_view(reinterpret_cast<const char*>(skdata->data()), skdata->size());
      // and string_view to string, which emscripten understands.
      return std::string(data_view);
    }

    // Gets the clip and matrix of the last command drawn
    std::string lastCommandInfo() {
      SkMatrix vm = visibleCanvas()->getCurrentMatrix();
      SkIRect clip = visibleCanvas()->getCurrentClip();

      SkDynamicMemoryWStream stream;
      SkJSONWriter writer(&stream, SkJSONWriter::Mode::kFast);
      writer.beginObject(); // root

      writer.appendName("ViewMatrix");
      DrawCommand::MakeJsonMatrix(writer, vm);
      writer.appendName("ClipRect");
      DrawCommand::MakeJsonIRect(writer, clip);

      writer.endObject(); // root
      writer.flush();
      auto skdata = stream.detachAsData();
      // Convert skdata to string_view, which accepts a length
      std::string_view data_view(reinterpret_cast<const char*>(skdata->data()), skdata->size());
      // and string_view to string, which emscripten understands.
      return std::string(data_view);
    }

    void changeFrame(int index) {
      fp = index;
    }

    // Return the png file at the requested index in
    // the skp file's vector of shared images. this is the set of images referred to by the
    // filenames like "\\1" in DrawImage commands.
    // Return type is the PNG data as a base64 encoded string with prepended URI.
    std::string getImageResource(int index) {
      sk_sp<SkData> pngData = fImages[index]->encodeToData();
      size_t len = SkBase64::Encode(pngData->data(), pngData->size(), nullptr);
      SkString dst;
      dst.resize(len);
      SkBase64::Encode(pngData->data(), pngData->size(), dst.writable_str());
      dst.prepend("data:image/png;base64,");
      return std::string(dst.c_str());
    }

    int getImageCount() {
      return fImages.size();
    }

    // Get the image info of one of the resource images.
    SimpleImageInfo getImageInfo(int index) {
      return toSimpleImageInfo(fImages[index]->imageInfo());
    }

    // return a list of layer draw events that happened at the beginning of this frame.
    std::vector<DebugLayerManager::LayerSummary> getLayerSummaries() {
      return fLayerManager->summarizeLayers(fp);
    }

    // When set to a valid layer index, causes this class to playback the layer draw event at nodeId
    // on frame fp. No validation of nodeId or fp is performed, this must be valid values obtained
    // from either fLayerManager.listNodesForFrame or fLayerManager.summarizeEvents
    // Set to -1 to return to viewing the top level animation
    void setInspectedLayer(int nodeId) {
      fInspectedLayer = nodeId;
    }

  private:

      // Loads a single frame (traditional) skp file from the provided data stream and returns
      // a newly allocated DebugCanvas initialized with the SkPicture that was in the file.
      std::unique_ptr<DebugCanvas> loadSingleFrame(SkMemoryStream* stream) {
        // note overloaded = operator that actually does a move
        sk_sp<SkPicture> picture = SkPicture::MakeFromStream(stream);
        if (!picture) {
          SkDebugf("Unable to deserialze frame.\n");
          return nullptr;
        }
        SkDebugf("Parsed SKP file.\n");
        // Make debug canvas using bounds from SkPicture
        fBounds = picture->cullRect().roundOut();
        std::unique_ptr<DebugCanvas> debugCanvas = std::make_unique<DebugCanvas>(fBounds);

        // Only draw picture to the debug canvas once.
        debugCanvas->drawPicture(picture);
        return debugCanvas;
      }

      void loadMultiFrame(SkMemoryStream* stream) {
        // Attempt to deserialize with an image sharing serial proc.
        auto deserialContext = std::make_unique<SkSharingDeserialContext>();
        SkDeserialProcs procs;
        procs.fImageProc = SkSharingDeserialContext::deserializeImage;
        procs.fImageCtx = deserialContext.get();

        int page_count = SkMultiPictureDocumentReadPageCount(stream);
        if (!page_count) {
          SkDebugf("Not a MultiPictureDocument");
          return;
        }
        SkDebugf("Expecting %d frames\n", page_count);

        std::vector<SkDocumentPage> pages(page_count);
        if (!SkMultiPictureDocumentRead(stream, pages.data(), page_count, &procs)) {
          SkDebugf("Reading frames from MultiPictureDocument failed");
          return;
        }

        fLayerManager = std::make_unique<DebugLayerManager>();

        int i = 0;
        for (const auto& page : pages) {
          // Make debug canvas using bounds from SkPicture
          fBounds = page.fPicture->cullRect().roundOut();
          std::unique_ptr<DebugCanvas> debugCanvas = std::make_unique<DebugCanvas>(fBounds);
          debugCanvas->setLayerManagerAndFrame(fLayerManager.get(), i);

          // Only draw picture to the debug canvas once.
          debugCanvas->drawPicture(page.fPicture);

          if (debugCanvas->getSize() <=0 ){
            SkDebugf("Skipped corrupted frame, had %d commands \n", debugCanvas->getSize());
            continue;
          }
          // If you don't set these, they're undefined.
          debugCanvas->setOverdrawViz(false);
          debugCanvas->setDrawGpuOpBounds(false);
          debugCanvas->setClipVizColor(SK_ColorTRANSPARENT);
          debugCanvas->setAndroidClipViz(false);
          frames.push_back(std::move(debugCanvas));
          i++;
        }
        fImages = deserialContext->fImages;

        udm.indexImages(fImages);
      }

      // constrains the draw command index to the frame's command list length.
      int constrainFrameCommand(int index) {
        int cmdlen = frames[fp]->getSize();
        if (index >= cmdlen) {
          SkDebugf("Constrained command index (%d) within this frame's length (%d)\n",
            index, cmdlen);
          return cmdlen-1;
        }
        return index;
      }

      // A vector of DebugCanvas, each one initialized to a frame of the animation.
      std::vector<std::unique_ptr<DebugCanvas>> frames;
      // The index of the current frame (into the vector above)
      int fp = 0;
      // The width and height of the animation. (in practice the bounds of the last loaded frame)
      SkIRect fBounds;
      // SKP version of loaded file.
      uint32_t fFileVersion;
      // image resources from a loaded file
      std::vector<sk_sp<SkImage>> fImages;

      // The URLDataManager here is a cache that accepts encoded data (pngs) and puts
      // numbers on them. We have our own collection of images (fImages) that was populated by the
      // SkSharingDeserialContext when mskp files are loaded which it can use for IDing images
      // without having to serialize them.
      UrlDataManager udm;

      // A structure holding the picture information needed to draw any layers used in an mskp file
      // individual frames hold a pointer to it, store draw events, and request images from it.
      // it is stateful and is set to the current frame at all times.
      std::unique_ptr<DebugLayerManager> fLayerManager;

      // The node id of a layer being inspected, if any.
      // -1 means we are viewing the top level animation, not a layer.
      // the exact draw event being inspected depends also on the selected frame `fp`.
      int fInspectedLayer = -1;
};

#if SK_SUPPORT_GPU
sk_sp<GrContext> MakeGrContext(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context)
{
    EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(context);
    if (r < 0) {
        SkDebugf("failed to make webgl context current %d\n", r);
        return nullptr;
    }
    // setup GrContext
    auto interface = GrGLMakeNativeInterface();
    // setup contexts
    sk_sp<GrContext> grContext(GrContext::MakeGL(interface));
    return grContext;
}

sk_sp<SkSurface> MakeOnScreenGLSurface(sk_sp<GrContext> grContext, int width, int height) {
    glClearColor(0, 0, 0, 0);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);


    // Wrap the frame buffer object attached to the screen in a Skia render
    // target so Skia can render to it
    GrGLint buffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
    GrGLFramebufferInfo info;
    info.fFBOID = (GrGLuint) buffer;
    SkColorType colorType;

    info.fFormat = GL_RGBA8;
    colorType = kRGBA_8888_SkColorType;

    GrBackendRenderTarget target(width, height, 0, 8, info);

    sk_sp<SkSurface> surface(SkSurface::MakeFromBackendRenderTarget(grContext.get(), target,
                                                                    kBottomLeft_GrSurfaceOrigin,
                                                                    colorType, nullptr, nullptr));
    return surface;
}

sk_sp<SkSurface> MakeRenderTarget(sk_sp<GrContext> grContext, int width, int height) {
    SkImageInfo info = SkImageInfo::MakeN32(width, height, SkAlphaType::kPremul_SkAlphaType);

    sk_sp<SkSurface> surface(SkSurface::MakeRenderTarget(grContext.get(),
                             SkBudgeted::kYes,
                             info, 0,
                             kBottomLeft_GrSurfaceOrigin,
                             nullptr, true));
    return surface;
}

sk_sp<SkSurface> MakeRenderTarget(sk_sp<GrContext> grContext, SimpleImageInfo sii) {
    sk_sp<SkSurface> surface(SkSurface::MakeRenderTarget(grContext.get(),
                             SkBudgeted::kYes,
                             toSkImageInfo(sii), 0,
                             kBottomLeft_GrSurfaceOrigin,
                             nullptr, true));
    return surface;
}
#endif

using namespace emscripten;
EMSCRIPTEN_BINDINGS(my_module) {

  // The main class that the JavaScript in index.html uses
  class_<SkpDebugPlayer>("SkpDebugPlayer")
    .constructor<>()
    .function("changeFrame",          &SkpDebugPlayer::changeFrame)
    .function("deleteCommand",        &SkpDebugPlayer::deleteCommand)
    .function("draw",                 &SkpDebugPlayer::draw, allow_raw_pointers())
    .function("drawTo",               &SkpDebugPlayer::drawTo, allow_raw_pointers())
    .function("getBounds",            &SkpDebugPlayer::getBounds)
    .function("getFrameCount",        &SkpDebugPlayer::getFrameCount)
    .function("getImageResource",     &SkpDebugPlayer::getImageResource)
    .function("getImageCount",        &SkpDebugPlayer::getImageCount)
    .function("getImageInfo",         &SkpDebugPlayer::getImageInfo)
    .function("getLayerSummaries",    &SkpDebugPlayer::getLayerSummaries)
    .function("getSize",              &SkpDebugPlayer::getSize)
    .function("jsonCommandList",      &SkpDebugPlayer::jsonCommandList, allow_raw_pointers())
    .function("lastCommandInfo",      &SkpDebugPlayer::lastCommandInfo)
    .function("loadSkp",              &SkpDebugPlayer::loadSkp, allow_raw_pointers())
    .function("setClipVizColor",      &SkpDebugPlayer::setClipVizColor)
    .function("setCommandVisibility", &SkpDebugPlayer::setCommandVisibility)
    .function("setGpuOpBounds",       &SkpDebugPlayer::setGpuOpBounds)
    .function("setInspectedLayer",    &SkpDebugPlayer::setInspectedLayer)
    .function("setOverdrawVis",       &SkpDebugPlayer::setOverdrawVis)
    .function("setAndroidClipViz",    &SkpDebugPlayer::setAndroidClipViz);

  // Structs used as arguments or returns to the functions above
  value_object<SkIRect>("SkIRect")
      .field("fLeft",   &SkIRect::fLeft)
      .field("fTop",    &SkIRect::fTop)
      .field("fRight",  &SkIRect::fRight)
      .field("fBottom", &SkIRect::fBottom);
  // emscripten provided the following convenience function for binding vector<T>
  // https://emscripten.org/docs/api_reference/bind.h.html#_CPPv415register_vectorPKc
  register_vector<DebugLayerManager::LayerSummary>("VectorLayerSummary");
  value_object<DebugLayerManager::LayerSummary>("DebugLayerManager::LayerSummary")
    .field("nodeId",            &DebugLayerManager::LayerSummary::nodeId)
    .field("frameOfLastUpdate", &DebugLayerManager::LayerSummary::frameOfLastUpdate)
    .field("fullRedraw",        &DebugLayerManager::LayerSummary::fullRedraw)
    .field("layerWidth",        &DebugLayerManager::LayerSummary::layerWidth)
    .field("layerHeight",       &DebugLayerManager::LayerSummary::layerHeight);

  // Symbols needed by cpu.js to perform surface creation and flushing.
  enum_<SkColorType>("ColorType")
    .value("RGBA_8888", SkColorType::kRGBA_8888_SkColorType);
  enum_<SkAlphaType>("AlphaType")
      .value("Opaque",   SkAlphaType::kOpaque_SkAlphaType)
      .value("Premul",   SkAlphaType::kPremul_SkAlphaType)
      .value("Unpremul", SkAlphaType::kUnpremul_SkAlphaType);
  value_object<SimpleImageInfo>("SkImageInfo")
    .field("width",     &SimpleImageInfo::width)
    .field("height",    &SimpleImageInfo::height)
    .field("colorType", &SimpleImageInfo::colorType)
    .field("alphaType", &SimpleImageInfo::alphaType);
  constant("TRANSPARENT", (JSColor) SK_ColorTRANSPARENT);
  function("_getRasterDirectSurface", optional_override([](const SimpleImageInfo ii,
                                                           uintptr_t /* uint8_t*  */ pPtr,
                                                           size_t rowBytes)->sk_sp<SkSurface> {
    uint8_t* pixels = reinterpret_cast<uint8_t*>(pPtr);
    SkImageInfo imageInfo = toSkImageInfo(ii);
    SkDebugf("Made raster direct surface.\n");
    return SkSurface::MakeRasterDirect(imageInfo, pixels, rowBytes, nullptr);
  }), allow_raw_pointers());
  class_<SkSurface>("SkSurface")
    .smart_ptr<sk_sp<SkSurface>>("sk_sp<SkSurface>")
    .function("width", &SkSurface::width)
    .function("height", &SkSurface::height)
    .function("_flush", select_overload<void()>(&SkSurface::flush))
    .function("getCanvas", &SkSurface::getCanvas, allow_raw_pointers());
  class_<SkCanvas>("SkCanvas")
    .function("clear", optional_override([](SkCanvas& self, JSColor color)->void {
      // JS side gives us a signed int instead of an unsigned int for color
      // Add a optional_override to change it out.
      self.clear(SkColor(color));
    }));

  #if SK_SUPPORT_GPU
    class_<GrContext>("GrContext")
        .smart_ptr<sk_sp<GrContext>>("sk_sp<GrContext>");
    function("currentContext", &emscripten_webgl_get_current_context);
    function("setCurrentContext", &emscripten_webgl_make_context_current);
    function("MakeGrContext", &MakeGrContext);
    function("MakeOnScreenGLSurface", &MakeOnScreenGLSurface);
    function("MakeRenderTarget", select_overload<sk_sp<SkSurface>(
      sk_sp<GrContext>, int, int)>(&MakeRenderTarget));
    function("MakeRenderTarget", select_overload<sk_sp<SkSurface>(
      sk_sp<GrContext>, SimpleImageInfo)>(&MakeRenderTarget));
    constant("gpu", true);
  #endif
}
