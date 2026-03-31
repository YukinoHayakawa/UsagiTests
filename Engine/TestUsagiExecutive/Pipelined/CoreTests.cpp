/*
 * Usagi Engine: Entity Database & Virtualization Core Proofs
 * -----------------------------------------------------------------------------
 * Exhaustively interrogates the mathematical guarantees of the spatial matrix,
 * deferred structural queues, and the N-Layer virtual page table overlays.
 */

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "EntityDatabase.hpp"

namespace usagi
{
// -----------------------------------------------------------------------------
// Component Topology Mocks
// -----------------------------------------------------------------------------
struct CoreCompA
{
};

template <>
consteval ComponentMask get_component_bit<CoreCompA>()
{
    return 1ull << 10;
}

struct CoreCompB
{
};

template <>
consteval ComponentMask get_component_bit<CoreCompB>()
{
    return 1ull << 11;
}

struct CoreCompC
{
};

template <>
consteval ComponentMask get_component_bit<CoreCompC>()
{
    return 1ull << 12;
}

struct CoreEdge
{
};

template <>
consteval ComponentMask get_component_bit<CoreEdge>()
{
    return 1ull << 13;
}

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiDatabaseCoreTest : public ::testing::Test
{
protected:
    LayeredDatabaseAggregator db;

    void SetUp() override
    {
        // The Aggregator natively boots with Layer 0 (Mutable).
        // Transient indices and payload maps are empty.
    }

    void TearDown() override
    {
        // Ensure no memory leaks or dangling atomic states across tests
        db.get_mutable_layer().clear_database();
    }
};

// =============================================================================
// NORMAL OPERATIONS (SINGLE LAYER)
// =============================================================================

TEST_F(UsagiDatabaseCoreTest, Normal_SpawnAndQuery_DeferredCommit)
{
    // Queue a spawn on partition 0
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);

    EXPECT_TRUE(db.has_pending_spawns())
        << "Aggregator failed to register the pending structural mutation.";

    // Query BEFORE commit MUST return mathematically empty
    auto pre_commit = db.query_entities(build_mask<CoreCompA>());
    EXPECT_TRUE(pre_commit.empty()) << "Deferred queue leaked entities into "
                                       "the sparse matrix before commit.";

    db.commit_pending_spawns();
    EXPECT_FALSE(db.has_pending_spawns())
        << "Commit failed to reset atomic semaphores.";

    // Query AFTER commit MUST resolve exactly 1 entity
    auto post_commit = db.query_entities(build_mask<CoreCompA>());
    ASSERT_EQ(post_commit.size(), 1);

    EntityId e = post_commit[0];
    EXPECT_EQ(e.domain_node, 0);
    EXPECT_EQ(e.partition, 0);
    EXPECT_EQ(e.page, 0);
    EXPECT_EQ(e.slot, 0);
}

TEST_F(UsagiDatabaseCoreTest, Normal_ComponentMutation_ActiveLayerImmediate)
{
    // Mutations on the active (top) layer are mathematically immediate.
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);
    db.commit_pending_spawns();

    EntityId e = db.query_entities(build_mask<CoreCompA>())[0];

    db.add_component(e, build_mask<CoreCompB>(), 0);

    // Assert immediate bitwise mutation
    ComponentMask mask = db.get_dynamic_mask(e);
    EXPECT_EQ(mask, (build_mask<CoreCompA, CoreCompB>()))
        << "Active layer mutation failed to bypass shadow queues.";

    db.remove_component(e, build_mask<CoreCompA>(), 0);
    mask = db.get_dynamic_mask(e);
    EXPECT_EQ(mask, build_mask<CoreCompB>())
        << "Active layer removal failed to execute immediate bitwise AND-NOT.";
}

TEST_F(UsagiDatabaseCoreTest, Normal_BipartiteEdge_RegistrationAndQuery)
{
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompB>(), 0);
    db.commit_pending_spawns();

    auto     a_ents = db.query_entities(build_mask<CoreCompA>());
    auto     b_ents = db.query_entities(build_mask<CoreCompB>());
    EntityId src    = a_ents[0];
    EntityId tgt    = b_ents[0];

    db.get_mutable_layer().queue_edge_registration(
        src, tgt, build_mask<CoreEdge>());
    db.commit_pending_spawns();

    auto edges = db.get_outbound_edges(src);
    ASSERT_EQ(edges.size(), 1)
        << "TransientEdgeIndex failed to map the bipartite relation.";

    EntityId edge_id = edges[0];
    EXPECT_EQ(db.get_dynamic_mask(edge_id), build_mask<CoreEdge>())
        << "Edge entity lost physical metadata mapping.";
}

// =============================================================================
// EDGE CASES (SINGLE LAYER)
// =============================================================================

TEST_F(UsagiDatabaseCoreTest, Edge_PartitionIsolation_CrossPartitionSpawns)
{
    // Spam entities across the absolute bounds of the partition space
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 128);
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 255);
    db.commit_pending_spawns();

    auto ents = db.query_entities(build_mask<CoreCompA>());
    ASSERT_EQ(ents.size(), 3);

    // Sort mathematically by partition for strict assertions
    std::sort(ents.begin(), ents.end(), [](EntityId a, EntityId b) {
        return a.partition < b.partition;
    });

    EXPECT_EQ(ents[0].partition, 0);
    EXPECT_EQ(ents[1].partition, 128);
    EXPECT_EQ(ents[2].partition, 255);
}

TEST_F(UsagiDatabaseCoreTest, Edge_RedundantMutations_NoCorruption)
{
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);
    db.commit_pending_spawns();
    EntityId e = db.query_entities(build_mask<CoreCompA>())[0];

    // Double Add
    db.add_component(e, build_mask<CoreCompA>(), 0);
    EXPECT_EQ(db.get_dynamic_mask(e), build_mask<CoreCompA>());

    // Remove non-existent
    db.remove_component(e, build_mask<CoreCompB>(), 0);
    EXPECT_EQ(db.get_dynamic_mask(e), build_mask<CoreCompA>());
}

TEST_F(UsagiDatabaseCoreTest, Edge_InvalidEntityDestruction_NoSegfault)
{
    // Ensure attempting to destroy garbage coordinates fails silently without
    // corrupting state
    EXPECT_NO_THROW({ db.destroy_entity(INVALID_ENTITY); });

    EntityId out_of_bounds_page { 0, 0, 9'999, 0, 0 };
    EXPECT_NO_THROW({ db.destroy_entity(out_of_bounds_page); });
}

// =============================================================================
// VIRTUALIZATION NORMAL OPERATIONS (MULTI-LAYER)
// =============================================================================

TEST_F(UsagiDatabaseCoreTest, Virtualization_PatchMutation_GeneratesRedirect)
{
    // 1. Establish D0
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);
    db.commit_pending_spawns();
    EntityId base_e = db.query_entities(build_mask<CoreCompA>())[0];

    // 2. Mount D1
    db.push_new_mutable_patch_layer();

    // 3. Mutate D0 entity from D1
    db.add_component(base_e, build_mask<CoreCompB>(), 0);
    EXPECT_TRUE(db.has_pending_spawns())
        << "Aggregator failed to queue shadow mutation.";

    db.commit_pending_spawns();

    // 4. Assert Redirect Topologies
    auto shadow_index = db.get_shadow_index();
    auto redirect     = shadow_index.resolve_redirect(base_e);
    ASSERT_TRUE(redirect.has_value());

    EntityId patch_e = *redirect;
    EXPECT_EQ(patch_e.domain_node, 1);

    // The mask must contain A (base), B (patch), and the Redirect brand
    EXPECT_EQ(
        db.get_dynamic_mask(base_e),
        (build_mask<CoreCompA, CoreCompB, CompShadowRedirect>()));
}

TEST_F(UsagiDatabaseCoreTest, Virtualization_Tombstone_ErasesFromQuery)
{
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);
    db.commit_pending_spawns();
    EntityId base_e = db.query_entities(build_mask<CoreCompA>())[0];

    db.push_new_mutable_patch_layer();

    db.destroy_entity(base_e);
    db.commit_pending_spawns();

    // The active iteration lanes must skip the tombstoned bit
    auto ents = db.query_entities(build_mask<CoreCompA>());
    EXPECT_TRUE(ents.empty())
        << "Base entity leaked through the Virtual Page Table overlay.";

    // Direct mask fetch should yield 0 for a fully tombstoned entity
    EXPECT_EQ(db.get_dynamic_mask(base_e), 0);
}

// =============================================================================
// VIRTUALIZATION EDGE CASES (MULTI-LAYER)
// =============================================================================

TEST_F(
    UsagiDatabaseCoreTest, VirtualizationEdge_MultiHopMutation_TerminalCollapse)
{
    // Prove that applying sequential mutations across deeply stacked DLCs
    // correctly collapses into the terminal alias without corrupting the
    // historical chain.
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);
    db.commit_pending_spawns();
    EntityId base_e = db.query_entities(build_mask<CoreCompA>())[0];

    db.push_new_mutable_patch_layer(); // D1
    db.add_component(base_e, build_mask<CoreCompB>(), 0);
    db.commit_pending_spawns();

    db.push_new_mutable_patch_layer(); // D2
    db.add_component(base_e, build_mask<CoreCompC>(), 0);
    db.commit_pending_spawns();

    // Query should fetch the exact terminal state: A | B | C | RedirectBrand
    ComponentMask final_mask = db.get_dynamic_mask(base_e);
    EXPECT_EQ(
        final_mask,
        (build_mask<CoreCompA, CoreCompB, CoreCompC, CompShadowRedirect>()))
        << "Multi-hop terminal resolution corrupted the physical component "
           "mask.";
}

TEST_F(
    UsagiDatabaseCoreTest,
    VirtualizationEdge_EdgeTombstoning_RejectsCrossLayerLeaks)
{
    // Prove that deleting an edge in D1 permanently hides it from the D0
    // target.
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompA>(), 0);
    db.get_mutable_layer().queue_spawn(build_mask<CoreCompB>(), 0);
    db.commit_pending_spawns();

    EntityId src = db.query_entities(build_mask<CoreCompA>())[0];
    EntityId tgt = db.query_entities(build_mask<CoreCompB>())[0];

    db.get_mutable_layer().queue_edge_registration(
        src, tgt, build_mask<CoreEdge>());
    db.commit_pending_spawns();
    EntityId edge = db.get_outbound_edges(src)[0];

    db.push_new_mutable_patch_layer(); // D1

    // Annihilate the edge in the patch layer
    db.destroy_entity(edge);
    db.commit_pending_spawns();

    auto out_edges = db.get_outbound_edges(src);
    EXPECT_TRUE(out_edges.empty())
        << "Tombstoned edge resurrected across the virtualization boundary.";
}
} // namespace usagi
