# General Instructions

To compile Gargoyle you will need CMake 3.1 or newer, a C11 compiler, and a
C++14 compiler.

The following are required to build Gargoyle on Unix, in addition to a standard
development environment (compiler, linker, etc.):

- CMake
- pkg-config (or compatible, such as pkgconf)
- Qt (5 or 6)
- fmt
- Fontconfig
- FreeType
- libjpeg (8 or newer) or libjpeg-turbo (2.0 or newer)
- libpng (1.6 or newer)
- zlib

fmt is technically not a requirement, as Gargoyle includes its own
header-only copy, but using Gargoyle's copy will slow down the build and
produce a larger binary. By default Gargoyle will use a system fmt if it
finds one. This can be overridden with the `WITH_BUNDLED_FMT` configure
option (see below).

In addition, if you want sound support, there are two backends: SDL and
Qt. For SDL, you will need SDL2 and SDL2\_mixer. For Qt, you will need
QtMultimedia, libsndfile (with Vorbis support), mpg123, and libopenmpt.

If you want text-to-speech support on Unix, you will need
speech-dispatcher. Windows and macOS natively provide text-to-speech
APIs which are used on those platforms.

For Qt builds, KDE Frameworks can optionally be used. They are used in
one place at the moment: discovery of the user's preferred text editor.
Qt cannot determine a user's text editor, so for editing the config
file, Gargoyle instructs Qt to open the file, hoping that the user's
environment will launch it in a text editor. If KDE Frameworks are used,
the user's preferred text editor will be discovered and used directly to
open the config file. KDE Frameworks are only available with Qt5. The
following KDE Frameworks modules are required:

- extra-cmake-modules
- kio
- kservice

On Unix, Gargoyle can be built like any standard CMake project.
Something like:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make -j`nproc`
    $ make install

By default Gargoyle installs to /usr/local. This can be changed by setting
`CMAKE_INSTALL_PREFIX`:

    $ cmake .. -DCMAKE_INSTALL_PREFIX=/usr

In addition, Gargoyle supports the following options:

- `WITH_BABEL`: If true (the default), use the Treaty of Babel to
  extract author and title information and display in the window's
  titlebar

- `BUILD_SHARED_LIBS`: If true (the default), a shared libgarglk will be
  built.  Otherwise, it will be static.

- `WITH_INTERPRETERS`: If true (the default), interpreters will be built.

- `WITH_LAUNCHER`: If true (the default), the launcher (gargoyle binary)
  will be built.

- `WITH_QT6`: If true, and if the interface is Qt (currently true for
  all platforms except macOS), use Qt6 instead of Qt5. The default is
  false, i.e. use Qt5.

- `WITH_FREEDESKTOP`: If true (the default), install
  freedesktop.org-compliant desktop, application, and MIME files. This
  is available only on non-Apple Unix platforms.

- `WITH_KDE`: If true, KDE Frameworks will be used.

- `WITH_TTS`: Takes one of four values: "ON", "OFF", "AUTO", or "DYNAMIC".

    - If "AUTO" (the default), text-to-speech support is enabled if the
      build environment supports it, and the resulting binaries will not work
      unless they're run on a platform that also supports it. Windows and macOS
      always support TTS, but Unix requires speech-dispatcher to be installed.
      If the build environment does not support TTS, then it is disabled.

    - If "DYNAMIC", text-to-speech support is always enabled, and the resulting
      binaries will work without platform text-to-speech support. This value is
      non-default because it may require source-level changes to keep it
      working on Unix platforms if speech-dispatcher has ABI-breaking changes.

    - If "ON" (or any true value), TTS support is enabled if the build
      environment supports it; otherwise, CMake will abort. The resulting
      binaries will not work without platform text-to-speech support.

    - If "OFF" (or any false value), TTS support is disabled.

- `SOUND`: Takes one of three values: "SDL", for SDL sound support,
  "QT", for Qt sound support, and "none" (or any other value), for no
  sound support.

- `JPEGLIB`: Gargoyle can use either the original libjpeg from the Independent
  JPEG Group (IJG), or libjpeg-turbo. There is no practical difference between
  the two for Gargoye's purposes, but Gargoyle does make use of features
  introduced in version 8 of IJG's library, which may not be available on all
  systems. To force libjpeg-turbo, set this value to `TURBO`. To force IJG
  libjpeg, set it to `IJG`. The default is `AUTO`, which first tries
  libjpeg-turbo, and if that fails, IJG libjpeg.

- `WITH_BUNDLED_FMT`: If true, prefer Gargoyle's bundled fmt library to
  the system library. Defaults to false.

As with any standard CMake-based project, DESTDIR can be used to install to a
staging area:

    $ make install DESTDIR=$PWD/Staging

Gargoyle will install headers to `${CMAKE_INSTALL_PREFIX}/include/gargoyle` and
libgarglk to `${CMAKE_INSTALL_PREFIX}/lib`. This allows Glk programs to easily
build against libgarglk without needing to be integrated with Gargoyle.
However, note that the shared library makes no promises of API or ABI stability,
and as such, is not currently versioned. The exception is the Glk API, which
will remain stable, but any Gargoyle-specific extensions are subject to change
without notice, as is the ABI. In short, if libgarglk is updated, any programs
linked against libgarglk.so should be rebuilt.

For development purposes, the environment variable `$GARGLK_INTERPRETER_DIR` can
be set to the interpreter build directory to allow the gargoyle binary to load
the newly-built interpreters instead of the system-wide interpreters (or instead
of failing if there are no interpreters installed). If this is set, the standard
directory will *not* be used at all, even if no interpreter is found. Example:

    $ cd build
    $ export GARGLK_INTERPRETER_DIR=$PWD/terps
    $ ./garglk/gargoyle

For Windows, the only supported build method is MinGW on Linux cross compiling
for Windows. The "install" target will install all binaries to `build/dist`. See
`windows.sh` at the top level for a script which is used to build the Windows
installer.

# System-specific instructions

## Building on Fedora GNU/Linux (37, 38): (notes by Andreas Davour, December 2019; updated April 2023)

To build cleanly with CMake, you first need to install the pre-requisite packages:

    sudo dnf install cmake fmt-devel fontconfig-devel freetype-devel gcc-c++ libpng-devel qt5-qtbase-devel qt5-qtbase turbojpeg-devel zlib-devel

If you want to compile with SDL support, you also need to install the development packages for that:

    sudo dnf install SDL2 SDL2-devel SDL2_mixer SDL2_mixer-devel

After that, just follow the general instructions above.

Note that on Fedora, /usr/local/lib is not included in the default search path,
so you may want to pass `-DCMAKE_INSTALL_PREFIX=/usr`. However, you may instead
want to use the provided `gargoyle-buildrpm.sh` script as a guide to build an
RPM package so that your package manager can track the Gargoyle installation.

## Building on MacOS: (notes by Andrew Plotkin, April 2017)

The new build script (contributed by Brad Town) can be run on any
MacOS 10.7 or higher. It requires a set of Unix packages (see below),
which can be installed with either MacPorts or Homebrew. If you're
building Gargoyle for your own use, that's all you need to know.

For the MacPorts packages you'll need, type:

    sudo port install libsdl2
    sudo port install libsdl2_mixer
    sudo port install libpng
    sudo port install libjpeg-turbo
    sudo port install freetype
    sudo port install pkgconfig

If you're using Homebrew, instead type:

    brew install sdl2
    brew install sdl2_mixer
    brew install libpng
    brew install libjpeg-turbo
    brew install libvorbis
    brew install freetype
    brew install pkg-config

Once the packages are installed, type:

    sh gargoyle_osx.sh

This should create Gargoyle.app, and also build a .DMG file which
contains the app.

However, to create a *release build* of Gargoyle.app -- one that can
be distributed to other people -- requires more care. After some
struggle, this is what I have learned:

- You need an Intel Mac with the developer tools (Xcode) installed.
  No surprise there.

- You should compile on the oldest MacOS available. You can build on an
  old Mac and get the result to run on new Macs. The reverse does not work
  reliably.

Therefore, I did the current build on MacOS 10.7 "Lion". (Support files
in this package are now set up for 10.7, and the resulting app runs on
10.7 and up. Unlike previous Gargoyle releases, it does not support the
PPC architecture.)

- You need to install Unix packages with MacPorts.

Homebrew, sadly, does not support 10.7 any more. (I tried to install the
necessary packages via Homebrew but ran into hopeless package dependency
loops.)

(Happily, Homebrew and MacPorts can coexist on the same box. I recommend
doing the build in a shell which can only see the MacPorts world: add
/opt/local/bin to your PATH and *remove* /usr/local/bin.)

### Building

To set up your build environment on a fresh Mac, install MacPorts
via https://www.macports.org/install.php. You want the "pkg" installer
for Lion (assuming that's your OS).

Once that's set up, run the MacPorts package install commands listed
above (the "sudo port" ones).

You can then run the build:

    sh gargoyle_osx.sh

If you have both Homebrew and MacPorts installed, the build will default
to Homebrew, which is not what you want. You must set the `MAC_USEHOMEBREW`
env variable to "no", for example like this:

    sh -c 'MAC_USEHOMEBREW=no; . gargoyle_osx.sh'

You now have build Gargoyle.app, and also the distributable .DMG file
which contains the app.

### Verifying the build

It's useful to know how to check the app for portableness. If you type a
command like

    otool -L Gargoyle.app/Contents/MacOS/Gargoyle
    otool -L Gargoyle.app/Contents/PlugIns/glulxe

...you will see a list of the dynamic libraries linked into the interpreter.
These may start with

    @executable_path/../Frameworks/...: libs included in Gargoyle.app
    /System/Library/Frameworks/...: MacOS standard libs
    /usr/lib/...: more MacOS standard libs

If you see any lines starting with /opt/local or /usr/local, you have a
problem. That library is trying to load out of MacPorts or Homebrew, and
the interpreter will fail on most other Macs (which do not have those repos
installed).

### MacOS code signing and notarization

For a fully accessible build on MacOS 10.15 (Catalina), you need to both
sign and notarize the app before packaging it up. (Unsigned builds still
run on 10.15, but you have to right-click and select "Open". The user
warnings for just launching the app normally are more derisive than before.)

To do this:

First, obviously, you need an Apple developer account.

Create an app-specific password with the name "altool". Copy the password
down once it's created. (You can't re-download it from Apple, you can only
delete it and create a new one.) The instructions on this are here:

https://support.apple.com/en-us/HT204397 "Using app-specific passwords"

Verify that this password works:

    xcrun altool --list-providers -u "$APPLE_ID"

`APPLE_ID` is the account name (probably an email address) for your
developer account. This will ask for a password. Use the altool password
that you just created above. The response will look something like:

    ProviderName   ProviderShortname      WWDRTeamID
    -------------- ---------------------- ----------
    Andrew Plotkin AndrewPlotkin176400769 BK75QRDQ9E

Build Gargoyle.app using the `gargoyle_osx.sh`. But delete the .dmg file;
we're going to create a new one later.

Code-sign the app: (The "-o runtime" argument specifies that you want
a "hardened" app, which is a requirement.)

    codesign -o runtime --deep --sign "$CERT_NAME" Gargoyle.app

The `CERT_NAME` is the name of your Developer ID Application certificate
in Keychain-Access. For me, this is:

"Developer ID Application: Andrew Plotkin (BK75QRDQ9E)".

Zip up the signed app for notarization:

    ditto -c -k --keepParent Gargoyle.app Gargoyle.zip

Send it off to be notarized:

    xcrun altool --notarize-app --primary-bundle-id 'com.googlecode.garglk.Launcher' --username "$APPLE_ID" --asc-provider "$PROV_SHORT_NAME" --file Gargoyle.zip

This will also ask for the altool password. `PROV_SHORT_NAME` is the
ProviderShortname you saw above. For me, it's AndrewPlotkin176400769.

If this succeeds, you'll see something like:

    No errors uploading 'Gargoyle.zip'.
    RequestUUID = 4745e9e4-87c2-4509-89cf-d08d84181234

Wait for Apple to send you email about this request. It's supposed to take "up to an hour"; I got a response within a few minutes.

Now look at the response details:

    xcrun altool --notarization-info "$REQ_UUID" -u "$APPLE_ID"

This will give a success/failure message, plus a LogFileURL for more
info. Look at the log file to make sure there are neither errors nor
warnings.

If it all looks good, update the app with the notarization info:

    xcrun stapler staple Gargoyle.app

(This doesn't need any extra arguments because Apple maintains a
public database of all successful notarizations. The stapler tool
looks up all necessary info online.)

Finally, re-run the command to create the DMG file:

    hdiutil create -fs "HFS+J" -ov -srcfolder Gargoyle.app/ gargoyle-2023.1-mac.dmg

## Building for Windows

### MinGW

The only supported method for building a Windows version of Gargoyle is
to cross compile with MinGW on Linux. In this way, Windows builds are
treated essentially as though they're Unix builds. As such, MinGW
versions of all packages listed above are required, as is a MinGW
toolchain.

To cross compile with CMake, a toolchain file is used:

    $ env MINGW_TRIPLE=i686-w64-mingw32 MINGW_LOCATION=/usr cmake .. -DCMAKE_TOOLCHAIN_FILE=../Toolchain.cmake
    $ make -j`nproc`

Releases are created with the `windows.sh` script, which builds Gargoyle
with MinGW and then creates an installer via the `install.nsi` file.
This uses the Nullsoft Scriptable Install System, which is available for
Linux.

### Clang on Windows

Support for building Gargoyle on Windows using Clang for Windows is currently
experimental. Some interpreters to be added in the future may only be supported
under this configuration. This assumes that the Clang-for-Windows executables
are in PATH, that you want to use Qt6, that you'll use Qt for sound, and that
the relevant Windows SDKs are also installed (if in doubt, install Visual Studio
with C++ workloads).

We recommend installing Qt via the official Qt installer, and using [vcpkg](https://vcpkg.io)
for the remaining dependencies (adjust triplet as needed):

```
vcpkg install freetype sndfile mpg123 libopenmpt libjpeg-turbo --triplet x64-windows
```

Invoke cmake as follows (adjust Qt and vcpkg paths accordingly):

```
cmake -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link -DINTERFACE=QT -DWITH_QT6=ON -DCMAKE_PREFIX_PATH=D:/Qt/6.5.2/msvc2019_64 -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DSOUND=QT ..
```

Building an installer from this configuration is not yet supported.

## Building on Haiku

The following packages are needed:

    $ pkgman install cmake freetype_devel qt5_devel qt5_tools fontconfig_devel libsndfile_devel mpg123_devel libopenmpt_devel

For sound support, Qt must be used instead of SDL. This looks to be
because both Qt and SDL create a `BApplication` instance, but there can
be only one such instance.

To compile:

    $ mkdir build
    $ cd build
    $ cmake .. -DSOUND=QT
    $ make

At the moment there is no installation procedure for Haiku, but
interpreters can be invividually run out of the build directory, or the
front-end launcher can be run out of the build directory if
`GARGLK_INTERPRETER_DIR` is set as described above.
