#pragma once
#include <cstdio>
#include "defines.h"
#include "core/trace.h"

// M12.3: refine transfer trace moved to diagnostics. Disabled mode performs
// no file operation. Rank-aware filename construction is bounded.
namespace Diagnostics {

class RefineTraceFile {
public:
	RefineTraceFile(p4est_t *p4est, const quad_data_t *parent,
		int step)
		: file_(NULL)
	{
		if (!refine_trace_enabled()) {
			return;
		}
		char fname[256];
		const int n = std::snprintf(fname, sizeof(fname),
			"refine_dbg_%d_%d.txt", p4est->mpisize, p4est->mpirank);
		if (n < 0 || n >= static_cast<int>(sizeof(fname))) {
			return;
		}
		file_ = std::fopen(fname, "a");
		if (file_) {
			const CVariable &vara = parent->m_vara;
			const CDoubleVector &center = vara.cell_vector(idCentroidCoord_cur);
			std::fprintf(file_,
				"REFINE_STEP_%d_PARENT at (%.6f, %.6f): parent SoundSpeed=%e, mass=%e, vol=%e\n",
				step, center.x, center.y,
				vara.cell(idSoundSpeed), vara.cell(idMass), vara.cell(idVolume));
		}
	}

	~RefineTraceFile()
	{
		if (file_) {
			std::fclose(file_);
		}
	}

	bool enabled() const { return file_ != NULL; }

	void write_child(int step, double px, double py, double sound_speed)
	{
		if (file_) {
			std::fprintf(file_,
				"REFINE_STEP_%d_CHILD at (%.6f, %.6f): child SoundSpeed=%e\n",
				step, px, py, sound_speed);
		}
	}

	RefineTraceFile(const RefineTraceFile &) = delete;
	RefineTraceFile &operator=(const RefineTraceFile &) = delete;

private:
	FILE *file_;
};

} // namespace Diagnostics
