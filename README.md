# BlitzX3D
This is a fork of Blitz3D TSS, originally based on Blitz3D and maintained by ZiYueCommentary.

## How to Build

### Prepare

- Visual Studio Community 2026
  - Desktop development with C++
  - C++ MFC for latest v145 build tools (x86 & x64)
  - C++ ATL for latest v145 build tools (x86 & x64)
  - ASP.NET and web development
  - A Brain
### Steps

1. Open `blitz3d.sln` in Visual Studio 2022.
2. Download the latest FFmpeg LGPL build from: https://github.com/sudo-nautilus/FFmpeg-Builds-Win32/releases/tag/latest. Get `ffmpeg-master-latest-win32-lgpl-shared.zip`, extract it, and copy the `bin`, `lib`, and `include` folders into the project's ffmpeg directory, replacing any existing files if prompted
3. Select the **Release** or **Debug** configuration and rebuild the entire solution.
4. All done! You can find the output files in the `_release` and `_release/bin` directories. Feel free to delete any `.pdb` and `.ilk` files.

## In Memory of Mark Sibly

[Mark Sibly](https://github.com/blitz-research), the creator of Blitz3D, passed away on 12 December 2024. 🕯️
