# HarmonyOS

## What is HarmonyOS?

(This is my current understanding, please correct these docs if incorrect.)

HarmonyOS is an operating system for tablets and phones. It is developed by
[Huawei](https://www.huawei.com/en/). It is competitive with platforms like
Android and iOS, capable of powering high-end phones. Unlike Google and Apple,
Huawei is a Chinese company, and HarmonyOS is largely serving the Chinese
market at this time.

The system uses some custom pieces, but also relies on open source software
from outside of the project, such as the Linux kernel, musl, etc.

Much of the platform is open source, under the name "OpenHarmony," and Huawei's
proprietary spin of OpenHarmony is called HarmonyOS. Huawei is the primary
contributor to OpenHarmony.

So it is probably best said: HarmonyOS is to Android what OpenHarmony is to
AOSP. But, to be clear, OpenHarmony is not derived from Android.

This document will generally refer to HarmonyOS, but from a development
viewpoint, you are actually targeting OpenHarmony, even if you're building
with intent to only target HarmonyOS on Huawei-produced phones.


## Some basic target platform truths

The C/C++ compiler is Clang. The platform triplet is "aarch64-linux-ohos"
(other CPU architectures are possible, but this is likely the most common. The
devtools mention 32-bit ARM and x86-64, too). The CMake toolchain file sets
the variable `OHOS` (OpenHarmony OS) to "OHOS", so `if(OHOS)` works. Further,
`CMAKE_SYSTEM_NAME` is set to "OHOS". The C preprocessor defines `__OHOS__`.


## Get HarmonyOS development tools

You will need to download Huawei's developer tools. They offer a full IDE,
called "DevEco Studio," but this is not necessary for compiling SDL. There are
also simple command line tools available for Windows, macOS, and Linux, and
these are sufficient to compile SDL.

To download, you'll need to register an account at
[the Huawei developer site](https://developer.huawei.com/en/). Then visit
[the tool download page](https://developer.huawei.com/consumer/en/download/)
and scroll down to "Command Line Tools." Download for your platform, which
should be a simple .zip file, and unpack it.

I unzipped this so it's sitting in `$HOME/HarmonyOS-SDK/command-line-tools`,
but anywhere you want to put it is fine.

When this document refers to this directory, it'll call it `$CTL` for short.


## Building SDL for HarmonyOS

SDL for HarmonyOS is built with CMake, like other platforms. Please note that
you _must_ use the CMake that comes with HarmonyOS's development tools, or
building will fail, as standard CMake does not recognize this platform at all
currently.

Generate a project file for building SDL. We use Ninja, because it's a fast
command line tool that will maximize multi-CPU compiling, but it's probably
possible to generate a real Visual Studio, Xcode, etc, project.

```bash
cd SDL
mkdir buildbot-ohos   # or whatever you want to call the build directory.
cd buildbot-ohos
$CTL/sdk/default/openharmony/native/build-tools/cmake/bin/cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=$CTL/sdk/default/openharmony/native/build/cmake/ohos.toolchain.cmake -DCMAKE_BUILD_TYPE=Release ..
```

(One can also add -DOHOS_ARCH=cputype, where "cputype" is "arm64-v8a" for ARM64, "armeabi-v7a" for ARM32, and "x86_64" Intel 64-bit. It defaults to ARM64 if unspecified.)


If all went well, you should be able to build SDL for HarmonyOS now. In our
case, build with Ninja:

```bash
ninja
```

Or use whatever instructs Xcode/Visual Studio/etc to build things.


## SDL platform defines

SDL will define SDL_PLATFORM_OPENHARMONY and SDL_PLATFORM_UNIX. Do be cautioned
that while HarmonyOS uses the Linux kernel, it is _not_ comparable to a
"normal" Linux system in almost any way, so SDL_PLATFORM_LINUX is not defined
(SDL for Android follows this same convention).


## Dynamic API

Presumably users cannot override the SDL build on their phones, so the Dynamic
API is disabled on this platform.


# GPU support

OpenGL ES 2 and 3 are supported, as is Vulkan. Desktop OpenGL is supported by
the platform, too, but for now support is disabled within SDL itself. EGL is
used to manage GL contexts internally, and a HarmonyOS-specific Vulkan
extension is provided to get a surface.

