#pragma once
#include "defines.h"

// M10L.10: ABI-neutral, non-owning view over the fixed parent-edge array.
// It exposes ParentBounInfo by edge index and preserves the existing layout.
namespace AMRCallbacks {

class ParentEdgeView {
public:
	explicit ParentEdgeView(quad_data_t &data)
		: data_(&data), cdata_(nullptr) {}
	explicit ParentEdgeView(const quad_data_t &data)
		: data_(nullptr), cdata_(&data) {}

	ParentBounInfo &at(int edge)
	{
		return data_->m_pc_edge_data[edge];
	}

	const ParentBounInfo &at(int edge) const
	{
		return cdata_->m_pc_edge_data[edge];
	}

	int size() const { return CNDIM; }

private:
	quad_data_t *data_;
	const quad_data_t *cdata_;
};

} // namespace AMRCallbacks
