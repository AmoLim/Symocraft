#include "core/ECS/Systems/character_system.h"
#include "core/ECS/component.h"
#include "core/ECS/registry.h"
#include "core/application.h"
#include "camera/camera.h"
#include "playercontroller/player_math.h"

namespace SymoCraft::Character::Player
{
    void Update(ECS::Registry& registry)
    {
        for (ECS::EntityId entity : registry.View<Transform, CharacterComponent, Physics::RigidBody>())
        {
            const auto& transform = registry.GetComponent<Transform>(entity);
            auto& character = registry.GetComponent<CharacterComponent>(entity);
            auto& body = registry.GetComponent<Physics::RigidBody>(entity);
            const float speed = character.is_running ? character.run_speed : character.base_speed;

            const glm::vec3 desired = PlayerMath::DesiredVelocity(character.movement_axis, transform.yaw,
                                                                  speed, body.use_gravity);
            body.velocity.x = desired.x;
            body.velocity.z = desired.z;
            if (!body.use_gravity)
            {
                body.velocity.y = desired.y;
                body.acceleration.y = 0.0f;
                body.on_ground = false;
                character.is_jumping = false;
            }

            if (body.on_ground)
            {
                character.is_jumping = false;
                body.acceleration.y = 0.0f;
            }
            if (character.apply_jump_force && body.use_gravity && body.on_ground && !body.is_sensor)
            {
                body.velocity.y = character.jump_force;
                body.acceleration.y = 0.0f;
                body.on_ground = false;
                character.is_jumping = true;
            }
            character.apply_jump_force = false;

            if (character.is_jumping && body.velocity.y <= 0.0f)
            {
                body.acceleration.y = character.down_jump_force;
                character.is_jumping = false;
            }
        }
    }

    void SyncCamera(ECS::Registry& registry)
    {
        const auto camera_entity = Application::GetCamera()->entity_id;
        if (camera_entity == ECS::null_entity || !registry.HasComponent<Transform>(camera_entity))
            return;
        auto& camera_transform = registry.GetComponent<Transform>(camera_entity);
        for (ECS::EntityId entity : registry.View<Transform, PlayerComponent>())
        {
            const auto& transform = registry.GetComponent<Transform>(entity);
            const auto& player = registry.GetComponent<PlayerComponent>(entity);
            camera_transform.position = transform.position + player.camera_offset;
            camera_transform.yaw = transform.yaw;
            camera_transform.pitch = transform.pitch;
            camera_transform.front = transform.front;
            camera_transform.up = transform.up;
            camera_transform.right = transform.right;
        }
    }
}
