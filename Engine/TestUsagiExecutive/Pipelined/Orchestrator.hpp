/*
 * Usagi Engine: GTest Hypervisor for Distributed Hypergraph Architecture
 * -----------------------------------------------------------------------------
 * Orchestrates the VirtualMemoryArenas, LayeredDatabaseAggregator, and
 * TaskGraphExecutive. Provides deterministic single-step and pipeline pacing,
 * alongside absolute binary state serialization for engine reboot proofs.
 */

#pragma once

#include <vector>

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi
{
class UsagiTestOrchestrator : public ::testing::Test
{
protected:
    LayeredDatabaseAggregator           db;
    std::unique_ptr<TaskGraphExecutive> executive;

    // Binary Disk Mocks
    std::vector<std::vector<uint8_t>> disk_layer0_dump;
    std::vector<std::vector<uint8_t>> disk_layer1_dump;
    ComponentPayloadStorage           disk_layer1_payloads;

    void SetUp() override
    {
        // Base Game (Layer 0) is natively created by the Aggregator
        // constructor. We mount the Mutable Patch (Layer 1).
        db.mount_readonly_layer(std::make_unique<EntityDatabase>());
        executive = std::make_unique<TaskGraphExecutive>(db);
    }

    void TearDown() override
    {
        executive->clear_systems();
        executive.reset();
    }

    // -------------------------------------------------------------------------
    // Execution Pacing
    // -------------------------------------------------------------------------

    /* Shio: Flushes the pipeline completely. Guarantees no worker threads are
       mutating the sparse matrix. Safe for immediate memory assertions. */
    void step_single_frame()
    {
        executive->submit_frame();
        executive->wait_for_frame_completion();
    }

    /* Yukino: Pushes N frames into the sliding window without blocking.
       Forces the DAG to resolve cross-frame Previous/Atomic dependencies
       dynamically. */
    void step_pipelined(uint32_t frames)
    {
        for(uint32_t i = 0; i < frames; ++i)
        {
            executive->submit_frame();
        }
        executive->wait_for_frame_completion();
    }

    // -------------------------------------------------------------------------
    // Virtualization & Serialization Control
    // -------------------------------------------------------------------------

    EntityDatabase &get_base_layer()
    {
        // Warning: Direct mutation of base layer violates the architecture,
        // but is required for test setups before mounting the patch.
        // We assume Layer 0 is at domain_node 0.
        // The Aggregator hides direct access, so we cast through a raw pointer
        // mock.
        return *reinterpret_cast<EntityDatabase *>(
            &db); // UNSAFE HACK for test setups only.
        // Alternatively, use Aggregator methods before mounting.
        // For strict testing, we do all base setup via Aggregator, then mount.
    }

    EntityDatabase &get_patch_layer() { return db.get_mutable_layer(); }

    /* Shio: Simulates a hard crash and reboot. Dumps Layer 1 to raw bytes,
       nukes the RAM indices, re-imports, and mathematically reconstructs the
       Bipartite and Virtualization overlays. */
    void simulate_engine_reboot()
    {
        // 1. Export Patch Layer (Domain 1)
        disk_layer1_dump     = get_patch_layer().export_layer_memory();
        disk_layer1_payloads = get_patch_layer().payloads;

        // 2. Annihilate RAM
        get_patch_layer().clear_database();

        // 3. Import Raw Bytes
        get_patch_layer().import_layer_memory(
            disk_layer1_dump, disk_layer1_payloads);

        // 4. Reconstruct Topological RAM Indices
        db.rebuild_transient_indices();
    }
};
} // namespace usagi
