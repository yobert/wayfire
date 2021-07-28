#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

#include "../stub.h"
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/transaction/surface-lock.hpp>


std::map<wlr_surface*, int> nr_locks;
static uint32_t mock_lock_pending(wlr_surface *surface)
{
    ++nr_locks[surface];
    return nr_locks[surface];
}

std::map<wlr_surface*, std::set<uint32_t>> unlocked;
static void mock_unlock_pending(wlr_surface *surface, uint32_t id)
{
    unlocked[surface].insert(id);
}

TEST_CASE("wlr_surface_manager_t")
{
    wlr_surface surface;
    nr_locks.clear();
    unlocked.clear();

    STUB(wlr_surface_lock_pending, mock_lock_pending);
    STUB(wlr_surface_unlock_cached, mock_unlock_pending);

    wf::wlr_surface_manager_t lockmgr(&surface);

    SUBCASE("No locks - commit passes through")
    {
        REQUIRE(nr_locks[&surface] == 0);
        REQUIRE(lockmgr.is_locked() == false);
    }

    SUBCASE("Create a checkpoint")
    {
        int lock_id = lockmgr.lock();
        REQUIRE(nr_locks[&surface] == 1);
        REQUIRE(lockmgr.is_locked() == true);

        SUBCASE("Hop checkpoints")
        {
            lockmgr.checkpoint(lock_id);
            REQUIRE(nr_locks[&surface] == 2);
            REQUIRE(unlocked[&surface].empty());

            lockmgr.checkpoint(lock_id);
            REQUIRE(nr_locks[&surface] == 3);
            REQUIRE(unlocked[&surface] == std::set<uint32_t>{2});

            lockmgr.checkpoint(lock_id);
            REQUIRE(nr_locks[&surface] == 4);
            REQUIRE(unlocked[&surface] == std::set<uint32_t>{2, 3});
            REQUIRE(lockmgr.is_locked() == true);
        }

        SUBCASE("Reach checkpoint")
        {
            lockmgr.checkpoint(lock_id);
            REQUIRE(nr_locks[&surface] == 2);
            REQUIRE(unlocked[&surface].empty());

            SUBCASE("Unlock commits up to checkpoint")
            {
                lockmgr.unlock(lock_id);
                REQUIRE(unlocked[&surface] == std::set<uint32_t>{1});

                SUBCASE("Unlock all commits")
                {
                    lockmgr.unlock_all(lock_id);
                    REQUIRE(unlocked[&surface] == std::set<uint32_t>{1, 2});
                }

                SUBCASE("Transfer lock")
                {
                    lockmgr.lock(); // new lock holds state 2
                    lockmgr.unlock_all(lock_id);
                    REQUIRE(unlocked[&surface] == std::set<uint32_t>{1});
                }
            }

            SUBCASE("Stay at first checkpoint")
            {
                lockmgr.lock(); // First lock not released -> remove checkpoint
                REQUIRE(unlocked[&surface] == std::set<uint32_t>{2});

                // Unlock the first lock.
                // Only the second checkpoint should be removed!
                lockmgr.unlock(lock_id);
                lockmgr.unlock_all(lock_id);
                REQUIRE(unlocked[&surface] == std::set<uint32_t>{2});
                REQUIRE(lockmgr.is_locked() == true);
            }
        }

        SUBCASE("Stay at first checkpoint")
        {
            lockmgr.lock();
            REQUIRE(unlocked[&surface].empty());
            REQUIRE(lockmgr.is_locked() == true);

            // Unlock the first lock.
            // Only the second checkpoint should be removed!
            lockmgr.unlock(lock_id);
            lockmgr.unlock_all(lock_id);
            REQUIRE(unlocked[&surface].empty());
            REQUIRE(lockmgr.is_locked() == true);
        }
    }
}
