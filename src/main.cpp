#include <MemoryAllocator/AmoBase.h>
#include "core/application.h"
#include "core/asset_paths.h"

#include <iostream>
#include <string_view>

int main(int argc, char* argv[])
{
    const bool check_assets_only = argc == 2 && std::string_view(argv[1]) == "--check-assets";
    if (argc > 1 && !check_assets_only)
    {
        std::cerr << "Usage: SymoCraft [--check-assets]\n";
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
    SymoCraft::Application::Init();
    SymoCraft::Application::Run();
    SymoCraft::Application::Free();

    AmoBase::AmoMemory_MemoryLeaksDetected();
    return 0;
}
