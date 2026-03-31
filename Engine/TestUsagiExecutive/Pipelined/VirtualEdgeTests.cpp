/*
 * Usagi Engine: Virtualization Shadow-Edge Collapse & Deep Scale Proofs
 * -----------------------------------------------------------------------------
 * Interrogates the mathematical union of Bipartite Graph Adjacency Lists across
 * N-Layer shadow redirects. Evaluates deep recursion bounds, ghost edge
 * resurrections, and aggregated scattering. Explicitly verifies that patching
 * an entity does not drop its relations, and that edge tombstones are properly
 * respected.
 */

#include "Orchestrator.hpp"

namespace usagi
{
// -----------------------------------------------------------------------------
// Test Component Topology
// -----------------------------------------------------------------------------
struct CompAgent
{
};

template <>
consteval ComponentMask get_component_bit<CompAgent>()
{
    return 1ull << 10;
}

struct CompTarget
{
};

template <>
consteval ComponentMask get_component_bit<CompTarget>()
{
    return 1ull << 11;
}

struct CompPoisoned
{
};

template <>
consteval ComponentMask get_component_bit<CompPoisoned>()
{
    return 1ull << 12;
}

struct EdgeTracking
{
};

template <>
consteval ComponentMask get_component_bit<EdgeTracking>()
{
    return 1ull << 13;
}

// -----------------------------------------------------------------------------
// Mathematical Proofs
// -----------------------------------------------------------------------------

class UsagiVirtualizationEdgeTest : public UsagiTestOrchestrator
{
protected:
    void SetUp() override { UsagiTestOrchestrator::SetUp(); }

    void TearDown() override { UsagiTestOrchestrator::TearDown(); }
};

TEST_F(UsagiVirtualizationEdgeTest, Virtualization_ShadowEdgePreservation)
{
    // 1. Build Base Layer (Domain 0) Topology
    LayeredDatabaseAggregator local_db;
    auto                      temp_base = std::make_unique<EntityDatabase>();

    EntityId source_ent =
        temp_base->create_entity_immediate(build_mask<CompAgent>(), 0);
    EntityId target_ent =
        temp_base->create_entity_immediate(build_mask<CompTarget>(), 0);

    // Register Edge in Base Layer
    temp_base->queue_edge_registration(
        source_ent, target_ent, build_mask<EdgeTracking>());
    temp_base->commit_pending_spawns();

    // Extract the physical Edge ID for tracking
    auto base_edges = temp_base->transient_edges.get_outbound_edges(source_ent);
    ASSERT_EQ(base_edges.size(), 1);
    EntityId base_edge_id = base_edges[0];

    // Mount Domain 0, establishing Mutable Domain 1 (Patch)
    local_db.mount_readonly_layer(std::move(temp_base));

    // 2. Mutate the Source Entity (Triggering the Redirect)
    local_db.add_component(source_ent, build_mask<CompPoisoned>(), 0);
    local_db.commit_pending_spawns();

    // 3. Mathematical Interrogation of the Identity Chain
    // Querying the outbound edges of the source MUST yield the original base
    // edge, despite the source's physical memory now routing to the patch
    // layer.
    auto aggregated_edges = local_db.get_outbound_edges(source_ent);

    ASSERT_EQ(aggregated_edges.size(), 1)
        << "Shadow-Edge Collapse occurred! The patch redirect severed the "
           "relational topology.";

    EXPECT_EQ(aggregated_edges[0], base_edge_id)
        << "The Aggregator returned a corrupted edge identity.";
}

TEST_F(UsagiVirtualizationEdgeTest, Virtualization_EdgeTombstoneFiltering)
{
    // 1. Build Base Layer Topology
    LayeredDatabaseAggregator local_db;
    auto                      temp_base = std::make_unique<EntityDatabase>();

    EntityId source_ent =
        temp_base->create_entity_immediate(build_mask<CompAgent>(), 0);
    EntityId target_ent =
        temp_base->create_entity_immediate(build_mask<CompTarget>(), 0);

    temp_base->queue_edge_registration(
        source_ent, target_ent, build_mask<EdgeTracking>());
    temp_base->commit_pending_spawns();

    EntityId base_edge_id =
        temp_base->transient_edges.get_outbound_edges(source_ent)[0];

    local_db.mount_readonly_layer(std::move(temp_base));

    // 2. Delete the Base Edge from the Patch Layer
    // This physically drops a Tombstone in Domain 1 targeting the Domain 0
    // edge.
    local_db.destroy_entity(base_edge_id);
    local_db.commit_pending_spawns();

    // 3. Mathematical Interrogation
    auto aggregated_edges = local_db.get_outbound_edges(source_ent);

    EXPECT_TRUE(aggregated_edges.empty())
        << "Tombstone Filtering Failure! A deleted base edge leaked through "
           "the virtualization layer.";
}

TEST_F(UsagiVirtualizationEdgeTest, Virtualization_MultiHopAliasResolution)
{
    // 1. Base Layer (Domain 0)
    LayeredDatabaseAggregator local_db;
    auto                      temp_base = std::make_unique<EntityDatabase>();
    EntityId                  source_ent =
        temp_base->create_entity_immediate(build_mask<CompAgent>(), 0);
    local_db.mount_readonly_layer(
        std::move(temp_base)); // Base is now D0, Mutable is D1

    // 2. Patch Layer 1 (Domain 1)
    local_db.add_component(source_ent, build_mask<CompTarget>(), 0);
    local_db.commit_pending_spawns();

    // Freeze D1, Mount Patch Layer 2 (Domain 2)
    local_db.push_new_mutable_patch_layer();

    // Mutate in Patch Layer 2 (Domain 2)
    // 4. Mutate in Patch Layer 2 (Domain 2)
    local_db.add_component(source_ent, build_mask<CompPoisoned>(), 0);
    local_db.commit_pending_spawns();

    // 5. Mathematical Interrogation
    // The query MUST resolve the full Base -> D1 -> D2 stack.
    ComponentMask final_mask = local_db.get_dynamic_mask(source_ent);

    // Yukino: The terminal entity in D2 possesses all applied components, plus
    // the Redirect serialization brand.
    ComponentMask expected_mask =
        build_mask<CompAgent, CompTarget, CompPoisoned, CompShadowRedirect>();

    EXPECT_EQ(final_mask, expected_mask)
        << "Multi-Hop Alias Resolution Failed. The Aggregator overwrote the D1 "
           "redirect and lost the chain.";
}

// --- PATHOLOGICAL SCALING PROOFS ---

TEST_F(UsagiVirtualizationEdgeTest, Pathological_DeepRedirectChainThrashing)
{
    LayeredDatabaseAggregator local_db;
    auto                      temp_base = std::make_unique<EntityDatabase>();
    EntityId                  ent =
        temp_base->create_entity_immediate(build_mask<CompAgent>(), 0);
    local_db.mount_readonly_layer(std::move(temp_base));

    // Thrash the entity across 100 mounted patch layers.
    // If the resolution algorithm is exponential or recursive without tail-call
    // optimization, this will blow the call stack or time out.
    constexpr int CHAIN_DEPTH = 100;
    for(int i = 0; i < CHAIN_DEPTH; ++i)
    {
        local_db.add_component(
            ent, build_mask<CompTarget>(), 0); // Mutate to force redirect
        local_db.commit_pending_spawns();
        local_db.push_new_mutable_patch_layer();
    }

    // Mathematical resolution across 100 DLC boundaries.
    ComponentMask final_mask = local_db.get_dynamic_mask(ent);
    EXPECT_EQ(
        final_mask, (build_mask<CompAgent, CompTarget, CompShadowRedirect>()))
        << "Deep chain alias resolution corrupted the physical component mask.";
}

TEST_F(UsagiVirtualizationEdgeTest, Edge_GhostResurrectionProof)
{
    LayeredDatabaseAggregator local_db;
    auto                      temp_base = std::make_unique<EntityDatabase>();
    EntityId                  src =
        temp_base->create_entity_immediate(build_mask<CompAgent>(), 0);
    EntityId tgt =
        temp_base->create_entity_immediate(build_mask<CompTarget>(), 0);

    temp_base->queue_edge_registration(src, tgt, build_mask<EdgeTracking>());
    temp_base->commit_pending_spawns();
    EntityId base_edge_id =
        temp_base->transient_edges.get_outbound_edges(src)[0];

    local_db.mount_readonly_layer(std::move(temp_base));

    // L1: Tombstone the edge
    local_db.destroy_entity(base_edge_id);
    local_db.commit_pending_spawns();
    local_db.push_new_mutable_patch_layer();

    // L2: Resurrect the edge (spawns a mathematically new physical entity, but
    // logically identical)
    local_db.get_mutable_layer().queue_edge_registration(
        src, tgt, build_mask<EdgeTracking>());
    local_db.commit_pending_spawns();

    auto aggregated = local_db.get_outbound_edges(src);

    // It must return EXACTLY ONE edge. Not zero (because we resurrected it),
    // and not two (because the base edge must remain mathematically
    // tombstoned).
    ASSERT_EQ(aggregated.size(), 1)
        << "Ghost Resurrection failed. Tombstone filter incorrectly merged or "
           "deleted active boundaries.";
    EXPECT_NE(aggregated[0], base_edge_id)
        << "Tombstone filter erroneously returned the dead base edge.";
}

TEST_F(UsagiVirtualizationEdgeTest, Edge_MultiTargetScatterAggregation)
{
    LayeredDatabaseAggregator local_db;
    auto                      temp_base = std::make_unique<EntityDatabase>();
    EntityId                  src =
        temp_base->create_entity_immediate(build_mask<CompAgent>(), 0);
    local_db.mount_readonly_layer(std::move(temp_base));

    // Scatter 50 targets across 10 layers.
    for(int layer = 0; layer < 10; ++layer)
    {
        for(int i = 0; i < 5; ++i)
        {
            EntityId tgt = local_db.get_mutable_layer().create_entity_immediate(
                build_mask<CompTarget>(), 0);
            local_db.get_mutable_layer().queue_edge_registration(
                src, tgt, build_mask<EdgeTracking>());
        }
        local_db.commit_pending_spawns();
        if(layer < 9) local_db.push_new_mutable_patch_layer();
    }

    auto aggregated = local_db.get_outbound_edges(src);
    EXPECT_EQ(aggregated.size(), 50) << "Aggregator dropped fractured edges "
                                        "across multi-layer DLC boundaries.";
}
} // namespace usagi
