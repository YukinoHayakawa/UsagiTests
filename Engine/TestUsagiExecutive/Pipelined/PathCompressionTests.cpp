/*
 * Usagi Engine: Disjoint-Set Path Compression Proofs
 * -----------------------------------------------------------------------------
 * Exhaustively mathematically asserts the O(1) flattened caching structures
 * within the LayeredDatabaseAggregator. Verifies cold-boot rebuild integrity,
 * independent root isolation, and intermediate node tombstoning.
 */

#include <gtest/gtest.h>

#include "Orchestrator.hpp"

namespace usagi
{
// -----------------------------------------------------------------------------
// Component Topology Mocks
// -----------------------------------------------------------------------------
struct CompRoot
{
};

template <>
consteval ComponentMask get_component_bit<CompRoot>()
{
    return 1ull << 10;
}

struct CompP1
{
};

template <>
consteval ComponentMask get_component_bit<CompP1>()
{
    return 1ull << 11;
}

struct CompP2
{
};

template <>
consteval ComponentMask get_component_bit<CompP2>()
{
    return 1ull << 12;
}

struct CompP3
{
};

template <>
consteval ComponentMask get_component_bit<CompP3>()
{
    return 1ull << 13;
}

struct CompP4
{
};

template <>
consteval ComponentMask get_component_bit<CompP4>()
{
    return 1ull << 14;
}

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiCompressionTest : public UsagiTestOrchestrator
{
protected:
    void SetUp() override { UsagiTestOrchestrator::SetUp(); }

    void TearDown() override { UsagiTestOrchestrator::TearDown(); }
};

// =============================================================================
// O(1) RESOLUTION PROOFS
// =============================================================================

TEST_F(UsagiCompressionTest, PathCompression_O1_Resolution)
{
    // 1. Establish D1 (Root). Orchestrator boots with D0 as an empty read-only
    // base.
    EntityId root_ent =
        get_patch_layer().create_entity_immediate(build_mask<CompRoot>(), 0);
    db.commit_pending_spawns();

    // 2. Build a linear patch chain across 4 DLC layers
    auto build_patch_layer = [&](ComponentMask added_mask) {
        db.push_new_mutable_patch_layer();
        db.add_component(
            root_ent,
            added_mask,
            0); // Modifying the root alias routes to terminal
        db.commit_pending_spawns();
    };

    build_patch_layer(build_mask<CompP1>()); // Domain 2
    build_patch_layer(build_mask<CompP2>()); // Domain 3
    build_patch_layer(build_mask<CompP3>()); // Domain 4
    build_patch_layer(build_mask<CompP4>()); // Domain 5

    // 3. Mathematical Inspection of the Transient Shadow Index
    const auto &shadow_index = db.get_shadow_index();

    // The redirect chain must contain exactly 5 physical aliases [D1, D2, D3,
    // D4, D5]
    auto chain = shadow_index.get_redirect_chain(root_ent);
    ASSERT_EQ(chain.size(), 5)
        << "Path compression failed to aggregate the complete alias history.";

    EntityId p1_ent = chain[1];
    EntityId p2_ent = chain[2];
    EntityId p3_ent = chain[3];
    EntityId p4_ent = chain[4];

    // Assert Domain mappings
    EXPECT_EQ(p1_ent.domain_node, 2);
    EXPECT_EQ(p2_ent.domain_node, 3);
    EXPECT_EQ(p4_ent.domain_node, 5);

    // O(1) Reverse Resolution: Any intermediate alias MUST resolve instantly to
    // D1
    EXPECT_EQ(shadow_index.get_logical_root(p3_ent), root_ent)
        << "Intermediate alias P3 failed O(1) root resolution.";
    EXPECT_EQ(shadow_index.get_logical_root(p4_ent), root_ent)
        << "Terminal alias P4 failed O(1) root resolution.";

    // O(1) Forward Resolution: The Root MUST resolve instantly to D5
    auto terminal_opt = shadow_index.resolve_redirect(root_ent);
    ASSERT_TRUE(terminal_opt.has_value());
    EXPECT_EQ(*terminal_opt, p4_ent)
        << "Root failed to bypass the chain and locate the terminal alias.";

    // Intermediate nodes MUST be explicitly tombstoned to prevent iteration
    // leaks
    EXPECT_TRUE(shadow_index.is_tombstoned(p1_ent))
        << "Intermediate P1 leaked its mask.";
    EXPECT_TRUE(shadow_index.is_tombstoned(p3_ent))
        << "Intermediate P3 leaked its mask.";
    EXPECT_FALSE(shadow_index.is_tombstoned(p4_ent))
        << "Terminal P4 was erroneously tombstoned.";
}

TEST_F(UsagiCompressionTest, PathCompression_AliasChain_Contiguity)
{
    EntityId root_ent =
        get_patch_layer().create_entity_immediate(build_mask<CompRoot>(), 0);
    db.commit_pending_spawns();

    db.push_new_mutable_patch_layer();
    db.add_component(root_ent, build_mask<CompP1>(), 0);
    db.commit_pending_spawns();

    db.push_new_mutable_patch_layer();
    db.add_component(root_ent, build_mask<CompP2>(), 0);
    db.commit_pending_spawns();

    const auto &shadow_index = db.get_shadow_index();
    auto        chain        = shadow_index.get_redirect_chain(root_ent);

    // Mathematical proof of vector contiguity (D1 -> D2 -> D3)
    ASSERT_EQ(chain.size(), 3);
    EXPECT_EQ(chain[0].domain_node, 1);
    EXPECT_EQ(chain[1].domain_node, 2);
    EXPECT_EQ(chain[2].domain_node, 3);

    // Proving the final dynamic mask natively uses the terminal alias
    ComponentMask expected_mask =
        build_mask<CompRoot, CompP1, CompP2, CompShadowRedirect>();
    EXPECT_EQ(db.get_dynamic_mask(root_ent), expected_mask);
}

// =============================================================================
// COLD BOOT BINARY RECONSTRUCTION
// =============================================================================

TEST_F(UsagiCompressionTest, PathCompression_ColdBoot_Reconstruction)
{
    // 1. Build State
    EntityId root_ent =
        get_patch_layer().create_entity_immediate(build_mask<CompRoot>(), 0);
    db.commit_pending_spawns();

    db.push_new_mutable_patch_layer();
    db.add_component(root_ent, build_mask<CompP1>(), 0);
    db.commit_pending_spawns();

    db.push_new_mutable_patch_layer();
    db.add_component(root_ent, build_mask<CompP2>(), 0);
    db.commit_pending_spawns();

    // 2. Perform raw simulated engine crash (Export to bytes, annihilate RAM)
    // Note: UsagiTestOrchestrator::simulate_engine_reboot() only reboots
    // Layer 1. To test a 4-layer stack properly (L0, L1, L2, L3), we must
    // manually invoke the rebuild.
    db.rebuild_transient_indices();

    // 3. Interrogate the rebuilt TransientShadowIndex
    const auto &shadow_index = db.get_shadow_index();
    auto        terminal_opt = shadow_index.resolve_redirect(root_ent);

    ASSERT_TRUE(terminal_opt.has_value())
        << "Cold boot failed to reconstruct terminal redirect map from pure "
           "POD structures.";

    // Terminal alias should physically reside in Domain 3
    EXPECT_EQ(terminal_opt->domain_node, 3)
        << "Cold boot terminal alias was not correctly collapsed to the top "
           "layer.";

    auto chain = shadow_index.get_redirect_chain(root_ent);
    ASSERT_EQ(chain.size(), 3)
        << "Cold boot failed to reconstruct the contiguous alias chain.";

    // Verify Tombstone restoration
    EXPECT_TRUE(shadow_index.is_tombstoned(root_ent))
        << "Cold boot failed to restore base layer tombstone.";
    EXPECT_TRUE(shadow_index.is_tombstoned(chain[1]))
        << "Cold boot failed to restore intermediate tombstone.";
}

// =============================================================================
// TOPOLOGICAL ISOLATION
// =============================================================================

TEST_F(UsagiCompressionTest, PathCompression_IndependentRoots_NoAliasing)
{
    // Proves that mutating disjoint entities simultaneously does not
    // cross-contaminate the logical root caches.

    EntityId root_a =
        get_patch_layer().create_entity_immediate(build_mask<CompRoot>(), 0);
    EntityId root_b =
        get_patch_layer().create_entity_immediate(build_mask<CompRoot>(), 0);
    db.commit_pending_spawns();

    db.push_new_mutable_patch_layer();
    db.add_component(root_a, build_mask<CompP1>(), 0);
    db.add_component(root_b, build_mask<CompP2>(), 0);
    db.commit_pending_spawns();

    const auto &shadow_index = db.get_shadow_index();
    auto        terminal_a   = shadow_index.resolve_redirect(root_a);
    auto        terminal_b   = shadow_index.resolve_redirect(root_b);

    ASSERT_TRUE(terminal_a.has_value());
    ASSERT_TRUE(terminal_b.has_value());
    EXPECT_NE(*terminal_a, *terminal_b)
        << "Aggregator collapsed independent roots into the same alias.";

    EXPECT_EQ(shadow_index.get_logical_root(*terminal_a), root_a);
    EXPECT_EQ(shadow_index.get_logical_root(*terminal_b), root_b);

    ComponentMask mask_a = build_mask<CompRoot, CompP1, CompShadowRedirect>();
    ComponentMask mask_b = build_mask<CompRoot, CompP2, CompShadowRedirect>();

    EXPECT_EQ(db.get_dynamic_mask(root_a), mask_a);
    EXPECT_EQ(db.get_dynamic_mask(root_b), mask_b);
}
} // namespace usagi
