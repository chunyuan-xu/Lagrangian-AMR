#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "diagnostics/memory_probe_observer.h"

namespace Diagnostics {

inline bool memory_high_water_output_directory(char *buffer, std::size_t capacity)
{
	const char *env = std::getenv("LAGRANGIAN_MEMORY_HIGH_WATER_DIR");
	if (env == NULL || env[0] == '\0') {
		env = "output";
	}
	if (capacity == 0) {
		return false;
	}
	std::snprintf(buffer, capacity, "%s", env);
	return true;
}

inline bool write_memory_high_water_rank_output(
	int rank, int size, const MemoryProbeContext &context)
{
	if (context.tracker == NULL || context.current_step == NULL) {
		return false;
	}
	char directory[1024];
	if (!memory_high_water_output_directory(directory, sizeof(directory))) {
		return false;
	}
	char path[2048];
	std::snprintf(path, sizeof(path),
		"%s/memory_high_water_rank_%d.json", directory, rank);
	std::FILE *file = std::fopen(path, "w");
	if (file == NULL) {
		return false;
	}

	const MemoryHighWaterValues &v = context.tracker->values();
	const int written = std::fprintf(file,
		"{\n"
		"  \"schema\": \"lagrangian-amr.memory-high-water.per-rank.v1\",\n"
		"  \"rank\": %d,\n"
		"  \"size\": %d,\n"
		"  \"coverage_bits\": %u,\n"
		"  \"values\": {\n"
		"    \"completed_exchange_count\": %llu,\n"
		"    \"max_generation\": %llu,\n"
		"    \"max_step\": %llu,\n"
		"    \"max_local_leaves\": %llu,\n"
		"    \"max_ghost_leaves\": %llu,\n"
		"    \"max_mirror_leaves\": %llu,\n"
		"    \"max_logical_send_entries\": %llu,\n"
		"    \"max_local_payload_bytes\": %llu,\n"
		"    \"max_ghost_payload_buffer_bytes\": %llu,\n"
		"    \"max_estimated_local_payload_bytes\": %llu,\n"
		"    \"max_estimated_ghost_payload_bytes\": %llu,\n"
		"    \"max_logical_send_payload_bytes\": %llu,\n"
		"    \"max_logical_receive_payload_bytes\": %llu,\n"
		"    \"max_estimated_send_payload_bytes\": %llu,\n"
		"    \"max_estimated_receive_payload_bytes\": %llu,\n"
		"    \"cumulative_logical_send_payload_bytes\": %llu,\n"
		"    \"cumulative_logical_receive_payload_bytes\": %llu,\n"
		"    \"cumulative_estimated_send_payload_bytes\": %llu,\n"
		"    \"cumulative_estimated_receive_payload_bytes\": %llu,\n"
		"    \"max_p4est_reported_ghost_bytes\": %llu\n"
		"  }\n"
		"}\n",
		rank,
		size,
		static_cast<unsigned>(context.coverage_bits),
		static_cast<unsigned long long>(v.completed_exchange_count),
		static_cast<unsigned long long>(v.max_generation),
		static_cast<unsigned long long>(v.max_step),
		static_cast<unsigned long long>(v.max_local_leaves),
		static_cast<unsigned long long>(v.max_ghost_leaves),
		static_cast<unsigned long long>(v.max_mirror_leaves),
		static_cast<unsigned long long>(v.max_logical_send_entries),
		static_cast<unsigned long long>(v.max_local_payload_bytes),
		static_cast<unsigned long long>(v.max_ghost_payload_buffer_bytes),
		static_cast<unsigned long long>(v.max_estimated_local_payload_bytes),
		static_cast<unsigned long long>(v.max_estimated_ghost_payload_bytes),
		static_cast<unsigned long long>(v.max_logical_send_payload_bytes),
		static_cast<unsigned long long>(v.max_logical_receive_payload_bytes),
		static_cast<unsigned long long>(v.max_estimated_send_payload_bytes),
		static_cast<unsigned long long>(v.max_estimated_receive_payload_bytes),
		static_cast<unsigned long long>(v.cumulative_logical_send_payload_bytes),
		static_cast<unsigned long long>(v.cumulative_logical_receive_payload_bytes),
		static_cast<unsigned long long>(v.cumulative_estimated_send_payload_bytes),
		static_cast<unsigned long long>(v.cumulative_estimated_receive_payload_bytes),
		static_cast<unsigned long long>(v.max_p4est_reported_ghost_bytes));
	const int closed = std::fclose(file);
	return written > 0 && closed == 0;
}

} // namespace Diagnostics