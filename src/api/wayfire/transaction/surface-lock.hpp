#pragma once

#include <wayfire/nonstd/wlroots.hpp>
#include <wayfire/util.hpp>
#include <optional>

namespace wf
{
/**
 * A class for managing wlr_surface locks.
 */
class wlr_surface_manager_t
{
  public:
    wlr_surface_manager_t(wlr_surface *surface);

    /**
     * Create a new lock on the CURRENT surface state.
     * Any older locks are overwritten, but the locked surface state remains
     * the same.
     *
     * Checkpoints for any previous locks are removed.
     *
     * @return The serial ID of the lock.
     */
    uint64_t lock();

    /**
     * Create a checkpoint for the given lock.
     * No-op if the ID is not the last lock.
     */
    void checkpoint(uint64_t id);

    /**
     * Unlock all commits up to the checkpoint for lock @id.
     * If no checkpoint has been reached, the behavior is the same as unlock_all().
     * No-op if the ID is not the last lock.
     *
     * @param id The ID returned by the corresponding lock_until request.
     */
    void unlock(uint64_t id);

    /**
     * Remove all locks on the surface, thus allowing subsequent commits.
     * No-op if the ID is not the last lock.
     *
     * @param id The ID returned by the corresponding lock_until request.
     */
    void unlock_all(uint64_t id);

    /**
     * @return True if there is a lock currently active.
     */
    bool is_locked() const;

  private:
    wlr_surface *surface;
    uint64_t last_id = 0;

    std::optional<uint32_t> current_checkpoint;
    std::optional<uint32_t> next_checkpoint;

  public:
    wlr_surface_manager_t(const wlr_surface_manager_t&) = delete;
    wlr_surface_manager_t& operator =(const wlr_surface_manager_t&) = delete;
    wlr_surface_manager_t(wlr_surface_manager_t&&) = delete;
    wlr_surface_manager_t& operator =(wlr_surface_manager_t&&) = delete;
};
}
