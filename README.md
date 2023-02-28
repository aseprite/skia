# Skia for Aseprite and laf

**Pre-built binaries of Skia** in the [releases page](https://github.com/aseprite/skia/releases).

Skia is a 2D graphic library developed by Google Inc., you can find
the official website in [skia.org](https://skia.org).

This fork is used to compile Skia automatically for
[laf](https://github.com/aseprite/laf) and
[Aseprite](https://github.com/aseprite/aseprite) using GitHub Actions.

# Building Skia

In the following sections you will find straightforward steps to
compile Skia. You can always check the [official Skia
instructions](https://skia.org/user/build) and select the OS you are
building for. [Aseprite](https://github.com/aseprite/aseprite) and
[laf](https://github.com/aseprite/laf) use the **`aseprite-m102`** branch.
So remember to checkout that specific branch.

These are the platform-specific steps to compile Skia:

* [Skia on Windows](#skia-on-windows)
* [Skia on macOS](#skia-on-macos)
* [Skia on Linux](#skia-on-linux)

After this you should have all Skia libraries compiled. After that,
when you compile laf or Aseprite remember to add
`-DSKIA_DIR=$HOME/deps/skia` parameter to your `cmake` call and all
other parameters.

## Skia on Windows

Download
[Google depot tools](https://storage.googleapis.com/chrome-infra/depot_tools.zip)
and uncompress it in some place like `C:\deps\depot_tools`.

[It's recommended to compile Skia with Clang](https://github.com/google/skia/blob/master/site/user/build.md#a-note-on-software-backend-performance)
to get better performance. So you will need to [download Clang](https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/LLVM-13.0.0-win64.exe),
and install it on a folder like `C:\deps\llvm` (a folder without whitespaces).

Open a [developer command prompt](https://docs.microsoft.com/en-us/dotnet/framework/tools/developer-command-prompt-for-vs)
or command line (`cmd.exe`) and call:

    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" -arch=x64

Then:

    set PATH=C:\deps\depot_tools;%PATH%
    cd C:\deps\depot_tools
    gclient sync

(The `gclient` command might print an error like
`Error: client not configured; see 'gclient config'`.
Just ignore it.)

    cd C:\deps
    git clone -b aseprite-m102 https://github.com/aseprite/skia.git
    cd skia
    set GIT_EXECUTABLE=git.bat
    python tools/git-sync-deps

(The `tools/git-sync-deps` will take some minutes because it downloads
a lot of packages, please wait and re-run the same command in case it
fails.)

Finally, if you've downloaded Clang, use this command:

    set PATH=C:\deps\llvm\bin;%PATH%
    set PATH=C:\deps\depot_tools\bootstrap-3_8_0_chromium_8_bin\python\bin;%PATH%
    gn gen out/Release-x64 --args="is_debug=false is_official_build=true skia_use_system_expat=false skia_use_system_icu=false skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_libwebp=false skia_use_system_zlib=false skia_use_sfntly=false skia_use_freetype=true skia_use_harfbuzz=true skia_pdf_subset_harfbuzz=true skia_use_system_freetype2=false skia_use_system_harfbuzz=false target_cpu=""x64"" cc=""clang"" cxx=""clang++"" clang_win=""c:\deps\llvm"" win_vc=""C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC"" extra_cflags=[""-MT""]"
    ninja -C out/Release-x64 skia modules

If you haven't installed Clang, and want to compile Skia with MSVC
(anyway it's not recommended because the performance penalty is too
big), you can use the following commands instead:

    gn gen out/Release-x64 --args="is_debug=false is_official_build=true skia_use_system_expat=false skia_use_system_icu=false skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_libwebp=false skia_use_system_zlib=false skia_use_sfntly=false skia_use_freetype=true skia_use_harfbuzz=true skia_pdf_subset_harfbuzz=true skia_use_system_freetype2=false skia_use_system_harfbuzz=false target_cpu=""x64"" win_vc=""C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC"" extra_cflags=[""-MT""]"
    ninja -C out/Release-x64 skia modules

## Skia on macOS

These steps will create a `deps` folder in your home directory with a
couple of subdirectories needed to build Skia (you can change the
`$HOME/deps` with other directory). Some of these commands will take
several minutes to finish:

    mkdir $HOME/deps
    cd $HOME/deps
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    git clone -b aseprite-m102 https://github.com/aseprite/skia.git
    export PATH="${PWD}/depot_tools:${PATH}"
    cd skia
    python tools/git-sync-deps
    gn gen out/Release-x64 --args="is_debug=false is_official_build=true skia_use_system_expat=false skia_use_system_icu=false skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_libwebp=false skia_use_system_zlib=false skia_use_sfntly=false skia_use_freetype=true skia_use_harfbuzz=true skia_pdf_subset_harfbuzz=true skia_use_system_freetype2=false skia_use_system_harfbuzz=false target_cpu=\"x64\" extra_cflags=[\"-stdlib=libc++\", \"-mmacosx-version-min=10.9\"] extra_cflags_cc=[\"-frtti\"]"
    ninja -C out/Release-x64 skia modules

### Skia on macOS for Apple Silicon

If you want to compile Skia for Apple Silicon (e.g. M1), you have to
specify the `arm64` CPU architecture:

    gn gen out/Release-arm64 --args="is_debug=false is_official_build=true skia_use_system_expat=false skia_use_system_icu=false skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_libwebp=false skia_use_system_zlib=false skia_use_sfntly=false skia_use_freetype=true skia_use_harfbuzz=true skia_pdf_subset_harfbuzz=true skia_use_system_freetype2=false skia_use_system_harfbuzz=false target_cpu=\"arm64\" extra_cflags=[\"-stdlib=libc++\", \"-mmacosx-version-min=11.0\"] extra_cflags_cc=[\"-frtti\"]"
    ninja -C out/Release-arm64 skia modules

## Skia on Linux

These steps will create a `deps` folder in your home directory with a
couple of subdirectories needed to build Skia (you can change the
`$HOME/deps` with other directory). Some of these commands will take
several minutes to finish:

    mkdir $HOME/deps
    cd $HOME/deps
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    git clone -b aseprite-m102 https://github.com/aseprite/skia.git
    export PATH="${PWD}/depot_tools:${PATH}"
    cd skia
    python tools/git-sync-deps

Next step is to generate the compilation rules. You have two options, run just one:

1. Generate rules for compiling skia using clang (recommended if you
   have clang compiler):

        gn gen out/Release-x64 --args='is_debug=false is_official_build=true skia_use_system_expat=false skia_use_system_icu=false skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_libwebp=false skia_use_system_zlib=false skia_use_sfntly=false skia_use_freetype=true skia_use_harfbuzz=true skia_pdf_subset_harfbuzz=true skia_use_system_freetype2=false skia_use_system_harfbuzz=false cc="clang" cxx="clang++" extra_cflags_cc=["-stdlib=libc++"] extra_ldflags=["-stdlib=libc++"]'

2. Generate rules for compiling skia using the default compiler
   (usually g++):

        gn gen out/Release-x64 --args="is_debug=false is_official_build=true skia_use_system_expat=false skia_use_system_icu=false skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_libwebp=false skia_use_system_zlib=false skia_use_sfntly=false skia_use_freetype=true skia_use_harfbuzz=true skia_pdf_subset_harfbuzz=true skia_use_system_freetype2=false skia_use_system_harfbuzz=false"

Final step:

    ninja -C out/Release-x64 skia modules

**REMEMBER**: When compiling Aseprite or laf, check that you are using
the same compiler (clang or g++) as the one used for compiling skia.
