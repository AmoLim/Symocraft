//
// Created by Amo on 2022/6/15.
//

#define STB_IMAGE_IMPLEMENTATION

#include "core/application.h"
#include "core/asset_paths.h"
#include "core.h"
#include "core/window.h"
#include "renderer/texture.h"
#include "renderer/renderer.h"
#include "world/chunk.h"
#include "core/ECS/registry.h"
#include "core/ECS/Systems/transform_system.h"
#include "core/ECS/Systems/character_system.h"
#include "core/ECS/Systems/physics_system.h"
#include "core/ECS/component.h"
#include "world/world.h"
#include "playercontroller/playercontroller.h"

namespace SymoCraft
{


    static bool first_enter = true;  // is first enter of cursor? initialized by true

    namespace Application
    {
        //irrklang::ISoundEngine *SoundEngine = irrklang::createIrrKlangDevice();

        // Block VAR
        int new_block_id = 0;
        const int kNumBlocks = 7;
        const float kBlockPlaceDebounceTime = 0.2f;
        float block_place_debounce = 0.0f;

        // Time variables
        const float kBlockChangeDebounceTime = 0.2f;
        float block_change_debounce = 0.0f;
        float delta_time = 0.016f;


        // Internal variables
        // static GlobalThreadPool* global_thread_pool;
        static Camera* camera;

        void Init()
        {
            Window::Init();
            Window& window = GetWindow();   // Get a reference pointer of the only Window
            if (!window.window_ptr)
            {
                AmoLogger_Error("Error: Could not create a window. ");
                return;
            }

            // Initialize all other subsystems.
            ECS::Registry &registry = GetRegistry();
            registry.RegisterComponent<Transform>("Transform");
            registry.RegisterComponent<Physics::RigidBody>("RigidBody");
            registry.RegisterComponent<Physics::HitBox>("HigBox");
            registry.RegisterComponent<Character::CharacterComponent>("CharacterComponent");
            registry.RegisterComponent<Character::PlayerComponent>("PlayerComponent");

            Renderer::Init();
            World::Init();

            camera = GetCamera();
        }

        void Run()
        {

            std::cout << player << std::endl;
            Window& window = GetWindow();
            double previous_frame_time = glfwGetTime();

            stbi_set_flip_vertically_on_load(true);
            // Texture retains a string_view, so keep its backing string alive throughout Run.
            const std::string texture_path = Assets::Resolve("textures/texture_atlas.png").string();
            TextureArray texture_array;
            texture_array = texture_array.CreateAtlasSlice(texture_path, true);

            glfwSetScrollCallback( (GLFWwindow *) window.window_ptr, MouseScrollCallBack);
            glfwSetCursorPosCallback((GLFWwindow *) window.window_ptr, MouseMovementCallBack);
            glfwSetInputMode((GLFWwindow*)window.window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

            // Manual chunk generation for testing
            InitializeNoise();
            for(int x = -World::chunk_radius; x <= World::chunk_radius; x++)
                for(int z = -World::chunk_radius; z <= World::chunk_radius; z++)
                    ChunkManager::CreateChunk({x, z});

            for( auto chunk : ChunkManager::GetAllChunks() )
            {
                chunk.second.GenerateTerrain();
                chunk.second.GenerateVegetation();
            }

            ChunkManager::RearrangeChunkNeighborPointers();

            Report();

            for(auto& pair : ChunkManager::GetAllChunks())
                if( pair.second.state == ChunkState::Updated)
                    AmoLogger_Log("Chunk (%d, %d) is skipped\n", pair.first.x, pair.first.y);

            ECS::Registry &registry = GetRegistry();
            glm::vec3 start_pos{0.0f, 140.0f, 0.0f};
            auto &transform = registry.GetComponent<Transform>(World::GetPlayer());
            transform.position = start_pos;
            // Renderer::ReportStatus(); # WIP

            // -------------------------------------------------------------------
            // Render Loop
            while (!window.ShouldClose())
            {
                double current_frame_time = glfwGetTime();
                delta_time = (float)(current_frame_time - previous_frame_time);

                block_place_debounce -= delta_time;
                block_change_debounce -= delta_time;

                // Temporary Input Process Function
                processInput((GLFWwindow*)GetWindow().window_ptr);
                PlayerController::DoRayCast(registry, window);

                //::Registry &registry = GetRegistry();

                TransformSystem::Update(GetRegistry());
                Physics::Update(GetRegistry());
                Character::Player::Update(GetRegistry());

                ChunkManager::UpdateAllChunks();
                ChunkManager::LoadAllChunks();

                glBindTextureUnit(0, texture_array.m_texture_Id);
                Renderer::Render();

                window.SwapBuffers();
                window.PollInt();

                previous_frame_time = current_frame_time;
            }
        }

        void Free()
        {
            // Free assets

            // Free resources
            // global_thread_pool->Free();
            // delete global_thread_pool;

            Window& window = GetWindow();
            window.Destroy();
            Window::Free();
            ChunkManager::FreeAllChunks();
            Renderer::Free();
            GetRegistry().Clear();
        }

        Window& GetWindow()
        {
            static Window* window = Window::Create("SymoCraft");
            return *window;
        }

        Camera* GetCamera()
        {
            static auto* camera = new Camera((float)GetWindow().width, (float)GetWindow().height);
            return camera;
        }

        ECS::Registry &GetRegistry()
        {
            static auto* registry = new ECS::Registry;
            return *registry;
        }
/*
        GlobalThreadPool& GetGlobalThreadPool()
        {
            return *global_thread_pool;
        }
*/
        void MouseMovementCallBack(GLFWwindow* window, double xpos_in, double ypos_in)
        {
            static float last_x = 0;       // last x position of cursor
            static float last_y = 0;       // last y position of cursor
            ECS::Registry &registry = Application::GetRegistry();
            //Transform &transform = registry.GetComponent<Transform>(camera->entity_id);
            auto &transform = registry.GetComponent<Transform>(World::GetPlayer());
            auto &player_com = registry.GetComponent<Character::PlayerComponent>(World::GetPlayer());
            auto xpos = static_cast<float>(xpos_in);
            auto ypos = static_cast<float>(ypos_in);

            // modify the first enter of the mouse
            if (first_enter)
            {
                last_x = xpos;
                last_y = ypos;
                first_enter = false;
            }
            float xoffset = xpos - last_x;
            float yoffset = last_y - ypos;   // reversed since y-coordinates range from bottom to top
            last_x = xpos;
            last_y = ypos;

            const float sensitivity = 0.05f;
            xoffset *= sensitivity;
            yoffset *= sensitivity;


            transform.pitch += yoffset;
            transform.yaw += xoffset;
            transform.pitch = glm::clamp(transform.pitch, -89.0f, 89.0f);


        }

        void MouseScrollCallBack(GLFWwindow* window, double x_pos_in, double y_pos_in)
        {
            GetCamera()->InsMouseScrollCallBack(window, x_pos_in, y_pos_in);
        }

        void processInput(GLFWwindow* window)
        {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

            ECS::Registry &registry = GetRegistry();
            auto &player_com = registry.GetComponent<Character::CharacterComponent>(World::GetPlayer());
            auto &rigid_body = registry.GetComponent<Physics::RigidBody>(World::GetPlayer());

            // --------------------------------------------------------------------------------------------
            // process input for camera moving


            player_com.is_running = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

            if (glfwGetKey(window, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS)
            {
                rigid_body.is_sensor = true;
                rigid_body.use_gravity = false;
                player_com.movement_axis.y =
                        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
                        ? -1.0f
                        : 0.0f;
            }
            else
            {
                rigid_body.is_sensor =false;
                rigid_body.use_gravity = true;
            }

            player_com.movement_axis.x =
                    glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS
                    ? 1.0f
                    :glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS
                      ? -1.0f
                      : 0.0f;
            player_com.movement_axis.z =
                    glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS
                    ? 1.0f
                    :
                    glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS
                      ? -1.0f
                      : 0.0f;

            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            {
                if (!player_com.is_jumping && rigid_body.on_ground)
                {
                    player_com.apply_jump_force = true;
                }
            }

            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && block_change_debounce <= 0.0f)
            {
                new_block_id++;
                if (new_block_id > kNumBlocks)
                    new_block_id = 0;
                block_change_debounce = kBlockChangeDebounceTime;
                PlayerController::DisplayCurrentBlockName();
            }

            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS && block_change_debounce <= 0.0f)
            {
                new_block_id--;
                if (new_block_id < 0)
                    new_block_id = kNumBlocks;
                block_change_debounce = kBlockChangeDebounceTime;
                PlayerController::DisplayCurrentBlockName();
            }

        }

    }

}
