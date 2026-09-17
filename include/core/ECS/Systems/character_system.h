//
// Created by Amo on 2022/6/18.
//

#ifndef SYMOCRAFT_CHARACTER_SYSTEM_H
#define SYMOCRAFT_CHARACTER_SYSTEM_H
#include "core.h"


namespace SymoCraft
{
    namespace ECS
    {
        class Registry;
    }

    namespace Character
    {
        namespace Player
        {
            void Init();
            void Update(ECS::Registry& registry);
            void SyncCamera(ECS::Registry& registry);
        }
    }
}

#endif //SYMOCRAFT_CHARACTER_SYSTEM_H
