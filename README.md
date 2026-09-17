# Symocraft

A Minecraft-inspired C++ / OpenGL voxel game, being developed from a course
prototype into a maintainable graphics and game-engine project.

The current scope is Windows x64 and the existing single-player, fixed-world
gameplay. Streaming chunks, saving, multiplayer, and additional rendering
features are not part of the initial recovery milestones. See the
[technical documentation](docs/README.md) for scope, progress, and evidence.

## Build

Required: Visual Studio 2022 C++ tools with a Windows SDK and the English
compiler language pack, CMake 3.22 or newer, and Ninja. The shared presets use
`VSLANG=1033` for reliable header-dependency tracking. CLion's bundled CMake
and Ninja are supported. The game requests
OpenGL 4.6 Core; the asset tests do not require a GPU or a window.

From an x64 Visual Studio developer shell with CMake and Ninja on PATH:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --no-tests=error
```

Use `windows-release` for Release. From ordinary 64-bit PowerShell, the helper
can locate Visual Studio and use an explicit CLion installation:

```powershell
./scripts/build.ps1 -Configuration Debug -CLionPath 'C:/path/to/CLion'
./scripts/build.ps1 -Configuration Release -Action Install -CLionPath 'C:/path/to/CLion'
```

Replace the CLion path with your installation, or set `CLION_HOME` once in the
current shell. The helper does not install tools or change system settings.
Its default action configures, builds, and runs tests.

For CLion, create a Visual Studio toolchain named **Symocraft MSVC**, select
the amd64/x64 architecture, and enable the `windows-debug` and `windows-release`
presets. See the [build and CLion guide](docs/development/build-and-clion.md).

## Run

```powershell
./out/build/windows-debug/bin/SymoCraft.exe
./out/build/windows-debug/bin/SymoCraft.exe --check-assets
```

Assets are staged next to the executable and resolved from its location, not
the shell's working directory. `--check-assets` checks required files without
opening a window; it does not validate gameplay or shader/image contents.
Use an ASCII installation path for the complete game until the legacy loaders'
Unicode handling has been validated.

`cmake --install out/build/windows-release` creates a development staging
directory at `out/install/windows-release`. This is not yet a validated final
redistributable: runtime-library deployment, gameplay, and long-run acceptance
remain separate milestones.

## Controls

| Input | Action |
| --- | --- |
| W / A / S / D | Move |
| Left Shift | Run |
| Mouse | Look |
| Mouse wheel | Adjust field of view |
| Space | Jump when grounded |
| Left / right click | Remove / place a block |
| Q / E | Select a block type |
| Hold Caps Lock | Debug mode without gravity or collision |
| Left Ctrl while holding Caps Lock | Descend |
| Esc | Exit |

## Original Technical Features

- OpenGL direct-state-access rendering
- Procedural terrain using noise
- Batched geometry submission and hidden-face removal
- Custom entity/component storage and character physics

These describe the existing implementation, not a claim that all engineering
or performance milestones have passed.

## Learning References

- [LearnOpenGL](https://learnopengl.com/) and *OpenGL SuperBible*
- [Procedural generation tutorial](https://www.youtube.com/watch?v=wbpMiKiSKm8)
- [Voxel mesh generation](https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/)
