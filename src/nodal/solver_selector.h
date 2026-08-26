#pragma once

#include <cstdint>

// E1a: experimental DBGF selector.  Default remains Legacy; DBGF is not yet
// available from the runtime selector.

namespace Nodal {

enum class SolverSelector : std::uint8_t {
	Legacy = 0,
	DBGFExperimental = 1
};

inline bool is_legacy(SolverSelector s)
{
	return s == SolverSelector::Legacy;
}

inline bool dbgf_experimental_available(SolverSelector s)
{
	return false;
}

} // namespace Nodal
