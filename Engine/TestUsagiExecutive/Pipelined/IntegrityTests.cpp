/*
 * Usagi Engine: Entity Database & Virtualization Integrity Proofs
 * -----------------------------------------------------------------------------
 * Interrogates the pointer-free VirtualMemoryArena, BMI1 iteration logic,
 * Bipartite Edge obliteration, and Layered Shadow Redirects.
 */

#include <unordered_map>

#include "Orchestrator.hpp"

namespace usagi
{
// -----------------------------------------------------------------------------
// Test Component Topology
// -----------------------------------------------------------------------------
struct CompDataA
{
    float val;
};

template <>
consteval ComponentMask get_component_bit<CompDataA>()
{
    return 1ull << 10;
}

struct CompDataB
{
    int state;
};

template <>
consteval ComponentMask get_component_bit<CompDataB>()
{
    return 1ull << 11;
}

struct EdgeDataLink
{
};

template <>
consteval ComponentMask get_component_bit<EdgeDataLink>()
{
    return 1ull << 12;
}

// Raw Data Mocks for Probing (Since HiveMetadataPage is pure POD masks)
std::unordered_map<EntityId, CompDataA> mock_data_a;
std::unordered_map<EntityId, CompDataB> mock_data_b;

void clear_integrity_mock_data()
{
    mock_data_a.clear();
    mock_data_b.clear();
}

// -----------------------------------------------------------------------------
// Mathematical Proofs
// -----------------------------------------------------------------------------

class UsagiDatabaseIntegrityTest : public UsagiTestOrchestrator
{
protected:
    void SetUp() override
    {
        UsagiTestOrchestrator::SetUp();
        clear_integrity_mock_data();
    }
};

TEST_F(UsagiDatabaseIntegrityTest, Storage_128Bit_SingularityBoundary)
{
    constexpr uint32_t TEST_PARTITION = 0;
    constexpr uint32_t ENTITY_COUNT   = HIVE_PAGE_SIZE + 1; // 65 entities

    std::vector<EntityId> ids;
    for(uint32_t i = 0; i < ENTITY_COUNT; ++i)
    {
        // Shio: Physical memory allocation requests MUST be routed to the
        // mutable patch layer.
        get_patch_layer().queue_spawn(build_mask<CompDataA>(), TEST_PARTITION);
    }
    db.commit_pending_spawns();

    auto results = db.query_entities(build_mask<CompDataA>());
    ASSERT_EQ(results.size(), ENTITY_COUNT);

    // Entity 64 (Index 63) must reside at Page 0, Slot 63
    EXPECT_EQ(results[63].page, 0);
    EXPECT_EQ(results[63].slot, 63);

    // Entity 65 (Index 64) must cleanly wrap to Page 1, Slot 0 without
    // corrupting
    EXPECT_EQ(results[64].page, 1);
    EXPECT_EQ(results[64].slot, 0);

    // Fetch the raw active_lanes mask from the underlying Patch Layer
    ComponentMask mask = db.get_dynamic_mask(results[0]);
    EXPECT_EQ(mask, build_mask<CompDataA>());
}

TEST_F(UsagiDatabaseIntegrityTest, Storage_BMI1_JumpIterationIntegrity)
{
    constexpr uint32_t TEST_PARTITION = 0;

    for(uint32_t i = 0; i < HIVE_PAGE_SIZE; ++i)
    {
        get_patch_layer().queue_spawn(build_mask<CompDataB>(), TEST_PARTITION);
    }
    db.commit_pending_spawns();

    auto ents = db.query_entities(build_mask<CompDataB>());
    ASSERT_EQ(ents.size(), HIVE_PAGE_SIZE);

    // Annihilate slots 1 through 62.
    for(uint32_t i = 1; i < HIVE_PAGE_SIZE - 1; ++i)
    {
        db.destroy_entity(ents[i]);
    }

    // The query MUST mathematically skip the 62 dead lanes via countr_zero.
    auto remaining = db.query_entities(build_mask<CompDataB>());
    ASSERT_EQ(remaining.size(), 2);

    // Validate the surviving coordinates
    EXPECT_EQ(remaining[0].slot, 0);
    EXPECT_EQ(remaining[1].slot, 63);
}

TEST_F(UsagiDatabaseIntegrityTest, Virtualization_CrossLayerRedirectProbing)
{
    // 1. Setup Base Layer (Domain 0) Entity
    // Yukino: We cannot copy/move assign std::atomic structures. To set up the
    // virtualization test cleanly without corrupting the GTest fixture state,
    // we build an isolated LayeredDatabaseAggregator locally.
    LayeredDatabaseAggregator local_db;

    // We heap allocate the future Base Layer directly, so we can std::move the
    // unique_ptr.
    auto     temp_base = std::make_unique<EntityDatabase>();
    EntityId base_ent =
        temp_base->create_entity_immediate(build_mask<CompDataA>(), 0);
    mock_data_a[base_ent] = { 42.0f };

    // Inject the populated database underneath the current mutable layer.
    // It is mathematically frozen as Domain 0.
    local_db.mount_readonly_layer(std::move(temp_base));

    // 2. Mutate the Base Entity from the Patch Layer (Domain 1)
    local_db.add_component(base_ent, build_mask<CompDataB>(), 0);
    local_db.commit_pending_spawns();

    // 3. Mathematical Interrogation
    auto shadow_index   = local_db.get_shadow_index();
    auto redirected_opt = shadow_index.resolve_redirect(base_ent);
    ASSERT_TRUE(redirected_opt.has_value())
        << "Aggregator failed to generate a physical redirect coordinate.";

    EntityId patch_ent = redirected_opt.value();
    EXPECT_EQ(patch_ent.domain_node, 1)
        << "Redirected entity was not allocated in the Mutable Patch Layer.";

    // 4. Probing the Illusion
    // A system querying the dynamic mask of the OLD base pointer must receive
    // the NEW mutated mask. Yukino: The patch entity physically possesses the
    // CompShadowRedirect bit to survive binary serialization.
    ComponentMask dynamic_mask = local_db.get_dynamic_mask(base_ent);
    ComponentMask expected_mask =
        build_mask<CompDataA, CompDataB, CompShadowRedirect>(); // Bypass GTest
                                                                // macro comma
                                                                // expansion
                                                                // failures

    EXPECT_EQ(dynamic_mask, expected_mask)
        << "Virtual Page Table failed to route the mask query to the shadow "
           "entity.";

    // Probing the active iteration lanes. The old entity must be mathematically
    // hidden.
    auto all_a = local_db.query_entities(build_mask<CompDataA>());
    ASSERT_EQ(all_a.size(), 1);
    EXPECT_EQ(all_a[0], patch_ent) << "Tombstone mask failed to hide the base "
                                      "entity from global iteration.";
}

TEST_F(UsagiDatabaseIntegrityTest, Bipartite_EdgeReificationObliteration)
{
    // 1. Setup Source and Target
    EntityId source =
        get_patch_layer().create_entity_immediate(build_mask<CompDataA>(), 0);
    EntityId target =
        get_patch_layer().create_entity_immediate(build_mask<CompDataB>(), 0);

    // 2. Register the Bipartite Edge
    get_patch_layer().queue_edge_registration(
        source, target, build_mask<EdgeDataLink>());
    db.commit_pending_spawns();

    // Verify Edge exists in the Transient RAM Index
    auto outbound = db.get_outbound_edges(source);
    ASSERT_EQ(outbound.size(), 1);
    EntityId edge_ent = outbound[0];

    // Verify Edge physically exists in the database
    EXPECT_EQ(db.get_dynamic_mask(edge_ent), build_mask<EdgeDataLink>());

    // 3. Annihilate the Source
    db.destroy_entity(source);

    // Note: In the actual DAG, `compute_transitive_closure` dynamically queues
    // the deletion of the edge. For this raw Database test, we manually trigger
    // the cascade that the DAG would perform.
    for(auto e : outbound)
        db.destroy_entity(e);

    // 4. Assert Obliteration
    auto new_outbound = db.get_outbound_edges(source);
    EXPECT_TRUE(new_outbound.empty())
        << "Transient RAM Index failed to clear the adjacency list.";

    EXPECT_EQ(db.get_dynamic_mask(edge_ent), 0)
        << "Edge entity physical memory was not zeroed.";
    EXPECT_EQ(db.get_dynamic_mask(target), build_mask<CompDataB>())
        << "Target entity was erroneously destroyed during cascade.";
}
} // namespace usagi
