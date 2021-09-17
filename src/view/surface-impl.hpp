#ifndef SURFACE_IMPL_HPP
#define SURFACE_IMPL_HPP

#include <wayfire/opengl.hpp>
#include <wayfire/surface.hpp>
#include <wayfire/util.hpp>
#include <wayfire/region.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>

namespace wf
{
class surface_interface_t::impl
{
  public:
    surface_interface_t *parent_surface;
    std::vector<std::unique_ptr<surface_interface_t>> surface_children_above;
    std::vector<std::unique_ptr<surface_interface_t>> surface_children_below;
    size_t last_cnt_surfaces = 0;

    /**
     * Remove all subsurfaces and emit signals for them.
     */
    void clear_subsurfaces(surface_interface_t *self);

    wf::output_t *output = nullptr;
    static int active_shrink_constraint;

    /**
     * Most surfaces don't have a wlr_surface. However, internal surface
     * implementations can set the underlying surface so that functions like
     *
     * subtract_opaque(), send_frame_done(), etc. work for the surface
     */
    wlr_surface *wsurface = nullptr;
};

/**
 * Data saved about a wlr surface when it is locked because of transactions.
 */
struct surface_state_t
{
    wlr_buffer *buffer = nullptr;
    wf::texture_t texture;

    wf::region_t opaque_region;
    wf::region_t input_region;

    int32_t scale;
    wf::dimensions_t size;

    void copy_from(wlr_surface *surface);
    void clear();

    surface_state_t() = default;
    ~surface_state_t();

    surface_state_t(const surface_state_t& state) = delete;
    surface_state_t(surface_state_t&& state) = delete;
    surface_state_t& operator =(const surface_state_t& state) = delete;
    surface_state_t& operator =(surface_state_t&& state) = delete;
};

/**
 * A base class for views and surfaces which are based on a wlr_surface
 * Any class that derives from wlr_surface_base_t must also derive from
 * surface_interface_t!
 */
class wlr_surface_base_t
{
  protected:
    wf::wl_listener_wrapper::callback_t handle_new_subsurface;
    wf::wl_listener_wrapper on_commit, on_destroy, on_new_subsurface;

    void apply_surface_damage();
    void apply_surface_damage_region(wf::region_t dmg);

    wlr_surface_base_t(wf::surface_interface_t *self);
    /* Pointer to this as surface_interface, see requirement above */
    wf::surface_interface_t *_as_si = nullptr;

  public:
    /* if surface != nullptr, then the surface is mapped */
    wlr_surface *surface = nullptr;

    virtual ~wlr_surface_base_t();

    wlr_surface_base_t(const wlr_surface_base_t &) = delete;
    wlr_surface_base_t(wlr_surface_base_t &&) = delete;
    wlr_surface_base_t& operator =(const wlr_surface_base_t&) = delete;
    wlr_surface_base_t& operator =(wlr_surface_base_t&&) = delete;

    /** @return The offset from the surface coordinates to the actual geometry */
    virtual wf::point_t get_window_offset();

    /** Update the surface output */
    virtual void update_output(wf::output_t *old_output,
        wf::output_t *new_output);

    /*
     * Functions that need to be implemented/overridden from the
     * surface_implementation_t
     */
    bool _is_mapped() const;
    wf::dimensions_t _get_size() const;
    wf::region_t _get_opaque_region(wf::point_t origin);
    void _simple_render(const wf::framebuffer_t& fb, int x, int y,
        const wf::region_t& damage);
    bool _accepts_input(int32_t sx, int32_t sy);

    /**
     * Lock the surface by saving its buffer and size.
     * Any surface commits will not be displayed, until the surface is unlocked
     * again.
     */
    void lock();

    /**
     * Unlock commits on the surface.
     *
     * If the surface has been locked N times, it needs to be unlocked N times
     * before it allows commits again.
     */
    void unlock();

  protected:
    virtual void map(wlr_surface *surface);
    virtual void unmap();
    virtual void commit();

    virtual wlr_buffer *get_buffer();

    surface_state_t wlr_state;
    int32_t lck_count = 0;
};

void for_each_wlr_surface(wf::surface_interface_t *root,
    std::function<void(wf::wlr_surface_base_t*)> cb);

/**
 * wlr_child_surface_base_t is a base class for wlr-surface based child
 * surfaces, i.e subsurfaces.
 *
 * However, they can still exist without a parent, for ex. drag icons.
 */
class wlr_child_surface_base_t :
    public surface_interface_t, public wlr_surface_base_t
{
  public:
    wlr_child_surface_base_t(surface_interface_t *self);
    virtual ~wlr_child_surface_base_t();

    wlr_child_surface_base_t(const wlr_child_surface_base_t &) = delete;
    wlr_child_surface_base_t(wlr_child_surface_base_t &&) = delete;
    wlr_child_surface_base_t& operator =(const wlr_child_surface_base_t&) = delete;
    wlr_child_surface_base_t& operator =(wlr_child_surface_base_t&&) = delete;

    /* Just pass to the default wlr surface implementation */
    virtual bool is_mapped() const override
    {
        return _is_mapped();
    }

    virtual wf::dimensions_t get_size() const override
    {
        return _get_size();
    }

    virtual wf::region_t get_opaque_region(wf::point_t origin) override
    {
        return _get_opaque_region(origin);
    }

    virtual bool accepts_input(int32_t sx, int32_t sy) override
    {
        return _accepts_input(sx, sy);
    }

    virtual void simple_render(const wf::framebuffer_t& fb, int x, int y,
        const wf::region_t& damage) override
    {
        _simple_render(fb, x, y, damage);
    }

    virtual void set_output(wf::output_t *output) override
    {
        update_output(get_output(), output);
        surface_interface_t::set_output(output);
    }
};
}

#endif /* end of include guard: SURFACE_IMPL_HPP */
