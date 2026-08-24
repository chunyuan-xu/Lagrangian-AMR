#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace Diagnostics {

struct MemoryProbeSample {
	std::uint64_t generation;
	std::uint64_t step;
	std::uint64_t local_leaves;
	std::uint64_t ghost_leaves;
	std::uint64_t mirror_leaves;
	std::uint64_t logical_send_entries;
	std::uint64_t p4est_reported_ghost_bytes;
};

struct MemoryHighWaterValues {
	std::uint64_t completed_exchange_count;
	std::uint64_t max_generation;
	std::uint64_t max_step;
	std::uint64_t max_local_leaves;
	std::uint64_t max_ghost_leaves;
	std::uint64_t max_mirror_leaves;
	std::uint64_t max_logical_send_entries;
	std::uint64_t max_local_payload_bytes;
	std::uint64_t max_ghost_payload_buffer_bytes;
	std::uint64_t max_estimated_local_payload_bytes;
	std::uint64_t max_estimated_ghost_payload_bytes;
	std::uint64_t max_logical_send_payload_bytes;
	std::uint64_t max_logical_receive_payload_bytes;
	std::uint64_t max_estimated_send_payload_bytes;
	std::uint64_t max_estimated_receive_payload_bytes;
	std::uint64_t cumulative_logical_send_payload_bytes;
	std::uint64_t cumulative_logical_receive_payload_bytes;
	std::uint64_t cumulative_estimated_send_payload_bytes;
	std::uint64_t cumulative_estimated_receive_payload_bytes;
	std::uint64_t max_p4est_reported_ghost_bytes;
};

class MemoryHighWaterTracker {
public:
	MemoryHighWaterTracker() { reset(); }

	void reset()
	{
		values_ = MemoryHighWaterValues{};
	}

	bool observe_completed_exchange(const MemoryProbeSample &sample,
		std::uint64_t payload_bytes, std::uint64_t estimated_payload_bytes)
	{
		if (payload_bytes == 0 || estimated_payload_bytes == 0) {
			return false;
		}

		std::uint64_t local_bytes = 0;
		std::uint64_t ghost_bytes = 0;
		std::uint64_t estimated_local_bytes = 0;
		std::uint64_t estimated_ghost_bytes = 0;
		std::uint64_t send_bytes = 0;
		std::uint64_t receive_bytes = 0;
		std::uint64_t estimated_send_bytes = 0;
		std::uint64_t estimated_receive_bytes = 0;
		std::uint64_t completed_count = 0;
		std::uint64_t cumulative_send = 0;
		std::uint64_t cumulative_receive = 0;
		std::uint64_t cumulative_estimated_send = 0;
		std::uint64_t cumulative_estimated_receive = 0;

		if (!checked_multiply(sample.local_leaves, payload_bytes, local_bytes) ||
			!checked_multiply(sample.ghost_leaves, payload_bytes, ghost_bytes) ||
			!checked_multiply(sample.local_leaves, estimated_payload_bytes, estimated_local_bytes) ||
			!checked_multiply(sample.ghost_leaves, estimated_payload_bytes, estimated_ghost_bytes) ||
			!checked_multiply(sample.logical_send_entries, payload_bytes, send_bytes) ||
			!checked_multiply(sample.ghost_leaves, payload_bytes, receive_bytes) ||
			!checked_multiply(sample.logical_send_entries, estimated_payload_bytes, estimated_send_bytes) ||
			!checked_multiply(sample.ghost_leaves, estimated_payload_bytes, estimated_receive_bytes) ||
			!checked_add(values_.completed_exchange_count, 1, completed_count) ||
			!checked_add(values_.cumulative_logical_send_payload_bytes, send_bytes, cumulative_send) ||
			!checked_add(values_.cumulative_logical_receive_payload_bytes, receive_bytes, cumulative_receive) ||
			!checked_add(values_.cumulative_estimated_send_payload_bytes, estimated_send_bytes, cumulative_estimated_send) ||
			!checked_add(values_.cumulative_estimated_receive_payload_bytes, estimated_receive_bytes, cumulative_estimated_receive)) {
			return false;
		}

		values_.completed_exchange_count = completed_count;
		values_.max_generation = std::max(values_.max_generation, sample.generation);
		values_.max_step = std::max(values_.max_step, sample.step);
		values_.max_local_leaves = std::max(values_.max_local_leaves, sample.local_leaves);
		values_.max_ghost_leaves = std::max(values_.max_ghost_leaves, sample.ghost_leaves);
		values_.max_mirror_leaves = std::max(values_.max_mirror_leaves, sample.mirror_leaves);
		values_.max_logical_send_entries = std::max(values_.max_logical_send_entries, sample.logical_send_entries);
		values_.max_local_payload_bytes = std::max(values_.max_local_payload_bytes, local_bytes);
		values_.max_ghost_payload_buffer_bytes = std::max(values_.max_ghost_payload_buffer_bytes, ghost_bytes);
		values_.max_estimated_local_payload_bytes = std::max(values_.max_estimated_local_payload_bytes, estimated_local_bytes);
		values_.max_estimated_ghost_payload_bytes = std::max(values_.max_estimated_ghost_payload_bytes, estimated_ghost_bytes);
		values_.max_logical_send_payload_bytes = std::max(values_.max_logical_send_payload_bytes, send_bytes);
		values_.max_logical_receive_payload_bytes = std::max(values_.max_logical_receive_payload_bytes, receive_bytes);
		values_.max_estimated_send_payload_bytes = std::max(values_.max_estimated_send_payload_bytes, estimated_send_bytes);
		values_.max_estimated_receive_payload_bytes = std::max(values_.max_estimated_receive_payload_bytes, estimated_receive_bytes);
		values_.cumulative_logical_send_payload_bytes = cumulative_send;
		values_.cumulative_logical_receive_payload_bytes = cumulative_receive;
		values_.cumulative_estimated_send_payload_bytes = cumulative_estimated_send;
		values_.cumulative_estimated_receive_payload_bytes = cumulative_estimated_receive;
		values_.max_p4est_reported_ghost_bytes = std::max(
			values_.max_p4est_reported_ghost_bytes,
			sample.p4est_reported_ghost_bytes);
		return true;
	}

	const MemoryHighWaterValues &values() const { return values_; }

private:
	static bool checked_multiply(std::uint64_t lhs, std::uint64_t rhs,
		std::uint64_t &result)
	{
		if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs) {
			return false;
		}
		result = lhs * rhs;
		return true;
	}

	static bool checked_add(std::uint64_t lhs, std::uint64_t rhs,
		std::uint64_t &result)
	{
		if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
			return false;
		}
		result = lhs + rhs;
		return true;
	}

	MemoryHighWaterValues values_;
};

} // namespace Diagnostics
