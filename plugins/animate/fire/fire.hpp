#ifndef FIRE_ANIMATION_HPP
#define FIRE_ANIMATION_HPP

#include <view-transform.hpp>
#include <render-manager.hpp>

struct Particle
{
    float life;
    float fade;

    float radius, base_radius;

    glm::vec2 pos{0.0, 0.0}, speed{0.0, 0.0}, g{0.0, 0.0};
    glm::vec2 start_pos;

    glm::vec4 color{1.0, 1.0, 1.0, 1.0};

    /* update the given particle
     * time is the percentage of the frame which has elapsed
     * must be thread-safe */
    void update(float time);
};

/* a function to initialize a particle */
using ParticleIniter = std::function<void(Particle&)>;

class ParticleSystem
{
    public:
        /* the user of this class has to set up a proper GL context
         * before creating the ParticleSystem */
        ParticleSystem(int num_part,
                       ParticleIniter part_init_func);
        ~ParticleSystem();

        /* spawn at most num new particles.
         * returns the number of actually spawned particles */
        int spawn(int num);

        /* update all particles */
        void update();

        // number of particles alive
        int statistic();

        /* render particles, each will be multiplied by matrix
         * The user of this class has to set up the same GL context that was
         * used during the creation of the particle system */
        void render(glm::mat4 matrix);

    private:
        ParticleSystem() = delete;

        ParticleIniter pinit_func;
        timespec last_update;

        int particles_alive = 0;
        std::vector<Particle> ps;

        static constexpr int color_per_particle = 4;
        std::vector<float> color, dark_color;

        static constexpr int radius_per_particle = 1;
        std::vector<float> radius;

        static constexpr int center_per_particle = 2;
        std::vector<float> center;

        struct {
            GLuint id;
            GLuint radius, position, center, color;
            GLuint matrix;
        } program;

        void exec_worker_threads(std::function<void(int, int)> spawn_worker);
        void update_worker(float time, int start, int end);
        void create_program();
};

class FireEffect
{
    wayfire_output *output;
    ParticleSystem ps;
    effect_hook_t hook, damage;
    int base_y = 540;

    public:
        FireEffect(wayfire_output *output);

        ~FireEffect();

        void init_particle(Particle& p);
};

#endif /* end of include guard: FIRE_ANIMATION_HPP */
