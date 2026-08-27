#pragma once
#include <cstdlib>
#include <cstring>

// M12.0: immutable startup diagnostic options read once from environment.
namespace Diagnostics {

class DiagnosticOptions {
public:
	bool trace_target() const { return trace_target_; }
	bool trace_refine() const { return trace_refine_; }
	bool verbose_amr() const { return verbose_amr_; }
	bool checksum_trace() const { return checksum_trace_; }
	bool refresh_idempotence() const { return refresh_idempotence_; }
	bool state_invariant() const { return state_invariant_; }
	bool memory_high_water() const { return memory_high_water_; }

	static const DiagnosticOptions &instance()
	{
		static const DiagnosticOptions options;
		return options;
	}

private:
	DiagnosticOptions()
		: trace_target_(flag_enabled("LAGRANGIAN_TRACE_TARGET")),
		  trace_refine_(flag_enabled("LAGRANGIAN_TRACE_REFINE")),
		  verbose_amr_(flag_enabled("LAGRANGIAN_VERBOSE_AMR")),
		  checksum_trace_(flag_enabled("LAGRANGIAN_TRACE_CHECKSUM")),
		  refresh_idempotence_(
			flag_enabled("LAGRANGIAN_CHECK_REFRESH_IDEMPOTENCE")),
		  state_invariant_(flag_enabled("LAGRANGIAN_CHECK_STATE_INVARIANTS")),
		  memory_high_water_(flag_enabled("LAGRANGIAN_MEMORY_HIGH_WATER"))
	{
	}

	static bool flag_enabled(const char *name)
	{
		const char *value = std::getenv(name);
		return value != NULL && value[0] != '\0' && std::strcmp(value, "0") != 0;
	}

	bool trace_target_;
	bool trace_refine_;
	bool verbose_amr_;
	bool checksum_trace_;
	bool refresh_idempotence_;
	bool state_invariant_;
	bool memory_high_water_;
};

} // namespace Diagnostics
