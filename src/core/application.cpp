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
#include "input/key_snapshot.h"
#include <algorithm>
#include <memory>
#include <stdexcept>

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


        static std::unique_ptr<Window> runtime_window;
        static std::unique_ptr<Camera> camera;
        static std::unique_ptr<ECS::Registry> runtime_registry;
        static bool glfw_initialized = false;
        static bool renderer_started = false;
        static bool keyboard_reset_needed = false;

        static void FocusCallback(GLFWwindow*, int focused)
        {
            if (!focused)
            {
                keyboard_reset_needed = true;
                first_enter = true;
            }
        }

        static void ClearStickyKeys(GLFWwindow* window)
        {
            // Do this after event polling: GLFW emits synthetic releases after the focus callback.
            glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_FALSE);
            glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
            keyboard_reset_needed = false;
        }

        static glm::vec3 FindSpawnPosition()
        {
            // Prefer clear ground near the center rather than spawning inside terrain or a tree.
            for (int radius = 0; radius <= 64; radius += 4)
            {
                for (int x = -radius; x <= radius; x += 4)
                {
                    for (int z = -radius; z <= radius; z += 4)
                    {
                        if (std::max(std::abs(x), std::abs(z)) != radius)
                            continue;
                        for (int y = k_chunk_height - 4; y >= 0; --y)
                        {
                            const Block block = ChunkManager::GetBlock({x, y, z});
                            if (block.block_id == 9)
                                break;
                            if (!get_block(block.block_id).m_is_solid)
                                continue;
                            if (block.block_id >= 2 && block.block_id <= 5)
                                return {x + 0.5f, y + 1.95f, z + 0.5f};
                            break;
                        }
                    }
                }
            }
            throw std::runtime_error("Cannot find a safe player spawn near the world center");
        }

        void Init()
        {
            if (glfw_initialized)
                throw std::logic_error("Application is already initialized");
            Window::Init();
            glfw_initialized = true;
            runtime_window.reset(Window::Create("SymoCraft"));
            if (!runtime_window || !runtime_window->window_ptr)
                throw std::runtime_error("Cannot create the game window");
            runtime_registry = std::make_unique<ECS::Registry>();

            // Initialize all other subsystems.
            ECS::Registry &registry = GetRegistry();
            registry.RegisterComponent<Transform>("Transform");
            registry.RegisterComponent<Physics::RigidBody>("RigidBody");
            registry.RegisterComponent<Physics::HitBox>("HigBox");
            registry.RegisterComponent<Character::CharacterComponent>("CharacterComponent");
            registry.RegisterComponent<Character::PlayerComponent>("PlayerComponent");

            renderer_started = true;
            Renderer::Init();
            World::Init();
            first_enter = true;
            keyboard_reset_needed = false;
        }

        void Run(unsigned int frame_limit)
        {
            Window& window = GetWindow();
            auto* native_window = static_cast<GLFWwindow*>(window.window_ptr);
            const double loading_start = glfwGetTime();

            stbi_set_flip_vertically_on_load(true);
            const std::string texture_path = Assets::Resolve("textures/texture_atlas.png").string();
            TextureArray texture_array;
            texture_array = texture_array.CreateAtlasSlice(texture_path, true);
            ValidateBlockTextures(texture_array.layer_amount);

            glfwSetScrollCallback( (GLFWwindow *) window.window_ptr, MouseScrollCallBack);
            glfwSetCursorPosCallback((GLFWwindow *) window.window_ptr, MouseMovementCallBack);
            glfwSetInputMode((GLFWwindow*)window.window_ptr, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetInputMode(native_window, GLFW_STICKY_KEYS, GLFW_TRUE);
            glfwSetWindowFocusCallback(native_window, FocusCallback);

            // Manual chunk generation for testing
            InitializeNoise();
            for(int x = -World::chunk_radius; x <= World::chunk_radius; x++)
                for(int z = -World::chunk_radius; z <= World::chunk_radius; z++)
                    ChunkManager::CreateChunk({x, z});

            ChunkManager::RearrangeChunkNeighborPointers();
            for (auto& [coords, chunk] : ChunkManager::GetAllChunks())
                chunk.GenerateTerrain();
            for (auto& [coords, chunk] : ChunkManager::GetAllChunks())
                chunk.GenerateVegetation();

            Report();

            ECS::Registry &registry = GetRegistry();
            const glm::vec3 start_pos = FindSpawnPosition();
            auto &transform = registry.GetComponent<Transform>(World::GetPlayer());
            transform.position = start_pos;
            TransformSystem::Update(registry);
            Character::Player::SyncCamera(registry);
            ChunkManager::UpdateAllChunks();

            // Loading is not simulation time. Start the frame clock only after the initial mesh is ready.
            Physics::ResetTiming();
            double previous_frame_time = glfwGetTime();
            bool paused = false;
            unsigned int rendered_frames = 0;
            std::cout << "[runtime] ready; chunks=" << ChunkManager::GetAllChunks().size()
                      << "; loading_ms=" << (previous_frame_time - loading_start) * 1000.0
                      << "; spawn=" << start_pos.x << ',' << start_pos.y << ',' << start_pos.z << std::endl;

            // -------------------------------------------------------------------
            // Render Loop
            while (!window.ShouldClose())
            {
                window.PollInt();
                if (window.ShouldClose())
                    break;
                const double current_frame_time = glfwGetTime();
                auto& character = registry.GetComponent<Character::CharacterComponent>(World::GetPlayer());
                auto& body = registry.GetComponent<Physics::RigidBody>(World::GetPlayer());
                const bool inactive = !glfwGetWindowAttrib(native_window, GLFW_FOCUSED) ||
                                      glfwGetWindowAttrib(native_window, GLFW_ICONIFIED) ||
                                      window.width <= 0 || window.height <= 0;
                if (inactive && frame_limit == 0)
                {
                    ClearStickyKeys(native_window);
                    character.movement_axis = glm::vec3(0.0f);
                    character.apply_jump_force = false;
                    body.velocity.x = body.velocity.z = 0.0f;
                    Physics::ResetTiming();
                    first_enter = true;
                    paused = true;
                    previous_frame_time = current_frame_time;
                    glfwWaitEventsTimeout(0.05);
                    continue;
                }
                if (paused || keyboard_reset_needed)
                {
                    ClearStickyKeys(native_window);
                    character.movement_axis = glm::vec3(0.0f);
                    character.apply_jump_force = false;
                    body.velocity.x = body.velocity.z = 0.0f;
                    Physics::ResetTiming();
                    first_enter = true;
                    paused = true;
                }
                delta_time = paused ? 0.0f : static_cast<float>(std::clamp(current_frame_time - previous_frame_time, 0.0, 0.1));
                paused = false;
                previous_frame_time = current_frame_time;

                block_place_debounce -= delta_time;
                block_change_debounce -= delta_time;

                // Temporary Input Process Function
                processInput(native_window);
                if (window.ShouldClose())
                    break;
                TransformSystem::Update(registry);
                Character::Player::Update(registry);
                Physics::Update(registry, delta_time);
                if (transform.position.y < -8.0f)
                {
                    transform.position = FindSpawnPosition();
                    body.zero_forces();
                    body.on_ground = false;
                    character.is_jumping = false;
                    Physics::ResetTiming();
                    std::cout << "[runtime] returned player to safe spawn" << std::endl;
                }
                Character::Player::SyncCamera(registry);
                PlayerController::DoRayCast(registry, window);

                ChunkManager::UpdateAllChunks();
                ChunkManager::LoadAllChunks();

                glBindTextureUnit(0, texture_array.m_texture_Id);
                Renderer::Render();

                window.SwapBuffers();
                ++rendered_frames;
                if (frame_limit != 0 && rendered_frames >= frame_limit)
                    window.Close();
            }
            std::cout << "[runtime] loop finished; rendered_frames=" << rendered_frames
                      << "; player=" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z
                      << std::endl;
        }

        void Free()
        {
            ChunkManager::FreeAllChunks();
            if (renderer_started)
            {
                Renderer::Free();
                renderer_started = false;
            }
            camera.reset();
            if (runtime_registry)
            {
                runtime_registry->Clear();
                runtime_registry.reset();
            }
            if (runtime_window)
            {
                runtime_window->Destroy();
                runtime_window.reset();
            }
            if (glfw_initialized)
            {
                Window::Free();
                glfw_initialized = false;
            }
        }

        Window& GetWindow()
        {
            if (!runtime_window)
                throw std::logic_error("Game window is not initialized");
            return *runtime_window;
        }

        Camera* GetCamera()
        {
            if (!camera)
                camera = std::make_unique<Camera>(static_cast<float>(GetWindow().width), static_cast<float>(GetWindow().height));
            return camera.get();
        }

        ECS::Registry &GetRegistry()
        {
            if (!runtime_registry)
                throw std::logic_error("Game registry is not initialized");
            return *runtime_registry;
        }
/*
        GlobalThreadPool& GetGlobalThreadPool()
        {
            return *global_thread_pool;
        }
*/
        void MouseMovementCallBack(GLFWwindow* window, double xpos_in, double ypos_in)
        {
            if (!glfwGetWindowAttrib(window, GLFW_FOCUSED))
            {
                first_enter = true;
                return;
            }
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
            using KeySampling::Control;
            constexpr std::array<int, KeySampling::Count> native_keys{
                GLFW_KEY_ESCAPE, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_CAPS_LOCK, GLFW_KEY_LEFT_CONTROL,
                GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_A, GLFW_KEY_SPACE, GLFW_KEY_E, GLFW_KEY_Q
            };
            const auto keys = KeySampling::Capture([window, &native_keys](Control control)
            {
                return glfwGetKey(window, native_keys[static_cast<std::size_t>(control)]) == GLFW_PRESS;
            });
            if (keys.Down(Control::Exit))
                glfwSetWindowShouldClose(window, true);

            ECS::Registry &registry = GetRegistry();
            auto &player_com = registry.GetComponent<Character::CharacterComponent>(World::GetPlayer());
            auto &rigid_body = registry.GetComponent<Physics::RigidBody>(World::GetPlayer());

            // --------------------------------------------------------------------------------------------
            // process input for camera moving


            player_com.is_running = keys.Down(Control::Run);
            player_com.movement_axis.y = 0.0f;

            if (keys.Down(Control::Sensor))
            {
                rigid_body.is_sensor = true;
                rigid_body.use_gravity = false;
                player_com.movement_axis.y =
                        keys.Down(Control::Descend)
                        ? -1.0f
                        : 0.0f;
            }
            else
            {
                rigid_body.is_sensor =false;
                rigid_body.use_gravity = true;
            }

            player_com.movement_axis.x =
                    keys.Down(Control::Forward)
                    ? 1.0f
                    :keys.Down(Control::Backward)
                      ? -1.0f
                      : 0.0f;
            player_com.movement_axis.z =
                    keys.Down(Control::Right)
                    ? 1.0f
                    :
                    keys.Down(Control::Left)
                      ? -1.0f
                      : 0.0f;

            if (keys.Down(Control::Jump))
            {
                if (!player_com.is_jumping && rigid_body.on_ground)
                {
                    player_com.apply_jump_force = true;
                }
            }

            if (keys.Down(Control::NextBlock) && block_change_debounce <= 0.0f)
            {
                new_block_id++;
                if (new_block_id > kNumBlocks)
                    new_block_id = 0;
                block_change_debounce = kBlockChangeDebounceTime;
                PlayerController::DisplayCurrentBlockName();
            }

            if (keys.Down(Control::PreviousBlock) && block_change_debounce <= 0.0f)
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
