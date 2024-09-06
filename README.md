# Skyrim Community Shaders

SKSE core plugin for community-driven advanced graphics modifications.

[Nexus](https://www.nexusmods.com/skyrimspecialedition/mods/86492)

## Requirements

- Any terminal of your choice (e.g., PowerShell)
- [Visual Studio Community 2022](https://visualstudio.microsoft.com/)
  - Desktop development with C++
- [CMake](https://cmake.org/)
  - Edit the `PATH` environment variable and add the cmake.exe install path as a new value
  - Instructions for finding and editing the `PATH` environment variable can be found [here](https://www.java.com/en/download/help/path.html)
- [Git](https://git-scm.com/downloads)
  - Edit the `PATH` environment variable and add the Git.exe install path as a new value
- [Vcpkg](https://github.com/microsoft/vcpkg)
  - Install vcpkg using the directions in vcpkg's [Quick Start Guide](https://github.com/microsoft/vcpkg#quick-start-windows)
  - After install, add a new environment variable named `VCPKG_ROOT` with the value as the path to the folder containing vcpkg

## User Requirements

- [Address Library for SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
  - Needed for SSE/AE
- [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101)
  - Needed for VR

## Register Visual Studio as a Generator

- Open `x64 Native Tools Command Prompt`
- Run `cmake`
- Close the cmd window

## Clone and Build
Open terminal (e.g., PowerShell) and run the following commands:

```
git clone https://github.com/doodlum/skyrim-community-shaders.git --recursive
cd skyrim-community-shaders
.\BuildRelease.bat
```

### CMAKE Options (optional)
If you want an example CMakeUserPreset to start off with you can copy the `CMakeUserPresets.json.template` -> `CMakeUserPresets.json`
#### AUTO_PLUGIN_DEPLOYMENT
* This option is default `"OFF"`
* Make sure `"AUTO_PLUGIN_DEPLOYMENT"` is set to `"ON"` in `CMakeUserPresets.json`
* Change the `"CommunityShadersOutputDir"` value to match your desired outputs, if you want multiple folders you can separate them by `;` is shown in the template example
#### AIO_ZIP_TO_DIST
* This option is default `"ON"`
* Make sure `"AIO_ZIP_TO_DIST"` is set to `"ON"` in `CMakeUserPresets.json`
* This will create a `CommunityShaders_AIO.7z` archive in /dist containing all features and base mod
#### ZIP_TO_DIST
* This option is default `"ON"`
* Make sure `"ZIP_TO_DIST"` is set to `"ON"` in `CMakeUserPresets.json`
* This will create a zip for each feature and one for the base Community shaders in /dist containing
#### TRACY_SUPPORT
* This option is default `"OFF"`
* This will enable tracy support, might need to delete build folder when this option is changed


When using custom preset you can call BuildRelease.bat with an parameter to specify which preset to configure eg:
`.\BuildRelease.bat ALL-WITH-AUTO-DEPLOYMENT`

When switching between different presets you might need to remove the build folder

## License

### Default

[GPL-3.0-or-later](COPYING) WITH [Modding Exception AND GPL-3.0 Linking Exception (with Corresponding Source)](EXCEPTIONS.md).  
Specifically, the Modded Code is Skyrim (and its variants) and Modding Libraries include [SKSE](https://skse.silverlock.org/) and Commonlib (and variants).

### Shaders

See LICENSE within each directory; if none, it's [Default](#default)

- [Features Shaders](features)
- [Package Shaders](package/Shaders/)
