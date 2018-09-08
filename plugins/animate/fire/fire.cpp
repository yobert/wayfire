#include "fire.hpp"
#include "shaders.hpp"
#include <thread>
#include <output.hpp>
#include <core.hpp>

#include <random>

// generate a random float between s and e
static float random(float s, float e)
{
    double r = 1.0 * (std::rand() % RAND_MAX) / (RAND_MAX - 1);
    return (s * r + (1 - r) * e);
}

void Particle::update(float time)
{
    if (life <= 0) // ignore
        return;

    const float slowdown = 0.8;

    pos += speed * 0.2f * slowdown;
    speed += g * 0.3f * slowdown;

    if (life != 0)
        color.a /= life;

    life -= fade * 0.3 * slowdown;
    radius = base_radius * std::pow(life, 0.5);
    color.a *= life;

    if (start_pos.x < pos.x)
        g.x = -1;
    else
        g.x = 1;

    if (life <= 0)
    {
        /* move outside */
        pos = {-10000, -10000};
    }
}

ParticleSystem::ParticleSystem(int particles, ParticleIniter init_func)
{
    this->pinit_func = init_func;
    ps.resize(particles);

    color.resize(color_per_particle * particles);
    dark_color.resize(color_per_particle * particles);
    radius.resize(radius_per_particle * particles);
    center.resize(center_per_particle * particles);

    clock_gettime(CLOCK_MONOTONIC, &last_update);
    create_program();
}

ParticleSystem::~ParticleSystem()
{
}

int ParticleSystem::spawn(int num)
{
    int spawned = 0;
    for (size_t i = 0; i < ps.size() && spawned < num; i++)
    {
        if (ps[i].life <= 0)
        {
            pinit_func(ps[i]);
            ++spawned;
            ++particles_alive;
        }
    }

    return spawned;
}

void ParticleSystem::update_worker(float time, int start, int end)
{
    end = std::min(end, (int)ps.size());
    for (int i = start; i < end; ++i)
    {
        if (ps[i].life <= 0)
            continue;

    //    printf("%d\n", i);
        ps[i].update(time);

        if (ps[i].life <= 0)
            --particles_alive;

        for (int j = 0; j < 4; j++) // maybe use memcpy?
        {
            color[4 * i + j] = ps[i].color[j];
            dark_color[4 * i + j] = ps[i].color[j] * 0.5;
        }

     //   printf("center %d gets %f", 2 * i, ps[i].pos[0]);
        center[2 * i] = ps[i].pos[0];
        center[2 * i + 1] = ps[i].pos[1];

        radius[i] = ps[i].radius;
    }
}

static int64_t timespec_to_msec(const timespec& ts)
{
    return ts.tv_sec * 1000ll + ts.tv_nsec / 1000000ll;
}

void ParticleSystem::exec_worker_threads(std::function<void(int, int)> spawn_worker)
{
    const int num_threads = std::thread::hardware_concurrency();
    const int worker_load = (ps.size() + num_threads - 1) / num_threads;

    std::thread workers[num_threads];

    for (int i = 0; i < num_threads; i++)
    {
        int thread_start = i * worker_load;
        int thread_end = (i + 1) * worker_load;
        thread_end = std::min(thread_end, (int)ps.size());

        workers[i] = std::thread([=] () { spawn_worker(thread_start, thread_end); });
    }

    for (auto& w : workers)
        w.join();
}

void ParticleSystem::update()
{
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // FIXME: don't hardcode 60FPS
    float time = (timespec_to_msec(now) - timespec_to_msec(last_update)) / 16.0;
    last_update = now;

    exec_worker_threads([=] (int start, int end) {
        update_worker(time, start, end);
    });
}

int ParticleSystem::statistic()
{
    return particles_alive;
}

void ParticleSystem::create_program()
{
    wlr_renderer_begin(core->renderer, 10, 10); // load the proper context

    auto vss = OpenGL::compile_shader(particle_vert_source, GL_VERTEX_SHADER);
    auto fss = OpenGL::compile_shader(particle_frag_source, GL_FRAGMENT_SHADER);

    // TODO: destroy program & shaders properly
    program.id = GL_CALL(glCreateProgram());
    GL_CALL(glAttachShader(program.id, vss));
    GL_CALL(glAttachShader(program.id, fss));
    GL_CALL(glLinkProgram(program.id));

    program.radius   = GL_CALL(glGetAttribLocation(program.id, "radius"));
    program.position = GL_CALL(glGetAttribLocation(program.id, "position"));
    program.center   = GL_CALL(glGetAttribLocation(program.id, "center"));
    program.color    = GL_CALL(glGetAttribLocation(program.id, "color"));
    program.matrix   = GL_CALL(glGetUniformLocation(program.id, "matrix"));

    wlr_renderer_end(core->renderer);
}

void ParticleSystem::render(glm::mat4 matrix)
{
    GL_CALL(glUseProgram(program.id));

    static float vertex_data[] = {
        -1, -1,
         1, -1,
         1,  1,
        -1,  1
    };

    // position
    GL_CALL(glEnableVertexAttribArray(program.position));
    GL_CALL(glVertexAttribPointer(program.position, 2, GL_FLOAT,
                                  false, 0, vertex_data));
    GL_CALL(glVertexAttribDivisor(program.position, 0));

    // particle radius
    GL_CALL(glEnableVertexAttribArray(program.radius));
    GL_CALL(glVertexAttribPointer(program.radius, 1, GL_FLOAT,
                                  false, 0, radius.data()));
    GL_CALL(glVertexAttribDivisor(program.radius, 1));;

    // particle center (offset)
    GL_CALL(glEnableVertexAttribArray(program.center));
    GL_CALL(glVertexAttribPointer(program.center, 2, GL_FLOAT,
                                  false, 0, center.data()));
    GL_CALL(glVertexAttribDivisor(program.center, 1));

    // matrix
    GL_CALL(glUniformMatrix4fv(program.matrix, 1, false, &matrix[0][0]));

    GL_CALL(glEnableVertexAttribArray(program.color));
    GL_CALL(glVertexAttribDivisor(program.color, 1));

    /* Darken the background */
    GL_CALL(glVertexAttribPointer(program.color, 4, GL_FLOAT,
                                  false, 0, dark_color.data()));
    GL_CALL(glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA));
    // TODO: optimize shaders for this case
    GL_CALL(glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, ps.size()));

    // particle color
    GL_CALL(glVertexAttribPointer(program.color, 4, GL_FLOAT,
                                  false, 0, color.data()));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE));
    GL_CALL(glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, ps.size()));

// reset vertex attrib state, other renderers may need this
    GL_CALL(glVertexAttribDivisor(program.position, 0));
    GL_CALL(glVertexAttribDivisor(program.radius, 0));;
    GL_CALL(glVertexAttribDivisor(program.center, 0));
    GL_CALL(glVertexAttribDivisor(program.color, 0));


    GL_CALL(glDisableVertexAttribArray(program.position));
    GL_CALL(glDisableVertexAttribArray(program.radius));
    GL_CALL(glDisableVertexAttribArray(program.center));
    GL_CALL(glDisableVertexAttribArray(program.color));
}

static constexpr int particles = 2500;
FireEffect::FireEffect(wayfire_output *output)
    : ps (particles,
          [=] (Particle& p) { this->init_particle(p); })
{
    std::srand(std::time(NULL));
    this->output = output;

    hook = [=] () {
        wlr_renderer_scissor(core->renderer, NULL);
        OpenGL::use_device_viewport();
        ps.render(output_get_projection(output));
    };
    output->render->add_effect(&hook, WF_OUTPUT_EFFECT_OVERLAY);

    damage = [=] () {
       // base_y -= 5;
        if (base_y <= 0)
        {
            delete this;
            return;
        }
        ps.spawn(particles / 5);
        ps.update();
        output->render->damage(NULL);
    };
    output->render->add_effect(&damage, WF_OUTPUT_EFFECT_PRE);
}

FireEffect::~FireEffect()
{
    output->render->rem_effect(&hook, WF_OUTPUT_EFFECT_OVERLAY);
    output->render->rem_effect(&damage, WF_OUTPUT_EFFECT_PRE);
    output->render->damage(NULL);
}

void FireEffect::init_particle(Particle& p)
{
    p.color = {random(0.4, 1), random(0.08, 0.2), random(0.008, 0.018), 1};
    p.life = 1;
    p.fade = random(0.1, 0.6);
    p.pos = {random(0, 1920), random(base_y - 10, base_y + 10)};
    p.start_pos = p.pos;
    p.speed = {random(-10, 10), random(-25, 5)};
    p.g = {-1, -3};
    p.base_radius = p.radius = random(10, 15) * 1.75;
}
