/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkGradientShader.h"

#include "SkTypeface.h"
#include "SkImageDecoder.h"
#include "SkStream.h"

static void setTypeface(SkPaint* paint, const char name[], SkTypeface::Style style) {
    SkSafeUnref(paint->setTypeface(SkTypeface::CreateFromName(name, style)));
}

class DownsampleBitmapGM : public skiagm::GM {

public:
    SkBitmap    fBM;
    SkString    fName;
    bool        fBitmapMade;
    
    DownsampleBitmapGM()
    {
        this->setBGColor(0xFFDDDDDD);
        fBitmapMade = false;
    }

    void setName(const char name[]) {
        fName.set(name);
    }

protected:
    virtual SkString onShortName() SK_OVERRIDE {
        return fName;
    }

    virtual SkISize onISize() SK_OVERRIDE {
        make_bitmap_wrapper();
        return SkISize::Make(4 * fBM.width(), fBM.height());
    }
    
    void make_bitmap_wrapper() {
        if (!fBitmapMade) {
            fBitmapMade = true;
            make_bitmap();
        }
    }

    virtual void make_bitmap() = 0;

    virtual void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        make_bitmap_wrapper();
        
        int curX = 0;
        int curWidth;
        float curScale = 1;
        do {
            
            SkMatrix matrix;
            matrix.setScale( curScale, curScale );
            
            SkPaint paint;
            paint.setFilterBitmap(true);
            paint.setFlags( paint.getFlags() | SkPaint::kHighQualityFilterBitmap_Flag );

            canvas->save();
            canvas->translate( (SkScalar) curX, 0.f );
            canvas->drawBitmapMatrix( fBM, matrix, &paint );
            canvas->restore();
            
            curWidth = (int) (fBM.width() * curScale + 2);
            curX += curWidth;
            curScale *= 0.75f;
        } while (curX < 4*fBM.width());
    }

private:
    typedef skiagm::GM INHERITED;
};

class DownsampleBitmapTextGM: public DownsampleBitmapGM {
  public:
      DownsampleBitmapTextGM(float textSize)
      : fTextSize(textSize)
        {
            char name[1024];
            sprintf(name, "downsamplebitmap_text_%.2fpt", fTextSize);
            setName(name);
        }

  protected:
      float fTextSize;

      virtual void make_bitmap() SK_OVERRIDE {
          fBM.setConfig(SkBitmap::kARGB_8888_Config, int(fTextSize * 8), int(fTextSize * 6));
          fBM.allocPixels();
          SkCanvas canvas(fBM);
          canvas.drawColor(SK_ColorWHITE);

          SkPaint paint;
          paint.setAntiAlias(true);
          paint.setSubpixelText(true);
          paint.setTextSize(fTextSize);

          setTypeface(&paint, "Times", SkTypeface::kNormal);
          canvas.drawText("Hamburgefons", 12, fTextSize/2, 1.2f*fTextSize, paint);
          setTypeface(&paint, "Times", SkTypeface::kBold);
          canvas.drawText("Hamburgefons", 12, fTextSize/2, 2.4f*fTextSize, paint);
          setTypeface(&paint, "Times", SkTypeface::kItalic);
          canvas.drawText("Hamburgefons", 12, fTextSize/2, 3.6f*fTextSize, paint);
          setTypeface(&paint, "Times", SkTypeface::kBoldItalic);
          canvas.drawText("Hamburgefons", 12, fTextSize/2, 4.8f*fTextSize, paint);
      }
  private:
      typedef DownsampleBitmapGM INHERITED;
};

class DownsampleBitmapCheckerboardGM: public DownsampleBitmapGM {
  public:
      DownsampleBitmapCheckerboardGM(int size, int numChecks)
      : fSize(size), fNumChecks(numChecks)
        {
            char name[1024];
            sprintf(name, "downsamplebitmap_checkerboard_%d_%d", fSize, fNumChecks);
            setName(name);
        }

  protected:
      int fSize;
      int fNumChecks;

      virtual void make_bitmap() SK_OVERRIDE {
          fBM.setConfig(SkBitmap::kARGB_8888_Config, fSize, fSize);
          fBM.allocPixels();
          SkAutoLockPixels lock(fBM);
          for (int y = 0; y < fSize; ++y) {
              for (int x = 0; x < fSize; ++x) {
                  SkPMColor* s = fBM.getAddr32(x, y);
                  int cx = (x * fNumChecks) / fSize;
                  int cy = (y * fNumChecks) / fSize;
                  if ((cx+cy)%2) {
                      *s = 0xFFFFFFFF;
                  } else {
                      *s = 0xFF000000;
                  }
              }
          }
      }
  private:
      typedef DownsampleBitmapGM INHERITED;
};

class DownsampleBitmapImageGM: public DownsampleBitmapGM {
  public:
      DownsampleBitmapImageGM(const char filename[])
      : fFilename(filename)
        {
            char name[1024];
            sprintf(name, "downsamplebitmap_image_%s", filename);
            setName(name);
        }

  protected:
      SkString fFilename;
      int fSize;

      virtual void make_bitmap() SK_OVERRIDE {
          SkString path(skiagm::GM::gResourcePath);
          path.append("/");
          path.append(fFilename);

          SkImageDecoder *codec = NULL;
          SkFILEStream stream(path.c_str());
          if (stream.isValid()) {
              codec = SkImageDecoder::Factory(&stream);
          }
          if (codec) {
              stream.rewind();
              codec->decode(&stream, &fBM, SkBitmap::kARGB_8888_Config,
                  SkImageDecoder::kDecodePixels_Mode);
              SkDELETE(codec);
          } else {
              fBM.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
              fBM.allocPixels();
              *(fBM.getAddr32(0,0)) = 0xFF0000FF; // red == bad
          }
          fSize = fBM.height();
      }
  private:
      typedef DownsampleBitmapGM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM( return new DownsampleBitmapTextGM(72); )
DEF_GM( return new DownsampleBitmapCheckerboardGM(512,256); )
DEF_GM( return new DownsampleBitmapImageGM("mandrill_512.png"); )
