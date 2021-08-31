#pragma once
#include <cstdint>
#include <vector>
#include <wayfire/transaction/instruction.hpp>
#include <wayfire/geometry.hpp>

namespace wf
{
/**
 * The gravity of the view indicates which corner of the view stays immobile
 * during unexpected resize operations.
 *
 * Here, unexpected resize operations happen when the client commits a buffer
 * with size different from the one requested by the compositor. In these cases,
 * the geometry of the view is recomputed so that the gravity corner remains
 * where the last transaction indicated it should be.
 */
enum class gravity_t : int
{
    TOP_LEFT     = 0,
    TOP_RIGHT    = 1,
    BOTTOM_LEFT  = 2,
    BOTTOM_RIGHT = 3,
};

/**
 * A bitmask consisting of all tiled edges.
 * This corresponds to a maximized state.
 */
constexpr uint32_t TILED_EDGES_ALL =
    WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT;

/**
 * view_state_t describes a state of a view.
 *
 * All state changes are done via transactions.
 */
struct view_state_t
{
    /**
     * Whether the view is mapped or not.
     * A view is mapped when it has a buffer with valid contents to be
     * displayed.
     */
    bool mapped = false;

    /**
     * The dimensions of the view.
     *
     * This does not include any shadows, subsurfaces outside of the main view
     * and transformers.
     */
    wf::geometry_t geometry;

    /**
     * The gravity corner of the view.
     */
    gravity_t gravity = gravity_t::TOP_LEFT;

    /**
     * Tiled edges of the view. The value is a bitmask of
     * WLR_EDGE_{LEFT,RIGHT,TOP,BOTTOM}.
     *
     * A tiled edge indicates that another surface / screen edge / etc. is
     * immediately next to the view on that side. Clients typically do not
     * draw shadows on these edges and may change the visual appearance in
     * other ways.
     *
     * The traditional maximized state is equivalent to the view being tiled
     * on all edges.
     */
    uint32_t tiled_edges = 0;

    /**
     * The fullscreen state of the view.
     */
    bool fullscreen = false;
};

namespace txn
{
class transaction_t;

/**
 * A view_transaction_t is an abstract factory class used to create instructions
 * for concrete implentations of views.
 *
 * Each view implementation typically has its own mechanisms of applying and
 * waiting on instructions. Because of that, instructions cannot be implemented
 * without knowledge of these implementation details. Instead, each view
 * implementation is required to provide a factory which implements the
 * view_transaction_t interface. It can be used to generate and batch together
 * instructions for the particular view implementation.
 *
 * Important: Some view properties (like gravity) may affect other
 * instructions, even from the same transaction. In these cases, they affect
 * ONLY instructions coming after them, be they in the same tx or not.
 */
class view_transaction_t
{
  public:
    /**
     * Request a new geometry for the view.
     * The view (the client) does not need to fulfill the request, but
     * it will typically resize to roughly match the requested dimensions.
     * In addition, fullscreen and tiled clients usually fulfill resize
     * requests.
     *
     * @param new_g The new view geometry.
     * @return Non-owning pointer to self.
     */
    virtual view_transaction_t *set_geometry(const wf::geometry_t& new_g) = 0;

    /**
     * Set a new gravity for the view.
     * @return Non-owning pointer to self.
     */
    virtual view_transaction_t *set_gravity(gravity_t gr) = 0;

    /**
     * Set the desired tiled edges of the view.
     * @return Non-owning pointer to self.
     */
    virtual view_transaction_t *set_tiled(uint32_t edges) = 0;

    /**
     * Set the desired fullscreen state of the view.
     * @return Non-owning pointer to self.
     */
    virtual view_transaction_t *set_fullscreen(bool fullscreen) = 0;

    /**
     * Schedule all batched instructions in the given transaction.
     */
    virtual void schedule_in(
        nonstd::observer_ptr<txn::transaction_t> transaction) = 0;

    /**
     * Convenience function for creating a new transaction, scheduling all
     * instructions there and submitting the transaction to core.
     *
     * @return The new transaction ID.
     */
    virtual uint64_t submit();

    virtual ~view_transaction_t() = default;
};
}
}
