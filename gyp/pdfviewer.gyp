# GYP file to build pdfviewer.
#
# To build on Linux:
#  ./gyp_skia pdfviewer.gyp && make pdfviewer
#
{
  'variables': {
    'skia_warnings_as_errors': 0,
  },
  'includes': [
    'apptype_console.gypi',
  ],
  'targets': [
    {
      'target_name': 'libpdfviewer',
      'type': 'static_library',
      'cflags': ['-fexceptions'],
      'cflags_cc': ['-fexceptions'],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'sources': [
        '../experimental/PdfViewer/SkPdfBasics.cpp',
        '../experimental/PdfViewer/SkPdfFont.cpp',
        '../experimental/PdfViewer/SkPdfParser.cpp',
        '../experimental/PdfViewer/SkPdfUtils.cpp',
        '../experimental/PdfViewer/pdfparser/podofo/autogen/SkPdfPodofoMapper_autogen.cpp',
        '../experimental/PdfViewer/pdfparser/podofo/autogen/SkPdfHeaders_autogen.cpp',
      ],
      'include_dirs': [
        '../third_party/externals/podofo/src/base',
        '../third_party/externals/podofo/src',
        '../third_party/externals/podofo',
        '../tools',
        '../experimental/PdfViewer',
        '../experimental/PdfViewer/pdfparser',
        '../experimental/PdfViewer/pdfparser/podofo',
        '../experimental/PdfViewer/pdfparser/podofo/autogen',
        #'../experimental/PdfViewer/pdfparser/native',
        #'../experimental/PdfViewer/pdfparser/native/autogen',
      ],
      'dependencies': [
        'core.gyp:core',
        'effects.gyp:effects',
        'images.gyp:images',
        'pdf.gyp:pdf',
        'ports.gyp:ports',
        'tools.gyp:picture_utils',
        '../third_party/externals/podofo/podofo.gyp:podofo',
      ],
      'link_settings': {
        'libraries': [
        ],
      },
      'defines': [
        'BUILDING_PODOFO',
      ],
    },
    {
      'target_name': 'pdfviewer',
      'type': 'executable',
      'cflags': ['-fexceptions'],
      'cflags_cc': ['-fexceptions'],
      'cflags!': [ '-fno-exceptions' ],
      'cflags_cc!': [ '-fno-exceptions' ],
      'sources': [
        '../experimental/PdfViewer/pdf_viewer_main.cpp',
      ],
      'include_dirs': [
        '../third_party/externals/podofo/src/base',
        '../third_party/externals/podofo/src',
        '../third_party/externals/podofo',
        '../tools',
        '../experimental/PdfViewer',
        '../experimental/PdfViewer/autogen',
      ],
      'dependencies': [
        'core.gyp:core',
        'images.gyp:images',
        'libpdfviewer',
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
