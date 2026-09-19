# Symocraft

A Minecraft-inspired C++ / OpenGL voxel game, being developed from a course
prototype into a maintainable graphics and game-engine project.

The current scope is Windows x64 and the existing single-player, fixed-world
gameplay. Streaming chunks, saving, multiplayer, and additional rendering
features are not part of the initial recovery milestones. See the
[technical documentation](docs/README.md) for scope, progress, and evidence.
The [M2-A desktop report](docs/milestones/m2-a/README.md) records the current
recovery work and its acceptance limits; M2 as a whole has not passed.

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

## Resource Regression Tests

The asset and shader-loading checks run without an OpenGL context. They cover
working-directory independence, relocated packages, missing assets, empty or
unreadable shader files, failure cleanup, and shader-program replacement.
Shader driver calls are stubbed in the unit test; real GLSL compilation still
requires the game smoke test on a compatible GPU.

To run these five focused tests (also included in the full suite):

```powershell
ctest --preset windows-debug -R '^(assets[.]|renderer[.]shader_loading$)' --output-on-failure --no-tests=error
```

Use `windows-release` to check the Release build. Build the selected preset
before running CTest so its executable and staged assets are up to date.

## Run

```powershell
./out/build/windows-debug/bin/SymoCraft.exe
./out/build/windows-debug/bin/SymoCraft.exe --check-assets
./out/build/windows-debug/bin/SymoCraft.exe --smoke-frames 120
```

Assets are staged next to the executable and resolved from its location, not
the shell's working directory. `--check-assets` checks required files without
opening a window; it does not validate gameplay or shader/image contents.
`--smoke-frames 1..10000` loads the real game and exits after the requested
number of rendered frames. It still needs a working OpenGL environment and
does not replace interactive gameplay, long-run, or performance acceptance.
Use an ASCII installation path for the complete game until the legacy loaders'
Unicode handling has been validated.

`cmake --install out/build/windows-release` creates a development staging
directory at `out/install/windows-release`. This is not yet a validated final
redistributable: runtime-library deployment, gameplay, and long-run acceptance
remain separate milestones.

## M2-A Candidate

The desktop candidate used for M2-A verification is
`out/install/m2-a/SymoCraft.exe`, with its adjacent `assets` directory.
The handoff archive path is `out/packages/Symocraft-M2-A-windows-x64.zip`,
containing the complete `m2-a` directory. Keep the executable and assets together.

This is a development candidate, not a final redistributable. Debug and Release
each passed nine automated tests; desktop smoke and limited interactive results
are recorded in the [M2-A report](docs/milestones/m2-a/README.md). The complete
14-case gameplay checklist, 15-minute session, performance baseline, Y9000P
laptop acceptance, and clean-machine runtime deployment remain outstanding.

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
