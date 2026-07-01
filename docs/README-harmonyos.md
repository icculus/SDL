# HarmonyOS

(And also OpenHarmony.)

## What is HarmonyOS?

(This is my current understanding, please correct these docs if incorrect.)

HarmonyOS is an operating system for tablets and phones. It is developed by
[Huawei](https://www.huawei.com/en/). It is competitive with platforms like
Android and iOS, capable of powering high-end phones. Unlike Google and Apple,
Huawei is a Chinese company, and HarmonyOS is largely serving the Chinese
market at this time.

The system uses some custom pieces, but also relies on open source software
from outside of the project, such as musl, etc.

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
called "DevEco Studio," but this is not strictly necessary for compiling SDL
itself.

There are also command line tools available for Windows, macOS, and Linux, and
these _are_ sufficient to compile SDL. For building full applications, you
likely need DevEco Studio to at least set up your project and manage signing
keys, etc. Some things (but not all) are doable from the command line tools,
and a handful of things are (currently) not.

DevEco Studio, the GUI, is only available for Windows and macOS. It appears to
be a fork of IntelliJ IDEA. It does not appear to be open source, afaict.

To download the developer tools, you'll need to register an account at
[the Huawei developer site](https://developer.huawei.com/en/). Then visit
[the tool download page](https://developer.huawei.com/consumer/en/download/)
For Linux, scroll down to "Command Line Tools." Download for your platform,
which should be a simple .zip file, and unpack it. If you're on Windows or
macOS, download and install DevEco Studio itself while you're here, too.


## Don't fight against DevEco Studio

I'm a Linux user, and I avoid programming IDEs in general, so my instinct is
to just get everything done with my text editor and the command line tools,
and this _feels_ possible, but I haven't quite figured it out. The system is
friendly to things like CMake, clang, etc, and the documentation, even in
English, is pretty thorough, but I still couldn't get there without using
DevEco Studio a little.

The system appears designed to at least hand off a working project from
DevEco Studio to a Linux machine for building and deploying from the command
line tools, for Continuous Integration if nothing else. Figuring out how to
eliminate the Windows/Mac GUI dependency entirely is an ongoing and unresolved
concern. I definitely needed it to create and configure a project, and manage
signing keys. Once I got that far, I was able to copy the appropriate
directories to a Linux system and use the `hvigorw` command line tool to build
and sign an app, and `hdc` to install it on a real phone.


## Install command line tools.

(If you're entirely focused on DevEco Studio, you can probably skip this.)

I unzipped the downloaded command line tools so they're sitting in
`$HOME/HarmonyOS-SDK/command-line-tools`, but anywhere you want to put it is
fine.

When this document refers to this directory, it'll call it `$CLT` for short.

Add some things to your PATH, so you can find `hdc` and `hvigorw`, etc.

```bash
export PATH=$PATH:$CLT/sdk/default/openharmony/toolchains:$CLT/bin
```


## Putting a HarmonyOS phone in developer mode.

If you want to run your SDL apps natively on a real phone directly, you have
to put the device in Developer Mode. Any retail phone running HarmonyOS should
be able to do this. This is almost identical to how one would do this for
local Android development.

On the phone, go to Settings, Device Name (it's "Mate 80" on mine, in a box by
itself near the top of the main settings page). This will bring up an "About"
page. Scroll down to "Software version" and tap the version number seven times
quickly. The phone will pop up a dialog asking if you want to enable developer
mode and reboot. Say yes. If you've already done this, it will tell you so.

After reboot, go into Settings, System. There will be a new "Developer options"
section at the bottom. Turn on USB debugging. Plug in a USB cable to your
computer, and choose to trust the connection (and choose "use USB to transfer
files" if given the option, otherwise it will only allow charging over USB).

At this point, you should be able to use "hdc" somewhat like you would use
"adc" on an Android device:

See if the phone is visible/accessible:

```bash
hdc list targets -v
```

You can also connect the phone over Wifi and not use USB, but I couldn't get
this to work with the command line tools. If you want to try, turn on
"Wireless debugging" in Developer Options, note the IP address and port, and
run:

```bash
hdc tconn device 192.168.1.104:40317
```

Where the IP address and port match the phone. But again, this didn't work for
me, whereas USB worked fine. Report back if you get this working!

(DevEco Studio worked fine, though: add the device by IP address and port by
going to "Tools" -> "IP Connection" on the menu bar. This appears to be IPv4
only at the moment, but it successfully connected to the phone. This was a
good option, because for whatever reason the phone's USB connection would
continually drop and reconnect on my VirtualBox Windows 10 VM. But YMMV!)


## HarmonyOS App Development with SDL

### Create a project in DevEco Studio

While basic C/C++ code can just be compiled for HarmonyOS without a lot of
drama, there is a fairly complex directory tree needed for an actual app that
will run on a phone, and currently you need DevEco Studio to create and manage
this for you.

Go into DevEco Studio, and either click the big Plus button to create a new
project, or find the menu bar and click "File", "New", "Create Project".

Choose the "Native C++" template. You can still use C instead of C++, and also
write pieces in ArkTS. Choose a unique package name, other important settings.


### Get a HarmonyOS Debug Certificate

You need to sign apps to run them on a real device, even for local debugging
and testing and not deployment and release.

For now, just create your project in DevEco Studio, then in the menu bar, go
to "File", "Project Structure", "Signing Configs", "Automatically generate
signature". Log in to your Huawei developer account if needed. Let it do the
work. Later, I copied the project's directory and ".ohos" dir in my home
directory to a Linux system, adjusted a few Windows absolute paths in
build-profile.json5, and that was good enough.


### Add SDL to your app's build.

!!! FIXME: this needs a _lot_ more discussion, as several important files
!!! FIXME:  need to replace defaults in the project directory, you need to
!!! FIXME:  copy or symlink SDL sources in, etc.

Native code in your project is compiled with CMake, so just tell CMake to
build SDL as part of your project.


### SDL platform defines

SDL will define SDL_PLATFORM_OPENHARMONY and SDL_PLATFORM_UNIX. Do be cautioned
that while HarmonyOS has a Linux kernel compatibility interface, it is _not_
comparable to a "normal" Linux system in almost any way, so SDL_PLATFORM_LINUX
is not defined (SDL for Android, which literally uses the Linux kernel,
follows this same convention inside SDL). HarmonyOS devices don't use the
actual Linux kernel (they use a microkernel named "HongMeng"), but OpenHarmony
devices that aren't specifically HarmonyOS _might_ use a Linux kernel. Don't
make assumptions.


### Dynamic API

Presumably users cannot override the SDL build on their phones, so the Dynamic
API is disabled on this platform.


### Main

HarmonyOS has a fairly complicated startup sequence (described later in this
document). Apps for this platform should use the "main callbacks" instead of
an ANSI C style "main" entry point (!!! FIXME: see if we can get this working
with SDL_main in a background thread, though!).


### GPU support

OpenGL ES 2 and 3 are supported, as is Vulkan. Desktop OpenGL is supported by
the platform, too, but for now support is disabled within SDL itself. EGL is
used to manage GL contexts internally, and a HarmonyOS-specific Vulkan
extension is provided to get a surface. This means the SDL3 GPU API is
available, in addition to a hardware-accelerated 2D Renderer API.


### Audio

HarmonyOS support OpenSL ES, but like on Android, it is deprecated. As our
OpenSL ES backend is heavy with Android-specific code, it is not used on
HarmonyOS. There is a new backend using HarmonyOS's OHAudio API.


### Power

Battery percent, and whether the device is plugged in, are reported.


### Thread

OpenHarmony uses pthreads, and are fully supported.


### LoadSO

HarmonyOS uses ELF shared libraries and has a POSIX-style dlopen() mechanism,
so SDL_LoadObject() works as expected.


### STILL TODO

- Camera
- Dialog
- Haptic
- HIDAPI
- Package I/O?
- Async I/O
- Joystick
- Locale
- Misc
- Process
- Sensor
- Render
- Storage
- Time
- Timer
- Tray
- Video


## Building SDL for HarmonyOS

(Skip this if you are developing an SDL-based app and not SDL itself. There's
a different section of this document for you, later.)

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
$CLT/sdk/default/openharmony/native/build-tools/cmake/bin/cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=$CLT/sdk/default/openharmony/native/build/cmake/ohos.toolchain.cmake -DCMAKE_BUILD_TYPE=Release ..
```

(One can also add -DOHOS_ARCH=cputype, where "cputype" is "arm64-v8a" for ARM64, "armeabi-v7a" for ARM32, and "x86_64" Intel 64-bit. It defaults to ARM64 if unspecified.)


If all went well, you should be able to build SDL for HarmonyOS now. In our
case, build with Ninja:

```bash
ninja
```

Or use whatever instructs Xcode/Visual Studio/etc to build things.



## Build an app on the command line

If you have everything (signing certs, the entire project structure, etc) in
place from DevEco Studio, you can build an app from the command line. Go to
the root folder of the project and run:

```bash
hvigorw assembleHap
```

If everything went well (AND IT OFTEN DOES NOT, READ THE OUTPUT!), you'll have
a signed HarmonyOS app bundle (a ".hap" file) you can install to a device.


## Install on real hardware from the command line.

Make sure the phone is connected (run `hdc list targets -v` to verify).

```bash
# (or whatever the directory and .hap file are named.)
hdc install ./entry/build/default/outputs/default/entry-default-signed.hap
```


## Startup sequence

(You can probably skip this section if you just want to target HarmonyOS with
an SDL3-based app and not delve into the low-level details.)

Starting an SDL app on HarmonyOS works differently than any other platform,
but efforts have been made to _hide_ most of this from the C programmer, so
this should mostly feel like any other SDL app they have built in C before.

This is how startup works:

(Exact paths might vary, and maybe this will simplify later.)

- The C application _must_ use the SDL3 Main Callbacks. SDL_main.h will #error
  out if not. (!!! FIXME: remove this limitation?).
- The actual application entry point is in ArkTS, in the source file
  `$PROJECT/entry/src/main/ets/entryability/EntryAbility.ets`.
- This file defines a class, EntryAbility, that extends UIAbility.
- EntryAbility has an overridden method named onWindowStageCreate. When this
  runs, the system is ready for the app to display to the screen. This
  method calls `windowStage.loadContent('pages/Index', ...)`.
- This causes the app to load `$PROJECT/entry/src/main/ets/pages/Index.ets`.
- Index.ets specifies a layout for the window/view/whatever. We simply have a
  single row and column containing a single "XComponent" which is more or
  less a UI widget backed by native code.
- By now, libSDL3.so has been loaded. In a shared library constructor named
  SDL_RegisterNativeInterfaces, it uses NAPI to register a native module. Once
  this module initializes, we'll be able to call into SDL's C code from ArkTS
  through interfaces we defined in SDL_Init_Native_Interfaces(). This is in
  SDL/src/core/openharmony/SDL_openharmony.c. This will _also_ hook into the
  XComponent, to register some event callbacks, and load libmain.so, which is
  where the app's actual C code lives.
- When the XComponent's OnSurfaceCreated callback fires (landing in
  SDL_XComponent_OnSurfaceCreatedCallback()), we are then ready to hand control
  to the actual C application. Here we will call libmain.so's SDL_AppInit().
- Then, whenever the XComponent fires its OnFrame callback, we will call
  SDL_AppIterate().





## Incomplete notes

Everything below here is incomplete, in-progress, and maybe incomprehensible.
You can stop reading now, if you like.

- A lot of how to get started with native code and rendering was gleaned from
  https://gitee.com/harmonyos_samples/ndk-opengl and then trying to find
  various symbols used in there in the HarmonyOS documentation.

- https://github.com/openharmony/app_samples/ETSUI/XComponent, seems to do
  a lot of the native startup without the windowStage nonsense. Not clear if
  this still works, or is safe to do, but it _would_ simplify things a little.


### Get a HarmonyOS Debug Certificate without DevEco Studio

This is a giant dump of notes for doing this manually, which I didn't _quite_
manage to get working. Huawei has docs on this, which I've sort of half limped
through.

For now, just create your project in DevEco Studio,
then in the menu bar, got to "File" -> "Project Structure" ->
"Signing Configs" -> "Automatically generate signature". Log in to your Huawei
developer account if needed. Let it do the work. Later, I copied the project's
directory and ".ohos" dir in my home directory to a Linux system, adjusted a
few Windows absolute paths in build-profile.json5, and that was good enough.


(Ignore these notes if you aren't improving them.)

If you aren't using DevEco Studio to automatically generate signatures:

You need a signing certificate before you can install apps on a real HarmonyOS
device.

You get a certificate by generating a Certificate Signing Request (a "CSR")
and submitting it to Huawei.

Right now this needs DevEco Studio to generate the CSR file, as a one-time
step. Go into the IDE, click the hamburger menu icon on the top left of the
window, choose the "Build" menu item, and then click "Generate Key and CSR".
In the dialog that pops up, you'll first make a signing key in ".p12" format.
Enter a filename and save path, choose a password for the key. Fill in the
"Alias" which is any short, unique, possibly-ASCII, human readable string.
Ignore the Advanced section.

Click "Next". Now you'll generate the CSR file. The same .p12 file, password
and alias you just generated will be prefilled in the new dialog. Just tell it
where to save the new .csr file. Click "Finish" and you're done with DevEco
Studio for this task.

The Common Name ("CN=" field) on the Certificate Request is what DevEco Studio
refers as an "alias." I put this in as "icculus.org" for mine, but it might
just need to be unique. It's required, all the other fields are not (indeed,
DevEco Studio does not ask for them by default and leaves them blank).

*TODO IN THE FUTURE:* the CSR files are in a standard format, so OpenSSL's
command line utility can _probably_ generate them without having to go through
DevEco Studio, but anything OpenSSL is complicated and needs research.

Dumping the .csr file that DevEco Studio generated (using the command line:
`openssl req -in MyCSRFile.csr -noout -text`) reports the following, so there
are probably OpenSSL incantations to generate your own .p12 and .csr file
directly.

```
Certificate Request:
    Data:
        Version: 1 (0x0)
        Subject: C=, ST=, L=, O=, OU=, CN=[the "Alias" that I entered is here]
        Subject Public Key Info:
            Public Key Algorithm: id-ecPublicKey
                Public-Key: (256 bit)
                pub:
                    [hex values for public key are here]
                ASN1 OID: prime256v1
                NIST CURVE: P-256
        Attributes:
            Requested Extensions:
                X509v3 Subject Key Identifier: 
                    [hex values for key identifier are here]
    Signature Algorithm: ecdsa-with-SHA256
    Signature Value:
        [hex values for signature are here]
```

Once you have a .csr file, go to Huawei's AppGallery Connect portal, and find
the "Certificates, app IDs, and profiles" tab at the top. Click the "New
certificate" button, and upload the .csr file you generated. Give it a name,
set the "Certificate type" to "Debug certificate" and point to your .csr file.
Click "Submit".

If everything went okay, you'll have a certificate listed for download. This
should be an immediate thing, it doesn't need approval on Huawei's side.
Download the .cer file and save it for later.


### Android mappings

These are not direct mappings (the command lines are not identical, etc).

- "adc" command line tool: "hdc"
- "adc logcat": "hdc hilog"
- "gradlew": "hvigorw"

