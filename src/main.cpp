#include <MemoryAllocator/AmoBase.h>
#include "core/application.h"
#include "core/asset_paths.h"

#include <iostream>
#include <charconv>
#include <exception>
#include <string_view>

int main(int argc, char* argv[])
{
    const bool check_assets_only = argc == 2 && std::string_view(argv[1]) == "--check-assets";
    unsigned int frame_limit = 0;
    bool valid_arguments = argc == 1 || check_assets_only;
    if (argc == 3 && std::string_view(argv[1]) == "--smoke-frames")
    {
        const std::string_view value(argv[2]);
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frame_limit);
        valid_arguments = parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() &&
                          frame_limit > 0 && frame_limit <= 10000;
    }
    if (!valid_arguments)
    {
        std::cerr << "Usage: SymoCraft [--check-assets | --smoke-frames 1..10000]\n";
        return 64;
    }
    if (!SymoCraft::Assets::CheckRequiredAssets(std::cerr))
        return 2;
    if (check_assets_only)
    {
        std::cout << "All required assets are present.\n";
        return 0;
    }

#ifdef _DEBUG
    AmoBase::AmoMemory_Init(true, 1024);
#endif
    int exit_code = 0;
    try
    {
        SymoCraft::Application::Init();
        SymoCraft::Application::Run(frame_limit);
    }
    catch (const std::exception& error)
    {
        std::cerr << "[runtime] fatal: " << error.what() << std::endl;
        exit_code = 3;
    }
    catch (...)
    {
        std::cerr << "[runtime] fatal: unexpected non-standard exception" << std::endl;
        exit_code = 3;
    }

    // Run's local GL objects have unwound; release renderer objects before the context.
    SymoCraft::Application::Free();
    AmoBase::AmoMemory_MemoryLeaksDetected();
    std::cout << "[runtime] shutdown complete; exit_code=" << exit_code << std::endl;
    return exit_code;
}
