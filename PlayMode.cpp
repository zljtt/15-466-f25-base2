#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <random>
#include <iomanip>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

std::mt19937 rng{std::random_device{}()};

GLuint fish_meshes_for_lit_color_texture_program = 0;
Load<MeshBuffer> fish_meshes(LoadTagDefault, []() -> MeshBuffer const *
                             {
	MeshBuffer const *ret = new MeshBuffer(data_path("fish.pnct"));
	fish_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret; });

Load<Scene> fish_scene(LoadTagDefault, []() -> Scene const *
                       { return new Scene(data_path("fish.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name)
                                          {
                                                 Mesh const &mesh = fish_meshes->lookup(mesh_name);

                                                 scene.drawables.emplace_back(transform);
                                                 Scene::Drawable &drawable = scene.drawables.back();

                                                 drawable.pipeline = lit_color_texture_program_pipeline;

                                                 drawable.pipeline.vao = fish_meshes_for_lit_color_texture_program;
                                                 drawable.pipeline.type = mesh.type;
                                                 drawable.pipeline.start = mesh.start;
                                                 drawable.pipeline.count = mesh.count; }); });

PlayMode::PlayMode() : scene(*fish_scene)
{

    // get pointers to leg for convenience:
    for (auto &transform : scene.transforms)
    {
        if (transform.name == "Player")
        {
            player = &transform;
        }
        else if (transform.name == "Water")
        {
            water = &transform;
        }
        // else if (transform.name == "UpperLeg.FL")
        //     upper_leg = &transform;
        // else if (transform.name == "LowerLeg.FL")
        //     lower_leg = &transform;
    }
    // if (hip == nullptr)
    //     throw std::runtime_error("Hip not found.");
    // if (upper_leg == nullptr)
    //     throw std::runtime_error("Upper leg not found.");
    // if (lower_leg == nullptr)
    //     throw std::runtime_error("Lower leg not found.");

    // hip_base_rotation = hip->rotation;
    // upper_leg_base_rotation = upper_leg->rotation;
    // lower_leg_base_rotation = lower_leg->rotation;

    // get pointer to camera for convenience:
    if (scene.cameras.size() != 1)
        throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
    camera = &scene.cameras.front();
    camera_offset = camera->transform->position - player->position;
    camera_relative_angle = glm::normalize(glm::inverse(player->rotation) * camera->transform->rotation);
}

PlayMode::~PlayMode()
{
}

void PlayMode::add_fish(glm::vec3 pos, glm::quat rot, int type, int size)
{
    // std::cout << "add fish at " << pos.x << " " << pos.y << " " << pos.z << " " << "\n";
    Mesh const &mesh = fish_meshes->lookup("Fish" + std::to_string(type));
    Scene::Transform &fish = scene.transforms.emplace_back();
    Scene::Transform *fish_p = &fish;
    // fish_p->parent = water;
    fish_p->position = pos;
    fish_p->scale = glm::vec3(0.1f + size * 0.1f, 0.1f + size * 0.1f, 0.1f + size * 0.1f);
    fish_p->rotation = rot;
    scene.drawables.emplace_back(fish_p);
    Scene::Drawable &drawable = scene.drawables.back();
    drawable.fish_type = type;
    drawable.scared_timer = 0;
    drawable.size = size;

    drawable.pipeline = lit_color_texture_program_pipeline;
    drawable.pipeline.vao = fish_meshes_for_lit_color_texture_program;
    drawable.pipeline.type = mesh.type;
    drawable.pipeline.start = mesh.start;
    drawable.pipeline.count = mesh.count;
}

void PlayMode::update_fishes(float elapsed)
{
    const glm::vec3 up(0, 0, 1);
    const float move_speed = 1.5f;
    const float max_yaw = 2.0f;

    for (auto &d : scene.drawables)
    {
        // if fish
        if (d.fish_type > 0 && d.transform)
        {
            if (d.scared_timer > 0)
            {
                d.scared_timer -= elapsed;
                glm::vec3 away_from_player = glm::normalize(d.transform->position - player->position);
                // queried ChatGPT for angle calculation
                glm::quat q = glm::rotation(glm::vec3(0, 1, 0), away_from_player);
                d.transform->rotation = q;
                glm::vec3 forward = glm::normalize(glm::vec3(away_from_player.x, away_from_player.y, 0.0f));
                d.transform->position += forward * move_speed * 2.0f * elapsed;
            }
            else
            {
                if (d.random_yaw_timer < 0)
                {
                    d.random_yaw_timer += (std::rand() / float(RAND_MAX)) * 0.3f;
                    float random_yaw = (std::rand() / float(RAND_MAX)) * 4.0f - 2.0f;
                    float yaw_d = random_yaw * max_yaw * elapsed;
                    glm::quat yaw_q = glm::angleAxis(yaw_d, up);
                    d.transform->rotation = glm::normalize(yaw_q * d.transform->rotation);
                }
                glm::vec3 forward = d.transform->rotation * glm::vec3(0, 1, 0);
                d.transform->position += forward * move_speed * elapsed;
                d.random_yaw_timer -= elapsed;
            }
        }
    }
}

void PlayMode::check_collision()
{
    std::vector<Scene::Drawable> eaten;
    std::vector<Scene::Drawable> to_remove;
    for (auto &d : scene.drawables)
    {
        // if fish
        if (d.fish_type > 0)
        {
            // too close - collide
            if (glm::distance(d.transform->position, player->position) < 0.4 + fish_point * 0.01f)
            {
                eaten.push_back(d);
            }
            // too far - remove
            if (glm::distance(d.transform->position, player->position) > 20)
            {
                to_remove.push_back(d);
            }
        }
    }

    for (auto &d : eaten)
    {
        remove_fish(d, true);
    }
    for (auto &d : to_remove)
    {
        remove_fish(d, false);
    }
}

void PlayMode::remove_fish(Scene::Drawable &drawable, bool eaten)
{
    if (eaten)
    {
        fish_point += drawable.size;

        for (auto &d : scene.drawables)
        {
            // if fish
            if (d.fish_type > 0 && d.fish_type == drawable.fish_type)
            {
                if (glm::distance(d.transform->position, drawable.transform->position) < 10)
                {
                    d.scared_timer = 5;
                }
            }
        }
    }
    scene.drawables.remove_if([&](Scene::Drawable d)
                              { return d.transform == drawable.transform; });

    scene.transforms.remove_if([&](const Scene::Transform &t)
                               { return t.position == drawable.transform->position; });
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size)
{
    if (timer < 0)
        return false;
    if (evt.type == SDL_EVENT_KEY_DOWN)
    {
        if (evt.key.key == SDLK_ESCAPE)
        {
            SDL_SetWindowRelativeMouseMode(Mode::window, false);
            return true;
        }
        else if (evt.key.key == SDLK_A)
        {
            left.downs += 1;
            left.pressed = true;
            return true;
        }
        else if (evt.key.key == SDLK_D)
        {
            right.downs += 1;
            right.pressed = true;
            return true;
        }
        else if (evt.key.key == SDLK_W)
        {
            up.downs += 1;
            up.pressed = true;
            return true;
        }
        else if (evt.key.key == SDLK_S)
        {
            down.downs += 1;
            down.pressed = true;
            return true;
        }
    }
    else if (evt.type == SDL_EVENT_KEY_UP)
    {
        if (evt.key.key == SDLK_A)
        {
            left.pressed = false;
            return true;
        }
        else if (evt.key.key == SDLK_D)
        {
            right.pressed = false;
            return true;
        }
        else if (evt.key.key == SDLK_W)
        {
            up.pressed = false;
            return true;
        }
        else if (evt.key.key == SDLK_S)
        {
            down.pressed = false;
            return true;
        }
    }
    else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (SDL_GetWindowRelativeMouseMode(Mode::window) == false)
        {
            SDL_SetWindowRelativeMouseMode(Mode::window, true);
            return true;
        }
    }
    else if (evt.type == SDL_EVENT_MOUSE_MOTION)
    {
        if (SDL_GetWindowRelativeMouseMode(Mode::window) == true)
        {
            glm::vec2 motion = glm::vec2(
                evt.motion.xrel / float(window_size.y),
                -evt.motion.yrel / float(window_size.y));
            camera->transform->rotation = glm::normalize(
                camera->transform->rotation * glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f)));
            return true;
        }
    }

    return false;
}

void PlayMode::update(float elapsed)
{
    if (timer < 0)
        return;

    timer -= elapsed;
    update_fishes(elapsed);

    check_collision();
    static const float SpawnCooldown = 1.0f;
    static const float SpawnRange = 5;

    // spawn fish
    {
        if (fish_spawn_timer < 0)
        {
            fish_to_add += 5;
            fish_spawn_timer += SpawnCooldown;
        }
        fish_spawn_timer -= elapsed;

        while (fish_to_add > 0)
        {
            float randx, randy;
            if (std::rand() % 2 == 0)
            {
                randx = -2 * SpawnRange + (std::rand() / float(RAND_MAX)) * SpawnRange;
            }
            else
            {
                randx = SpawnRange + (std::rand() / float(RAND_MAX)) * SpawnRange;
            }
            if (std::rand() % 2 == 0)
            {
                randy = -2 * SpawnRange + (std::rand() / float(RAND_MAX)) * SpawnRange;
            }
            else
            {
                randy = SpawnRange + (std::rand() / float(RAND_MAX)) * SpawnRange;
            }
            glm::vec3 spawnPos(player->position.x + randx, player->position.y + randy, 0);
            // queried ChatGPT for random quad
            std::uniform_real_distribution<float> yawDist(0.0f, glm::two_pi<float>());
            float yaw = yawDist(rng);
            auto random_rotation = glm::angleAxis(yaw, glm::vec3(0, 0, 1));
            int random_size = 1;
            int r = std::rand() % 100;
            if (r < 10)
                random_size = 3;
            else if (r < 30)
                random_size = 2;
            else
                random_size = 1;
            add_fish(spawnPos, random_rotation, (std::rand() % 3) + 1, random_size);
            fish_to_add--;
        }
    }
    // player movement
    {

        // combine inputs into a move:
        constexpr float PlayerSpeed = 4.0f;
        constexpr float PlayerTurnSpeed = glm::radians(60.0f);
        glm::vec2 move = glm::vec2(0.0f);
        if (left.pressed && !right.pressed)
            move.y = -1.0f;
        if (!left.pressed && right.pressed)
            move.y = 1.0f;
        // if (down.pressed && !up.pressed)
        // move.x = 1.0f;
        // if (!down.pressed && up.pressed)
        //     move.x = -1.0f;

        // make it so that moving diagonally doesn't go faster:
        // if (move != glm::vec2(0.0f))
        //     move = glm::normalize(move) * PlayerSpeed * elapsed;

        // glm::mat4x3 frame = camera->transform->make_parent_from_local();
        // glm::vec3 frame_right = frame[0];
        // // glm::vec3 up = frame[1];
        // glm::vec3 frame_forward = -frame[2];
        // camera->transform->position += move.x * frame_right + move.y * frame_forward;
        // player->position += glm::vec3(move.x, move.y, 0);
        if (move.y != 0)
        {
            float yawD = move.y * PlayerTurnSpeed * elapsed;
            glm::quat yawQ = glm::angleAxis(-yawD, glm::vec3(0.0f, 0.0f, 1.0f));
            player->rotation = glm::normalize(yawQ * player->rotation);
            // camera->transform->rotation = player->rotation;
        }
        if (up.pressed)
        {
            glm::vec3 forward = player->rotation * glm::vec3(-1.0f, 0.0f, 0.0f);
            player->position += forward * PlayerSpeed * elapsed;
            // camera->transform->position += forward * PlayerSpeed * elapsed;
        }
        player->scale = glm::vec3(1 + fish_point * 0.05, 1 + fish_point * 0.05, 1 + fish_point * 0.05);
        camera->transform->rotation = glm::normalize(player->rotation * camera_relative_angle);
        camera->transform->position = player->position + player->rotation * camera_offset;
    }

    // reset button press counters:
    left.downs = 0;
    right.downs = 0;
    up.downs = 0;
    down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size)
{
    // update camera aspect ratio for drawable:
    camera->aspect = float(drawable_size.x) / float(drawable_size.y);

    // set up light type and position for lit_color_texture_program:
    //  TODO: consider using the Light(s) in the scene to do this
    glUseProgram(lit_color_texture_program->program);
    glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
    glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
    glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
    glUseProgram(0);

    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClearDepth(1.0f); // 1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); // this is the default depth comparison function, but FYI you can change it.

    GL_ERRORS(); // print any errors produced by this setup code

    scene.draw(*camera);

    { // use DrawLines to overlay some text:
        glDisable(GL_DEPTH_TEST);
        float aspect = float(drawable_size.x) / float(drawable_size.y);
        DrawLines lines(glm::mat4(
            1.0f / aspect, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f));

        constexpr float H = 0.09f;
        std::ostringstream formatter;
        formatter << "Fish Eaten: " << fish_point << "      Time Left: " << std::fixed << std::setprecision(2) << timer;

        lines.draw_text(formatter.str(),
                        glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
                        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                        glm::u8vec4(0x00, 0x00, 0x00, 0x00));
        float ofs = 2.0f / drawable_size.y;
        lines.draw_text(formatter.str(),
                        glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
                        glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                        glm::u8vec4(0xff, 0xff, 0xff, 0x00));
    }
}
