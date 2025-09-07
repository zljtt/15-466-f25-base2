#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <random>

struct PlayMode : Mode
{
    PlayMode();
    virtual ~PlayMode();
    // functions called by main loop:
    virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
    virtual void update(float elapsed) override;
    virtual void draw(glm::uvec2 const &drawable_size) override;

    //----- game state -----

    // input tracking:
    struct Button
    {
        uint8_t downs = 0;
        uint8_t pressed = 0;
    } left, right, down, up;

    // local copy of the game scene (so code can change it during gameplay):
    Scene scene;

    // hexapod leg to wobble:
    Scene::Transform *player = nullptr;
    Scene::Transform *water = nullptr;

    int fish_point = 0;
    float fish_spawn_timer = 0;
    float timer = 60;
    int fish_to_add = 10;
    glm::quat hip_base_rotation;
    glm::quat upper_leg_base_rotation;
    glm::quat lower_leg_base_rotation;
    float wobble = 0.0f;
    // camera:
    Scene::Camera *camera = nullptr;
    glm::vec3 camera_offset;
    glm::quat camera_relative_angle;

    void add_fish(glm::vec3 pos, glm::quat rot, int type, int size);
    void update_fishes(float elapsed);
    void check_collision();
    void remove_fish(Scene::Drawable &drawable, bool is_eaten);
};
