# USB Tree

A native USB topology explorer inspired by [USB Device Tree Viewer](https://www.uwe-sieber.de/usbtreeview_e.html). Built in C++20 with statically linked SDL3 and Dear ImGui docking. Windows is the first enumeration backend; the model and interface are shared for future macOS and Linux support.

## Build (Windows x64)

Requires Visual Studio C++ desktop tools, a Windows SDK, CMake 3.24+, Ninja, and internet access for the first configure.

```powershell
./scripts/build.ps1 -Configuration Release -Test
```

All generated build files live in `C:/buildfiles/usbtree`. Copy the entire `C:/buildfiles/usbtree/release/portable/UsbTree` folder to run elsewhere. Fonts and licenses are staged beside the executable. Runtime paths are relative to the executable, including when launched from a different working directory. The MSVC CRT, SDL3 and ImGui are linked statically; Windows system libraries are supplied by the OS.

## Structure

- `src/main.cpp`: entry point only.
- `src/app`: plain application state, filtering, reports and descriptor decoding; no GUI or OS headers.
- `src/ui`: Dear ImGui drawing, typography and theme.
- `src/platform`: SDL window, filesystem, Windows USB enumeration.
- `assets`: fonts and notices copied from fwcom.
- `tests`: console tests and ImGui Test Engine end-to-end harness.

`--demo` uses an explicitly labeled deterministic fixture for interface development. `--smoke` opens, renders and exits after startup.

## Visual provenance

Open Sans, Fira Code, Material Icons and the Wili Dark theme metrics/colors are copied or adapted from the local fwcom project as requested. See `assets/NOTICE.md` and font licenses. This is an independent implementation; no UsbTreeView executable or source is redistributed.
