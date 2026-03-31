/*
 * Usagi Engine: Exhaustive Mathematical Proofs for Executive Normal Operations
 * -----------------------------------------------------------------------------
 * Validates the core endomorphism, JIT lock escalation, and topological sorting
 * over valid, non-crashing data-oriented Task Graphs.
 */

#include <unordered_map>

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi::poc::executive_1
{
// -----------------------------------------------------------------------------
// Component Definitions & Bit Mappings
// -----------------------------------------------------------------------------
struct ComponentTransform
{
    float x { 0.0f };
    float y { 0.0f };
};

template <>
consteval ComponentMask get_component_bit<ComponentTransform>()
{
    return 1ull << 0;
}

struct ComponentVelocity
{
    float vx { 0.0f };
    float vy { 0.0f };
};

template <>
consteval ComponentMask get_component_bit<ComponentVelocity>()
{
    return 1ull << 1;
}

struct ComponentRenderMesh
{
    uint32_t mesh_id { 0 };
    bool     is_visible { true };
};

template <>
consteval ComponentMask get_component_bit<ComponentRenderMesh>()
{
    return 1ull << 2;
}

struct ComponentParticleEmitter
{
    uint32_t active_particles { 0 };
};

template <>
consteval ComponentMask get_component_bit<ComponentParticleEmitter>()
{
    return 1ull << 3;
}

struct ComponentSpawnEvent
{
    float target_x { 0.0f };
    float target_y { 0.0f };
};

template <>
consteval ComponentMask get_component_bit<ComponentSpawnEvent>()
{
    return 1ull << 4;
}

// -----------------------------------------------------------------------------
// Mock Sparse Matrix Memory Chunks (For actual data read/write validation)
// -----------------------------------------------------------------------------
/* Yukino: In the actual engine, these would be cache-coherent linear allocators
   tied to the Archetype. For the PoC tests, we use maps keyed by EntityId to
   verify data transitions. */
std::unordered_map<EntityId, ComponentTransform>       g_transforms;
std::unordered_map<EntityId, ComponentVelocity>        g_velocities;
std::unordered_map<EntityId, ComponentRenderMesh>      g_meshes;
std::unordered_map<EntityId, ComponentParticleEmitter> g_emitters;

void clear_mock_memory()
{
    g_transforms.clear();
    g_velocities.clear();
    g_meshes.clear();
    g_emitters.clear();
}

// -----------------------------------------------------------------------------
// System Mocks
// -----------------------------------------------------------------------------

struct SystemUpdateTransforms
{
    using EntityQuery =
        EntityQuery<Read<ComponentVelocity>, Write<ComponentTransform>>;

    static void update(DatabaseAccess<SystemUpdateTransforms> &db)
    {
        // Query the structural intersection
        auto entities = db.query_entities(
            build_mask<ComponentVelocity, ComponentTransform>());
        for(EntityId id : entities)
        {
            // Actual Data Mutation
            g_transforms[id].x += g_velocities[id].vx;
            g_transforms[id].y += g_velocities[id].vy;
        }
    }
};

struct SystemDeleteInvisibleMeshes
{
    using EntityQuery = EntityQuery<
        Read<ComponentRenderMesh>, IntentDelete<ComponentRenderMesh>
    >;

    static void update(DatabaseAccess<SystemDeleteInvisibleMeshes> &db)
    {
        auto entities = db.query_entities(build_mask<ComponentRenderMesh>());
        for(EntityId id : entities)
        {
            if(!g_meshes[id].is_visible)
            {
                // Shio: Safe immediate destruction via JIT lock escalation
                db.destroy_entity(id);
                g_meshes.erase(id); // Cleanup mock memory
            }
        }
    }
};

struct SystemProcessSpawnEvents
{
    // Consumes a spawn event token to trigger topological re-entrancy
    using EntityQuery = EntityQuery<
        Read<ComponentSpawnEvent>, IntentDelete<ComponentSpawnEvent>
    >;

    static void update(DatabaseAccess<SystemProcessSpawnEvents> &db)
    {
        auto events = db.query_entities(build_mask<ComponentSpawnEvent>());
        for(EntityId id : events)
        {
            // Queue a new particle emitter structural mask
            db.queue_spawn(
                build_mask<ComponentTransform, ComponentParticleEmitter>());
            // Consume the trigger
            db.destroy_entity(id);
        }
    }
};

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiExecutiveNormalTest : public ::testing::Test
{
protected:
    EntityDatabase     db;
    TaskGraphExecutive executive { db };

    void SetUp() override
    {
        db.clear_database();
        executive.clear_systems();
        clear_mock_memory();
    }
};

// --- NORMAL OPERATIONS ---

TEST_F(UsagiExecutiveNormalTest, StaticExecution_AppliesDataMutations)
{
    EntityId e1 = db.create_entity_immediate(
        build_mask<ComponentTransform, ComponentVelocity>());
    g_transforms[e1] = { 10.0f, 20.0f };
    g_velocities[e1] = { 1.5f, -0.5f };

    executive.register_system<SystemUpdateTransforms>("UpdateTransforms");
    executive.execute_frame();

    const auto &log = executive.get_execution_log();
    ASSERT_EQ(log.size(), 1);
    EXPECT_EQ(log[0], "Executed: UpdateTransforms");

    // Mathematical verification of state transition
    EXPECT_FLOAT_EQ(g_transforms[e1].x, 11.5f);
    EXPECT_FLOAT_EQ(g_transforms[e1].y, 19.5f);
}

TEST_F(
    UsagiExecutiveNormalTest,
    JITLockEscalation_DynamicIntersectionValidatesData)
{
    /* Shio: We create an entity that dynamically intersects both Mesh and
       Transform. The Executive must correctly compute the JIT blast radius,
       serialize the deletion BEFORE the transform update to prevent modifying
       freed memory. */
    ComponentMask dual_mask = build_mask<
        ComponentTransform,
        ComponentVelocity,
        ComponentRenderMesh
    >();

    EntityId visible_ent      = db.create_entity_immediate(dual_mask);
    g_transforms[visible_ent] = { 0.0f, 0.0f };
    g_velocities[visible_ent] = { 1.0f, 1.0f };
    g_meshes[visible_ent]     = { 42, true }; // Visible, survives

    EntityId invisible_ent      = db.create_entity_immediate(dual_mask);
    g_transforms[invisible_ent] = { 100.0f, 100.0f };
    g_velocities[invisible_ent] = { 5.0f, 5.0f };
    g_meshes[invisible_ent]     = { 99, false }; // Invisible, condemned

    executive.register_system<SystemDeleteInvisibleMeshes>("DeleteMeshes");
    executive.register_system<SystemUpdateTransforms>("UpdateTransforms");

    executive.execute_frame();

    // Verify Topology: Delete must run before Update due to the JIT Escalation
    const auto &log     = executive.get_execution_log();
    int         del_idx = -1, upd_idx = -1;
    for(size_t i = 0; i < log.size(); ++i)
    {
        if(log[i] == "Executed: DeleteMeshes") del_idx = i;
        if(log[i] == "Executed: UpdateTransforms") upd_idx = i;
    }
    EXPECT_LT(del_idx, upd_idx) << "Topological sort failed: Data mutation ran "
                                   "parallel to memory destruction.";

    // Verify Data Integrity
    // Visible entity should be updated
    EXPECT_FLOAT_EQ(g_transforms[visible_ent].x, 1.0f);

    // Invisible entity should be mathematically erased from the sparse matrix
    auto surviving_transforms =
        db.query_entities(build_mask<ComponentTransform>());
    EXPECT_EQ(surviving_transforms.size(), 1);
    EXPECT_EQ(surviving_transforms[0], visible_ent);
}

TEST_F(
    UsagiExecutiveNormalTest, CyclicReEntrancy_ResolvesCleanlyAndSpawnsEntities)
{
    db.create_entity_immediate(build_mask<ComponentSpawnEvent>());

    executive.register_system<SystemProcessSpawnEvents>("ProcessSpawns");

    executive.execute_frame();

    const auto &log = executive.get_execution_log();

    bool re_entry_detected = false;
    for(const auto &msg : log)
    {
        if(msg == "--- Executive: Re-entrancy cycle triggered ---")
            re_entry_detected = true;
    }
    EXPECT_TRUE(re_entry_detected) << "Executive failed to evaluate the spawn "
                                      "queue and trigger topological loop.";

    // Mathematical proof: The transient event is consumed, and the new
    // composite entity exists.
    auto remaining_events =
        db.query_entities(build_mask<ComponentSpawnEvent>());
    EXPECT_TRUE(remaining_events.empty());

    auto spawned_particles = db.query_entities(
        build_mask<ComponentParticleEmitter, ComponentTransform>());
    EXPECT_EQ(spawned_particles.size(), 1)
        << "Structural deferred spawn failed to commit.";
}
} // namespace usagi::poc::executive_1
