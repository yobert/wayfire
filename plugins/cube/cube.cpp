#include <opengl.hpp>
#include <output.hpp>
#include <core.hpp>
#include <render-manager.hpp>
#include <workspace-manager.hpp>
#include <animation.hpp>
#include <nonstd/make_unique.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <linux/input-event-codes.h>
#include <config.h>

#define Z_OFFSET_NEAR 0.89567f
#define Z_OFFSET_FAR  2.00000f

#define ZOOM_MAX 10.0f
#define ZOOM_MIN 0.1f


#ifdef USE_GLES32
#include <GLES3/gl32.h>
#endif

class wayfire_cube : public wayfire_plugin_t
{
    button_callback activate;
    render_hook_t renderer;

    std::vector<std::unique_ptr<wf_workspace_stream>> streams;

    wf_option XVelocity, YVelocity, ZVelocity;

    float side_angle;
    /* the Z camera distance so that (-1, 1) is mapped to the whole screen
     * for the given FOV */
    float identity_z_offset;

    wf_point grab_pos;

    struct {
        GLuint id = -1;
        GLuint modelID, vpID;
        GLuint posID, uvID;
#ifdef USE_GLES32
        GLuint defID, lightID;
        GLuint easeID;
#endif
    } program;

    struct
    {
        wf_transition offset_y {0, 0}, offset_z {0, 0}, rotation {0, 0}, zoom{1, 1};
#ifdef USE_GLES32
        wf_transition ease_deformation{0, 0};
#endif

        bool in_exit, active = false;
    } animation;

    wf_duration duration;



    glm::mat4 vp, model, view, project;

#ifdef USE_GLES32
    wf_option use_light, use_deform;
#endif

    wf_option background_color;
    bool tessellation_support;

    public:
    void init(wayfire_config *config)
    {
        grab_interface->name = "cube";
        grab_interface->abilities_mask = WF_ABILITY_CONTROL_WM;

        auto section = config->get_section("cube");

        XVelocity  = section->get_option("speed_spin_horiz", "0.1");
        YVelocity  = section->get_option("speed_spin_vert",  "0.1");
        ZVelocity  = section->get_option("speed_zoom",       "0.05");

        duration = wf_duration(section->get_option("initial_animation", "350"));
        duration.start();

        background_color = section->get_option("background", "0 0 0 1");

        auto button = section->get_option("activate", "<alt> <ctrl> BTN_LEFT");
        activate = [=] (uint32_t, int32_t, int32_t) {
            initiate();
        };

        output->add_button(button, &activate);

        grab_interface->callbacks.pointer.button = [=] (uint32_t b, uint32_t s)
        {
            if (s == WL_POINTER_BUTTON_STATE_RELEASED)
                input_released();
        };

        grab_interface->callbacks.pointer.motion = [=] (int32_t, int32_t)
        {
            pointer_moved();
        };

        grab_interface->callbacks.pointer.axis = [=] (
                wlr_event_pointer_axis *ev) {
            if (ev->orientation == WLR_AXIS_ORIENTATION_VERTICAL)
                pointer_scrolled(ev->delta);
        };

        grab_interface->callbacks.cancel = [=] ()
        {
            terminate();
        };


#ifdef USE_GLES32
        use_light  = section->get_option("light", "1");
        use_deform = section->get_option("deform", "0");
#endif

        auto vw = std::get<0>(output->workspace->get_workspace_grid_size());
        side_angle = 2 * M_PI / float(vw);
        identity_z_offset = 0.5 / std::tan(side_angle / 2);
        animation.offset_z = {identity_z_offset + Z_OFFSET_NEAR,
            identity_z_offset + Z_OFFSET_NEAR};

        renderer = [=] (uint32_t fb) {render(fb);};
    }

    void schedule_next_frame()
    {
        output->render->schedule_redraw();
        output->render->damage(NULL);
    }

    void load_program()
    {

#ifdef USE_GLES32
        std::string ext_string(reinterpret_cast<const char*> (glGetString(GL_EXTENSIONS)));
        tessellation_support =
            ext_string.find(std::string("GL_EXT_tessellation_shader")) != std::string::npos;
#else
        tesselation_support = false;
#endif

        std::string shaderSrcPath;
        if (tessellation_support)
        {
            shaderSrcPath = INSTALL_PREFIX "/share/wayfire/cube/shaders_3.2";
        } else
        {
            shaderSrcPath = INSTALL_PREFIX "/share/wayfire/cube/shaders_2.0";
        }

        program.id = GL_CALL(glCreateProgram());
        GLuint vss, fss, tcs = -1, tes = -1, gss = -1;

        vss = OpenGL::load_shader(std::string(shaderSrcPath)
                                  .append("/vertex.glsl").c_str(), GL_VERTEX_SHADER);

        fss = OpenGL::load_shader(std::string(shaderSrcPath)
                                  .append("/frag.glsl").c_str(), GL_FRAGMENT_SHADER);

        GL_CALL(glAttachShader(program.id, vss));
        GL_CALL(glAttachShader(program.id, fss));

        if (tessellation_support)
        {
        tcs = OpenGL::load_shader(std::string(shaderSrcPath)
                                  .append("/tcs.glsl").c_str(),
                                  GL_TESS_CONTROL_SHADER);

        tes = OpenGL::load_shader(std::string(shaderSrcPath)
                                  .append("/tes.glsl").c_str(),
                                  GL_TESS_EVALUATION_SHADER);

        gss = OpenGL::load_shader(std::string(shaderSrcPath)
                                  .append("/geom.glsl").c_str(),
                                  GL_GEOMETRY_SHADER);
        GL_CALL(glAttachShader(program.id, tcs));
        GL_CALL(glAttachShader(program.id, tes));
        GL_CALL(glAttachShader(program.id, gss));
        }

        GL_CALL(glLinkProgram(program.id));
        GL_CALL(glUseProgram(program.id));

        GL_CALL(glDeleteShader(vss));
        GL_CALL(glDeleteShader(fss));
        if (tessellation_support)
        {
            GL_CALL(glDeleteShader(tcs));
            GL_CALL(glDeleteShader(tes));
            GL_CALL(glDeleteShader(gss));
        }

        program.vpID = GL_CALL(glGetUniformLocation(program.id, "VP"));
        GL_CALL(glUniformMatrix4fv(program.vpID, 1, GL_FALSE, &vp[0][0]));

        program.uvID = GL_CALL(glGetAttribLocation(program.id, "uvPosition"));
        program.posID = GL_CALL(glGetAttribLocation(program.id, "position"));
        program.modelID = GL_CALL(glGetUniformLocation(program.id, "model"));

        if (tessellation_support)
        {
            program.defID = GL_CALL(glGetUniformLocation(program.id, "deform"));
            program.lightID = GL_CALL(glGetUniformLocation(program.id, "light"));
            program.easeID = GL_CALL(glGetUniformLocation(program.id, "ease"));
        }

        GetTuple(vw, vh, output->workspace->get_workspace_grid_size());
        (void) vh; // silence compiler warning

        streams.resize(vw);
        for(int i = 0; i < vw; i++)
        {
            streams[i] = nonstd::make_unique<wf_workspace_stream>();
            streams[i]->fbuff = streams[i]->tex = -1;
        }

        project = glm::perspective(45.0f, 1.f, 0.1f, 100.f);
    }

    void initiate()
    {
        if (!animation.active)
        {
            if (!output->activate_plugin(grab_interface))
                return;

            grab_interface->grab();
            output->render->set_renderer(renderer);
        }

        animation.in_exit = false;
        animation.active = true;

        animation.rotation = {duration.progress(animation.rotation), 0};
        animation.offset_y = {duration.progress(animation.offset_y), 0};
        animation.offset_z = {duration.progress(animation.offset_z),
            identity_z_offset + Z_OFFSET_FAR};
        animation.zoom = {duration.progress(animation.zoom), 1};
        animation.ease_deformation = {0, 1};

        duration.start();

        if (update_animation())
            schedule_next_frame();

        GetTuple(px, py, output->get_cursor_position());
        grab_pos = {px, py};
    }

    bool update_animation()
    {
        view = glm::lookAt(glm::vec3(0., duration.progress(animation.offset_y),
                duration.progress(animation.offset_z)),
            glm::vec3(0., 0., 0.),
            glm::vec3(0., 1., 0.));

        return duration.running();
    }

    void render(uint32_t target_fb)
    {
        if (program.id == (uint)-1)
            load_program();

        OpenGL::use_device_viewport();

        auto clear_color = background_color->as_cached_color();
        GL_CALL(glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a));

        GL_CALL(glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT));

        GetTuple(vx, vy, output->workspace->get_current_workspace());
        for(size_t i = 0; i < streams.size(); i++) {
            if (!streams[i]->running) {
                streams[i]->ws = std::make_tuple(i, vy);
                output->render->workspace_stream_start(streams[i].get());
            } else {
                output->render->workspace_stream_update(streams[i].get());
            }
        }

        GL_CALL(glUseProgram(program.id));
        GL_CALL(glEnable(GL_DEPTH_TEST));
        GL_CALL(glDepthFunc(GL_LESS));

        OpenGL::use_device_viewport();
        GL_CALL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target_fb));

        float zoom_factor = duration.progress(animation.zoom);
        glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0),
                glm::vec3(1. / zoom_factor, 1. / zoom_factor, 1. / zoom_factor));

        vp = output->render->get_target_framebuffer().transform *
            project * view * scale_matrix;
        GL_CALL(glUniformMatrix4fv(program.vpID, 1, GL_FALSE, &vp[0][0]));

        if (tessellation_support)
        {
            GL_CALL(glUniform1i(program.defID, *use_deform));
            GL_CALL(glUniform1i(program.lightID, *use_light));
            GL_CALL(glUniform1f(program.easeID,
                    duration.progress(animation.ease_deformation)));
        }

        GLfloat vertexData[] = {
            -0.5,  0.5,
             0.5,  0.5,
             0.5, -0.5,
            -0.5,  0.5,
             0.5, -0.5,
            -0.5, -0.5
        };

        GLfloat coordData[] = {
            0.0f, 1.0f,
            1.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 0.0f,
            0.0f, 0.0f,
        };

        GL_CALL(glVertexAttribPointer(program.posID, 2, GL_FLOAT, GL_FALSE, 0, vertexData));
        GL_CALL(glEnableVertexAttribArray(program.posID));

        GL_CALL(glVertexAttribPointer(program.uvID, 2, GL_FLOAT, GL_FALSE, 0, coordData));
        GL_CALL(glEnableVertexAttribArray(program.uvID));

        wlr_renderer_scissor(core->renderer, NULL);
        for(size_t i = 0; i < streams.size(); i++) {
            int index = (vx + i) % streams.size();

            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

            GL_CALL(glBindTexture(GL_TEXTURE_2D, streams[index]->tex));
            GL_CALL(glActiveTexture(GL_TEXTURE0));

            model = glm::rotate(glm::mat4(1.0),
                float(i) * side_angle + float(duration.progress(animation.rotation)),
                glm::vec3(0, 1, 0));

            model = glm::translate(model, glm::vec3(0, 0, identity_z_offset));
            GL_CALL(glUniformMatrix4fv(program.modelID, 1, GL_FALSE, &model[0][0]));

           if (tessellation_support)
           {
               GL_CALL(glDrawArrays(GL_PATCHES, 0, 6));
           } else
           {
               GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
           }
        }
        glDisable(GL_DEPTH_TEST);

        GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        GL_CALL(glDisableVertexAttribArray(program.posID));
        GL_CALL(glDisableVertexAttribArray(program.uvID));

        bool result = update_animation();
        if (result)
            schedule_next_frame();

        if (animation.in_exit && !result)
            terminate();
    }

    void input_released()
    {
        int size = streams.size();

        float dx = -duration.progress(animation.rotation) / side_angle;
        int dvx = std::floor(dx + 0.5);

        GetTuple(vx, vy, output->workspace->get_current_workspace());
        int nvx = (vx + (dvx % size) + size) % size;
        output->workspace->set_workspace(std::make_tuple(nvx, vy));


        animation.in_exit = true;
        animation.zoom = {duration.progress(animation.zoom), 1.0};
        animation.offset_z = {duration.progress(animation.offset_z), identity_z_offset + Z_OFFSET_NEAR};
        animation.offset_y = {duration.progress(animation.offset_y), 0};
        animation.rotation = {duration.progress(animation.rotation) + 1.0f * dvx * side_angle, 0};
        animation.ease_deformation = {duration.progress(animation.ease_deformation), 0};

        duration.start();

        update_animation();
        schedule_next_frame();
    }

    void terminate()
    {
        animation.active = false;
        animation.in_exit = false;

        output->render->reset_renderer();
        output->deactivate_plugin(grab_interface);

        auto size = streams.size();
        for (uint i = 0; i < size; i++)
            output->render->workspace_stream_stop(streams[i].get());
    }

    void pointer_moved()
    {
        GetTuple(cx, cy, output->get_cursor_position());
        if (animation.in_exit)
            return;

        int xdiff = grab_pos.x - cx;
        int ydiff = grab_pos.y - cy;

        animation.zoom = {duration.progress(animation.zoom), animation.zoom.end};

        float current_off_y = duration.progress(animation.offset_y);
        animation.offset_y = {current_off_y, current_off_y - ydiff * YVelocity->as_cached_double()} ;
        animation.offset_z = {duration.progress(animation.offset_z), animation.offset_z.end};

        float current_rotation = duration.progress(animation.rotation);
        animation.rotation = {current_rotation,
            current_rotation - xdiff * XVelocity->as_cached_double()};

        animation.ease_deformation = {duration.progress(animation.ease_deformation),
            animation.ease_deformation.end};

        duration.start();

        grab_pos = {cx, cy};
        schedule_next_frame();
    }

    void pointer_scrolled(double amount)
    {
        if (animation.in_exit)
            return;

        animation.offset_y = {duration.progress(animation.offset_y), animation.offset_y.end} ;
        animation.offset_z = {duration.progress(animation.offset_z), animation.offset_z.end};
        animation.rotation = {duration.progress(animation.rotation), animation.rotation.end};
        animation.ease_deformation = {duration.progress(animation.ease_deformation), animation.ease_deformation.end};

        float target_zoom = duration.progress(animation.zoom);
        float start_zoom = target_zoom;

        target_zoom +=  std::min(std::pow(target_zoom, 1.5f), ZOOM_MAX) * amount * ZVelocity->as_cached_double();
        target_zoom = std::min(std::max(target_zoom, ZOOM_MIN), ZOOM_MAX);
        animation.zoom = {start_zoom, target_zoom};

        duration.start();

        schedule_next_frame();
    }

    void fini()
    {
        if (output->is_plugin_active(grab_interface->name))
            terminate();

        auto size = streams.size();
        for (uint i = 0; i < size; i++)
        {
            if (streams[i]->fbuff != uint32_t(-1))
            {
                GL_CALL(glDeleteFramebuffers(1, &streams[i]->fbuff));
                GL_CALL(glDeleteTextures(1, &streams[i]->tex));
            }
        }

        output->rem_binding(&activate);
    }
};

extern "C"
{
    wayfire_plugin_t *newInstance()
    {
        return new wayfire_cube();
    }
}
