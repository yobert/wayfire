#pragma once

#include "../view-impl.hpp"
#include <wayfire/transaction/surface-lock.hpp>
#include "../instruction-impl.hpp"

#define KILL_TX "__kill-tx"

/**
 * A class for xdg-shell popups
 */
class wayfire_xdg_popup : public wf::wlr_view_t
{
  protected:
    wf::wl_listener_wrapper on_destroy, on_new_popup,
        on_map, on_unmap, on_ping_timeout;
    wf::signal_connection_t parent_geometry_changed,
        parent_title_changed, parent_app_id_changed;

    wf::wl_idle_call pending_close;
    wlr_xdg_popup *popup;
    void unconstrain();
    void update_position();

  public:
    wayfire_xdg_popup(wlr_xdg_popup *popup);
    void initialize() override;

    std::unique_ptr<wf::txn::view_transaction_t> next_state() override
    {
        assert(false);
    }

    wlr_view_t *popup_parent;
    virtual void map(wlr_surface *surface) override;
    virtual void commit() override;

    virtual wf::point_t get_window_offset() override;
    virtual void destroy() override;
    virtual void close() override;
    void ping() final;
};

void create_xdg_popup(wlr_xdg_popup *popup);

class wayfire_xdg_view : public wf::wlr_view_t
{
  private:
    wf::wl_listener_wrapper on_map, on_unmap, on_destroy, on_new_popup,
        on_request_move, on_request_resize,
        on_request_minimize, on_request_maximize,
        on_request_fullscreen, on_set_parent,
        on_set_title, on_set_app_id, on_show_window_menu,
        on_ping_timeout;

    wf::point_t xdg_surface_offset = {0, 0};
    uint32_t last_configure_serial = 0;

    wf::wl_listener_wrapper on_precommit;

  protected:
    void initialize() override final;

    /**
     * Check the current pending state of the view.
     * If the surface window geometry or size changed, start a new transaction
     * in order to update.
     *
     * Otherwise, let the commit through so that content of the view is updated.
     */
    void handle_precommit();

  public:
    std::unique_ptr<wf::wlr_surface_manager_t> lockmgr;
    wlr_xdg_toplevel *xdg_toplevel;

    wayfire_xdg_view(wlr_xdg_toplevel *toplevel);
    virtual ~wayfire_xdg_view();

    wlr_surface *get_wlr_surface() final;

    bool is_mapped() const final;
    void map(wlr_surface *surface) final;
    void commit() final;

    wf::point_t get_window_offset() final;
    wf::geometry_t get_wm_geometry() final;

    void set_activated(bool act) final;
    void set_fullscreen(bool full) final;

    void move(int x, int y) final;
    void resize(int w, int h) final;
    void set_geometry(wf::geometry_t) final;

    std::unique_ptr<wf::txn::view_transaction_t> next_state() override;

    void request_native_size() override final;

    void destroy() final;
    void close() final;
    void ping() final;
};
