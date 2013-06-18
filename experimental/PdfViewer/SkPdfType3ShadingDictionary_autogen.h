#ifndef __DEFINED__SkPdfType3ShadingDictionary
#define __DEFINED__SkPdfType3ShadingDictionary

#include "SkPdfEnums_autogen.h"
#include "SkPdfArray_autogen.h"
#include "SkPdfShadingDictionary_autogen.h"

class SkPdfType3ShadingDictionary : public SkPdfShadingDictionary {
public:
  virtual SkPdfObjectType getType() const { return kObjectDictionaryShadingDictionaryType3ShadingDictionary_SkPdfObjectType;}
  virtual SkPdfObjectType getTypeEnd() const { return (SkPdfObjectType)(kObjectDictionaryShadingDictionaryType3ShadingDictionary_SkPdfObjectType + 1);}
public:
  virtual SkPdfType3ShadingDictionary* asType3ShadingDictionary() {return this;}
  virtual const SkPdfType3ShadingDictionary* asType3ShadingDictionary() const {return this;}

private:
  virtual SkPdfType1ShadingDictionary* asType1ShadingDictionary() {return NULL;}
  virtual const SkPdfType1ShadingDictionary* asType1ShadingDictionary() const {return NULL;}

  virtual SkPdfType2ShadingDictionary* asType2ShadingDictionary() {return NULL;}
  virtual const SkPdfType2ShadingDictionary* asType2ShadingDictionary() const {return NULL;}

  virtual SkPdfType4ShadingDictionary* asType4ShadingDictionary() {return NULL;}
  virtual const SkPdfType4ShadingDictionary* asType4ShadingDictionary() const {return NULL;}

  virtual SkPdfType5ShadingDictionary* asType5ShadingDictionary() {return NULL;}
  virtual const SkPdfType5ShadingDictionary* asType5ShadingDictionary() const {return NULL;}

  virtual SkPdfType6ShadingDictionary* asType6ShadingDictionary() {return NULL;}
  virtual const SkPdfType6ShadingDictionary* asType6ShadingDictionary() const {return NULL;}

public:
private:
public:
  SkPdfType3ShadingDictionary(const PdfMemDocument* podofoDoc = NULL, const PdfObject* podofoObj = NULL) : SkPdfShadingDictionary(podofoDoc, podofoObj) {}

  virtual bool valid() const {return true;}

  SkPdfType3ShadingDictionary& operator=(const SkPdfType3ShadingDictionary& from) {this->fPodofoDoc = from.fPodofoDoc; this->fPodofoObj = from.fPodofoObj; return *this;}

  SkPdfArray Coords() const {
    SkPdfArray ret;
    if (ArrayFromDictionary(fPodofoDoc, fPodofoObj->GetDictionary(), "Coords", "", &ret)) return ret;
    // TODO(edisonn): warn about missing required field, assert for known good pdfs
    return SkPdfArray();
  }

  SkPdfArray Domain() const {
    SkPdfArray ret;
    if (ArrayFromDictionary(fPodofoDoc, fPodofoObj->GetDictionary(), "Domain", "", &ret)) return ret;
    // TODO(edisonn): warn about missing required field, assert for known good pdfs
    return SkPdfArray();
  }

  SkPdfFunction Function() const {
    SkPdfFunction ret;
    if (FunctionFromDictionary(fPodofoDoc, fPodofoObj->GetDictionary(), "Function", "", &ret)) return ret;
    // TODO(edisonn): warn about missing required field, assert for known good pdfs
    return SkPdfFunction();
  }

  SkPdfArray Extend() const {
    SkPdfArray ret;
    if (ArrayFromDictionary(fPodofoDoc, fPodofoObj->GetDictionary(), "Extend", "", &ret)) return ret;
    // TODO(edisonn): warn about missing required field, assert for known good pdfs
    return SkPdfArray();
  }

};

#endif  // __DEFINED__SkPdfType3ShadingDictionary
