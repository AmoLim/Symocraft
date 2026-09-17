# 资源路径模块：从可执行文件定位游戏资源

本文对应 M1 新增的 `SymoCraft::Assets` 模块。目标是让资源定位不再依赖 CLion 的运行目录，也不依赖启动命令所在的位置。本模块是一个小型路径与资源存在性检查组件，不是资源管理器，更不是文件系统安全沙箱。

## 1. 解决的问题与职责边界

原有加载调用使用 `../assets/...`。相对路径默认从进程的当前工作目录解析，而不是从源文件、项目根目录或可执行文件所在目录解析。因此，同一个游戏程序可能在 IDE 中正常，在资源管理器或另一个终端目录中启动时失败。

M1 采用固定的运行布局：

```text
<game-directory>/
  SymoCraft.exe
  assets/
    configs/blockFormats.yaml
    shaders/vs_BlockShader.glsl
    shaders/fs_BlockShader.glsl
    shaders/vs_FrameShader.glsl
    shaders/fs_FrameShader.glsl
    textures/texture_atlas.png
```

这里的 `<game-directory>` 可以是构建输出目录，也可以是安装目录。移动程序时，应移动包含 `assets` 的完整目录，而不是只移动 exe。

模块负责：

- 获取当前进程可执行文件所在目录。
- 将资源相对路径解析到 exe 同级的 `assets` 目录内。
- 拒绝不符合约定的路径，减少意外访问目录外文件的可能性。
- 检查当前六个必需资源是否为普通文件，输出缺失或无法访问的文件名称。

模块不负责：

- 读取或解码 PNG、解析 YAML、编译 GLSL。
- 创建或销毁 OpenGL 对象，管理资源缓存、热重载任务或引用计数。
- 创建资源目录、写入游戏存档、下载缺失文件。
- 修改进程当前工作目录，或从源码目录回退查找资源。
- 保证不可信输入下的文件访问安全，或解决文件检查之后被其他进程替换的竞态问题。

构建侧由 [StageAssets.cmake](../../cmake/StageAssets.cmake) 将必需文件同步到 exe 同级目录；安装侧由顶层 [CMakeLists.txt](../../CMakeLists.txt) 放置 exe 和资源。运行时模块不猜测开发机目录，从而避免发布产物偷偷读取开发目录中的文件。

## 2. 数据流与源码入口

```text
源目录 assets 中的必需文件
          |
          | CMake 构建同步或安装
          v
exe 同级 assets 目录
          |
          v
main -> CheckRequiredAssets -> Resolve
          |                     |
          |                     +-> ExecutableDirectory -> GetModuleFileNameW
          |                     +-> Root -> assets
          |                     +-> 输入约束、规范化、目录包含关系检查
          |
          +-> 失败：打印诊断，返回退出码 2
          +-> --check-assets：检查通过后返回 0，不创建窗口
          +-> 正常启动：Application::Init / Run
                              |
                              +-> Renderer::Init / ReloadShaders -> Resolve -> GLSL 加载器
                              +-> Renderer::Init -> Resolve -> YAML 加载器
                              +-> Application::Run -> Resolve -> PNG 加载器
```

建议按以下顺序阅读：

1. [asset_paths.h](../../include/core/asset_paths.h)：公开接口及依赖。
2. [asset_paths.cpp](../../src/core/asset_paths.cpp)：Windows 路径获取、输入检查、规范化和存在性检查。
3. [main.cpp](../../src/main.cpp)：启动预检与退出码。
4. [renderer.cpp](../../src/renderer/renderer.cpp)、[application.cpp](../../src/core/application.cpp)：加载器边界和字符串生命周期。
5. [asset_paths_tests.cpp](../../tests/asset_paths_tests.cpp)：路径契约和迁移包测试。

## 3. 公开接口与错误语义

所有公开接口位于 `SymoCraft::Assets` 命名空间。

| 接口 | 返回值与职责 | 错误处理 |
| --- | --- | --- |
| `ExecutableDirectory()` | 返回当前进程 exe 所在目录的 `std::filesystem::path` | Windows API 失败时抛出 `std::system_error`；缓冲区尺寸无法安全增长时抛出 `std::length_error`；常规内存分配也可能失败 |
| `Root()` | 返回 `ExecutableDirectory() / "assets"`；本身不要求目录存在，也不执行规范化 | 传播路径获取、构造过程中的异常 |
| `Resolve(relative_path)` | 返回规范化后的资源路径；允许目标文件尚不存在 | 非法输入或解析结果越界时抛出 `std::invalid_argument`；文件系统查询失败也可能抛出 `std::filesystem::filesystem_error` |
| `RequiredFiles()` | 返回六个必需资源相对名称的常量数组引用 | 引用及其中的 `string_view` 指向模块的静态存储，不需要调用者释放 |
| `CheckRequiredAssets(errors)` | 检查所有必需文件；全部符合条件返回 `true`，否则写入诊断并返回 `false` | 捕获每个文件解析过程中常见的 `std::exception`，继续检查其他文件；不声明 `noexcept`，不能据此保证输出流或分配失败时绝不抛异常 |

`Resolve` 不打开文件。它回答“这个资源名称应该映射到哪个位置”，而不是“这个文件现在可读且内容正确”。将路径解析与文件读取分开，既方便测试，也避免路径模块依赖 OpenGL、PNG 或 YAML。

`CheckRequiredAssets` 使用 `is_regular_file(path, error_code)` 检查文件类型。它不能证明后续打开一定成功：读取权限、共享锁或文件在检查后被删除，都可能造成实际读取失败。这些失败仍应由加载器处理。

`main` 的命令行约定如下：

| 情况 | 行为与退出码 |
| --- | --- |
| 无参数，资源预检通过 | 进入现有游戏初始化、运行和释放流程 |
| `--check-assets`，资源预检通过 | 输出成功信息，返回 `0`，不进入窗口和 OpenGL 初始化 |
| 必需资源缺失或检查失败 | 向标准错误输出诊断，返回 `2` |
| 其他参数组合 | 输出用法，返回 `64` |

这段预检没有重构原有游戏初始化异常处理或 OpenGL 资源释放顺序。不能将其理解为整个启动流程已经具备完整的失败恢复能力。

## 4. 为什么使用 GetModuleFileNameW

Windows 有两个不同的概念：

- **可执行文件位置**：当前进程实际加载的 exe 位于哪里。
- **当前工作目录**：普通相对文件路径从哪里开始解析，可以由 IDE、快捷方式、启动器或调用者设置。

模块使用 `GetModuleFileNameW(nullptr, ...)` 获取当前进程 exe 路径，再取 `parent_path()`。它没有使用 `argv[0]`，因为启动参数中的程序名称不必是完整路径；也没有使用 `current_path()`，因为那正是原来的不稳定依赖。

API 名称末尾的 `W` 表示宽字符版本。Windows 上 `std::filesystem::path` 的原生路径表示也是宽字符，因此这里可以保留原生路径，而不是先转换成某个系统代码页的窄字符串。

实现使用动态缓冲区，而不是假定所有路径都短于固定常量：

1. 创建包含 256 个 `wchar_t` 的缓冲区。
2. 调用 API；返回 `0` 时，读取 `GetLastError()` 并抛出包含系统错误的异常。
3. 返回长度小于缓冲区容量时，按实际字符数量构造路径。
4. 缓冲区不足时将容量翻倍并重试，增长前检查 `DWORD` 尺寸上限。

这避免了静默截断 exe 路径，但不代表已经完成 Windows 长路径端到端支持。后续文件加载 API、系统策略和安装路径仍有各自的限制。

当前实现每次需要时重新查询路径，没有修改工作目录，也没有可变的全局路径缓存。该模块主要用于启动和资源加载，不应无理由放入每帧、每个方块的热路径中。

## 5. 路径规范化与越界拒绝

### 5.1 第一层：检查输入形式

`Resolve` 只接受非空的资源相对路径，依次拒绝：

- 空路径。
- 含根目录或根名称的路径，例如绝对路径、UNC 路径、`C:outside.txt` 这种盘符相对路径。
- 内嵌空字符，避免路径对象记录的名称与使用空字符结尾的 API 实际解释不同。
- 冒号，避免 Windows 备用数据流等非普通资源名称。
- 任意 `..` 路径分量，即使 `shaders/../file` 最终可能仍位于资源根内，也按统一的简单规则拒绝。

使用的是路径分量迭代，而不是在原始字符串里搜索两个点。正常文件名中的点不等于父目录操作。

### 5.2 第二层：规范化后检查包含关系

模块分别对资源根和拼接后的目标调用 `std::filesystem::weakly_canonical`：

```cpp
const auto root = std::filesystem::weakly_canonical(Root());
const auto result = std::filesystem::weakly_canonical(root / relative_path);
```

`weakly_canonical` 会处理现有路径部分并规范化剩余部分，因此目标文件还不存在时仍可解析。这与要求整条路径存在的 `canonical` 不同，也比仅整理 `.` 和 `..` 的词法规范化多了对现有文件系统链接的处理。

随后 `IsInside` 按路径分量比较：目标必须以根目录的全部分量开头，并且至少多一个分量。这样，资源根本身 `.` 不会被当成资源文件接受，名称相似的相邻目录也不会仅因为字符串前缀相同就通过。

当资源根内部的现有链接将目标引向根外时，规范化后的包含关系检查会拒绝它。但应注意两点：

- 检查的是当时的文件系统状态。检查完成到加载器打开文件之间，仍可能发生文件或链接被替换的竞态；本模块没有使用句柄级访问来消除这一问题。
- `assets` 根本身会被规范化。如果根目录就是一个链接，其解析后位置会成为本次检查的根。这不是“拒绝所有链接”的政策。

现有自动测试覆盖非法路径形式，但没有创建实际的符号链接或目录联接进行专项测试。因此，链接处理属于已有实现机制，不应写成已经完成所有重解析点安全测试。

## 6. 路径类型与字符串所有权

### 6.1 为什么公开接口返回 path

路径组合使用 `path / relative`，而不是手工拼接分隔符。使用结构化路径类型可以保留 Windows 原生表示，并把文件系统路径与面向显示的文本区分开。

诊断输出需要窄字符串时，内部 `ToUtf8` 通过 `u8string()` 得到 UTF-8 字节，再转换为 `std::string` 交给输出流。控制台能否正确显示这些字节，仍与控制台和日志查看器的编码设置有关。

### 6.2 纹理路径的生命周期

现有纹理类型会保留 `std::string_view`。`string_view` 不拥有它引用的字符串，因此下面这种写法不能用于会保留视图的接口：

```cpp
// Do not retain a view into this temporary string.
texture_array.CreateAtlasSlice(Assets::Resolve("textures/texture_atlas.png").string(), true);
```

完整表达式结束后，临时 `std::string` 会销毁，纹理对象保存的视图将失效。当前调用在 `Application::Run` 中保留了明确的所有者：

```cpp
const std::string texture_path = Assets::Resolve("textures/texture_atlas.png").string();
TextureArray texture_array;
texture_array = texture_array.CreateAtlasSlice(texture_path, true);
```

`texture_path` 在 `Run` 期间持续存在，且声明先于纹理对象。这个改动修复的是路径字符串的所有权边界，不代表纹理对象已经完成 RAII 化，也不改变原有 GPU 资源清理流程。

当前 GLSL 编译和 YAML 配置加载调用同步消费传入的路径，不保存该路径视图，因此调用表达式中的临时字符串可以覆盖这次同步调用。若未来改成异步加载或保存重载路径，就必须改成持有 `std::string` 或 `std::filesystem::path`，不能继续依赖这个前提。

### 6.3 Unicode 支持的明确边界

路径模块使用宽字符 Windows API 和 `std::filesystem`，自动测试也包含 Unicode 包目录。但是游戏的 GLSL、YAML 和纹理加载调用仍通过 `.string()` 将路径交给旧的窄字符串接口。

因此，**路径模块的 Unicode 测试通过，不等于整个游戏可以安装在任意 Unicode 目录中**。系统代码页无法表达的字符可能在转换或打开文件时失败。M1 不宣称完整支持这类游戏安装路径；建议暂时使用 ASCII 安装路径。后续需要逐个改造加载器并用真实资源进行端到端验证。

## 7. 测试设计、复现与证据

CTest 当前注册了四项无窗口测试，定义见 [tests/CMakeLists.txt](../../tests/CMakeLists.txt)：

| 测试 | 检查内容 |
| --- | --- |
| `assets.contracts` | 正常路径、`.` 规范化、允许不存在的目标、非法输入拒绝、工作目录不被修改，以及迁移包子进程测试 |
| `assets.from_source_directory` | 从源码根目录再次运行同组契约测试，避免只在构建目录中偶然通过 |
| `assets.game_preflight` | 运行实际游戏 exe 的 `--check-assets` 入口；确认预检成功，但不进入游戏窗口或 OpenGL |
| `assets.game_package` | 将实际游戏 exe 和源码目录中的必需资源复制到含空格的隔离包目录，从无关目录启动，检查完整包、缺失纹理和非法参数的退出码及诊断 |

前两项中的路径模块迁移测试会复制测试 exe，而不是直接更改某个全局“资源根”变量。由于 `ExecutableDirectory()` 查询真实进程位置，只有启动复制后的子进程，才能检查根目录是否真的跟随 exe 移动。

每次测试在系统临时目录下建立独立子目录，并依次检查：

1. 包目录名称含空格，资源完整，从另一个工作目录启动。
2. 删除必需清单中的纹理文件，验证失败结果及具体文件诊断。
3. 包目录名称包含 Unicode 字符，验证路径模块和预检。

测试生成的资源文件是存在性检查夹具，内容只是文本，并不是真实 PNG 或 GLSL。这个选择是有意的：测试对象是路径和清单契约，不是解码器。不能把这些测试当成渲染或游戏启动验收。

子进程使用 `CREATE_NO_WINDOW`，有 10 秒等待上限；异常等待情况下会尝试终止并回收进程句柄。当前失败信息包含子进程退出码，但没有捕获完整的子进程标准错误输出。最外层 CTest 另有超时限制，防止整组测试无限等待。

第四项由 [CheckAssetPackage.cmake](../../tests/CheckAssetPackage.cmake) 实现，使用真实资源，并在构建目录的 `package-probes` 下创建随机隔离子目录。每次子进程运行限制为 10 秒，捕获标准输出和标准错误；失败时保留包目录供诊断，成功后仅清理经过规范化和父目录检查的本次子目录。它验证的是实际游戏命令行预检在迁移包中的行为，仍不进入图形初始化，也不直接验证 `cmake --install` 生成的安装目录或无开发环境机器上的运行库部署。

可以在正确的 MSVC 开发环境中通过工程的 Debug/Release 测试预设运行。仅重跑已有验证目录中的资源测试时，命令形式为：

```powershell
ctest --test-dir <configured-build-directory> --output-on-failure -R "^assets\."
```

应先构建，再执行 CTest；CTest 本身不负责补建缺失的程序和资源。`<configured-build-directory>` 必须替换为实际的已配置构建目录。

### 7.1 M1 的沙箱失败与授权复测

本次默认系统 `TEMP` 中的迁移子进程在受限执行环境下失败，造成前两项测试失败；实际游戏的无窗口预检通过。失败日志保留在 [首次构建测试日志](../milestones/m1/evidence/01-debug-build-test.log) 和 [全新 Debug 构建测试日志](../milestones/m1/evidence/02-debug-clean-build-test.log)。这些失败不能删去或改写为成功。

随后经授权在宿主环境运行同一验证构建目录中的 CTest，当时已注册的三项测试全部通过；这是增加 `assets.game_package` 之前的历史记录：

- 测试目录：`out/m1-validation/windows-debug`。
- 结果：`3/3` 通过，`0` 失败。
- 本次记录总耗时：`0.30` 秒。
- 原始证据：[授权宿主测试日志](../milestones/m1/evidence/03-debug-tests-host.log)。

这组对照支持“受限环境中的默认临时目录子进程执行问题”的判断，而不是通过删除迁移测试掩盖失败。它不替代对所有安全软件或沙箱配置的兼容性验证，也不授权通过更换执行位置规避权限要求。以后出现相同情况，应保留退出码和日志，并按权限流程复验。

这份通过记录只属于上述 Debug 资源测试。它不是 Release 构建、图形初始化、玩法、性能或整个 M1 节点已经通过的证据；整体节点状态以里程碑报告为准。

## 8. 设计取舍与后续演进

当前方案选择“一个固定根目录 + 明确的必需资源清单”，而不是搜索多个目录或引入完整资源管理器。好处是容易解释、可迁移、失败位置明确；代价是没有资源覆盖层、模组目录或用户级搜索路径。

运行时清单位于 `asset_paths.cpp`，构建侧清单位于 [RequiredAssets.cmake](../../cmake/RequiredAssets.cmake)，由 `StageAssets.cmake` 和游戏迁移包测试共用；安装流程使用同一构建侧清单。新增必需资源时，需要同步维护运行时与构建侧两处清单，并检查安装布局和预检。当前测试能发现运行时要求的文件未被同步，但不会自动发现构建清单中新增的文件未加入运行时清单。

值得保留的演进方向包括：

- 统一清单来源，减少构建与运行时清单漂移。
- 让文件加载器直接接受 `std::filesystem::path`，完成 Unicode 路径与真实内容的端到端验证。
- 在加载器层补齐读取、解码、解析和 GPU 创建失败的明确返回结果。
- 增加实际链接、长路径和不可读文件的专项测试，并准确记录平台条件。
- 若引入后台资源任务，显式传递拥有数据的路径和值，规定取消、错误交接和主线程 GPU 上传边界。

这些是后续方向，不是 M1 已完成的能力。当前模块应保持小而清楚，先让资源定位具备可验证的工程契约，再在实际需要出现时扩展。
