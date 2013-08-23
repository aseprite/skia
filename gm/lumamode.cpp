/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkCanvas.h"
#include "SkGradientShader.h"
#include "SkLumaXfermode.h"

static unsigned kSize   = 80;
static unsigned kInset  = 10;
static SkColor  kColor1 = SkColorSetARGB(0xff, 0xff, 0xff, 0);
static SkColor  kColor2 = SkColorSetARGB(0xff, 0x80, 0xff, 0);

static void draw_label(SkCanvas* canvas, const char* label,
                       const SkPoint& offset) {
    SkPaint paint;
    size_t len = strlen(label);

    SkScalar width = paint.measureText(label, len);
    canvas->drawText(label, len, offset.x() - width / 2, offset.y(),
                     paint);
}

static void draw_scene(SkCanvas* canvas, SkXfermode* mode, bool aa,
                       SkShader* s1, SkShader* s2) {
    SkPaint paint;
    paint.setAntiAlias(aa);
    SkRect r, c, bounds = SkRect::MakeWH(kSize, kSize);

    c = bounds;
    c.fRight = bounds.centerX();
    canvas->drawRect(bounds, paint);

    canvas->saveLayer(&bounds, NULL);

    r = bounds;
    r.inset(kInset, 0);
    paint.setShader(s1);
    paint.setColor(s1 ? SK_ColorBLACK : SkColorSetA(kColor1, 0x80));
    canvas->drawOval(r, paint);
    if (!s1) {
        canvas->save();
        canvas->clipRect(c);
        paint.setColor(s1 ? SK_ColorBLACK : kColor1);
        canvas->drawOval(r, paint);
        canvas->restore();
    }

    SkPaint xferPaint;
    xferPaint.setXfermode(mode);
    canvas->saveLayer(&bounds, &xferPaint);

    r = bounds;
    r.inset(0, kInset);
    paint.setShader(s2);
    paint.setColor(s2 ? SK_ColorBLACK : SkColorSetA(kColor2, 0x80));
    canvas->drawOval(r, paint);
    if (!s2) {
        canvas->save();
        canvas->clipRect(c);
        paint.setColor(s2 ? SK_ColorBLACK : kColor2);
        canvas->drawOval(r, paint);
        canvas->restore();
    }

    canvas->restore();
    canvas->restore();
}

class LumaXfermodeGM : public skiagm::GM {
public:
    LumaXfermodeGM() {
        fSrcInXfer.reset(SkLumaMaskXfermode::Create(SkXfermode::kSrcIn_Mode));
        fDstInXfer.reset(SkLumaMaskXfermode::Create(SkXfermode::kDstIn_Mode));

        SkColor  g1Colors[] = { kColor1, SkColorSetA(kColor1, 0x20) };
        SkColor  g2Colors[] = { kColor2, SkColorSetA(kColor2, 0x20) };
        SkPoint  g1Points[] = { {0, 0}, {0, 100} };
        SkPoint  g2Points[] = { {0, 0}, {kSize, 0} };
        SkScalar pos[] = { 0.2f, 1.0f };

        fGr1.reset(SkGradientShader::CreateLinear(g1Points,
                                                  g1Colors,
                                                  pos,
                                                  SK_ARRAY_COUNT(g1Colors),
                                                  SkShader::kClamp_TileMode));
        fGr2.reset(SkGradientShader::CreateLinear(g2Points,
                                                  g2Colors,
                                                  pos,
                                                  SK_ARRAY_COUNT(g2Colors),
                                                  SkShader::kClamp_TileMode));
    }

protected:
    virtual SkString onShortName() SK_OVERRIDE {
        return SkString("lumamode");
    }

    virtual SkISize onISize() SK_OVERRIDE {
        return SkISize::Make(600, 420);
    }

    virtual void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        SkXfermode* modes[] = { NULL, fSrcInXfer, fDstInXfer };
        struct {
            SkShader*   fShader1;
            SkShader*   fShader2;
        } shaders[] = {
            { NULL, NULL },
            { NULL, fGr2 },
            { fGr1, NULL },
            { fGr1, fGr2 },
        };

        draw_label(canvas, "SrcOver",
                   SkPoint::Make(kSize + 2 * kInset, 20));
        draw_label(canvas, "SrcInLuma",
                   SkPoint::Make(3 * (kSize + 2 * kInset), 20));
        draw_label(canvas, "DstInLuma",
                   SkPoint::Make(5 * (kSize + 2 * kInset), 20));
        for (size_t i = 0; i < SK_ARRAY_COUNT(shaders); ++i) {
            canvas->save();
            canvas->translate(kInset, 30 + i * (kSize + 2 * kInset));
            for (size_t m = 0; m < SK_ARRAY_COUNT(modes); ++m) {
                draw_scene(canvas, modes[m], true, shaders[i].fShader1,
                           shaders[i].fShader2);
                canvas->translate(kSize + 2 * kInset, 0);
                draw_scene(canvas, modes[m], false, shaders[i].fShader1,
                           shaders[i].fShader2);
                canvas->translate(kSize + 2 * kInset, 0);
            }
            canvas->restore();
        }
    }

private:
    SkAutoTUnref<SkShader>   fGr1, fGr2;
    SkAutoTUnref<SkXfermode> fSrcInXfer, fDstInXfer;

    typedef skiagm::GM INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

DEF_GM( return SkNEW(LumaXfermodeGM); )
