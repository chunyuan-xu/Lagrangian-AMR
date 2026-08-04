#pragma once
#include <cassert>
#include <cstddef>
#include <p4est.h>
#include <p4est_ghost.h>
#include "defines.h"

// M3.2: GhostSession — compatibility wrapper around the p4est ghost
// lifecycle (create / exchange / destroy) that tracks a generation and
// invalidates after topology changes.
//
// Debug detection: in builds without NDEBUG (this project builds with
// -O2 -g, no NDEBUG, so <cassert> is active), accessing (get/data/remote)
// or exchanging an invalidated session aborts via assert. A bug where a
// topology change (refine/coarsen/balance/partition) is followed by using
// the stale ghost therefore surfaces immediately as an assertion failure
// instead of silent wrong results or a NULL dereference.
//
// M3.2 keeps the existing call sites working: they obtain the raw
// `p4est_ghost_t*` / `quad_data_t*` from get()/data() and keep passing them
// to p4est_iterate / phase functions. The phased migration of those call
// sites onto session methods is M3.3. `remote()` is the read-only remote
// snapshot accessor that M3.4 will route callbacks through.

class GhostSession {
public:
	GhostSession()
		: forest_(NULL), ghost_(NULL), data_(NULL),
		  data_size_(0), generation_(0), topology_version_(0), valid_(false)
	{
	}

	// Build a fresh ghost of the given connectivity plus a user-data buffer
	// of elem_count quad_data_t, then exchange once.
	void initialize(p4est_t *forest, p4est_connect_type_t connectivity)
	{
		destroy();
		forest_ = forest;
		ghost_ = p4est_ghost_new(forest_, connectivity);
		data_ = P4EST_ALLOC(quad_data_t, ghost_->ghosts.elem_count);
		data_size_ = sizeof(quad_data_t) * ghost_->ghosts.elem_count;
		generation_++;
		valid_ = true;
		exchange();
	}

	// Destroy the current ghost + buffer and mark the session invalid.
	// Safe to call when empty.
	void destroy()
	{
		if (ghost_) {
			p4est_ghost_destroy(ghost_);
			ghost_ = NULL;
		}
		if (data_) {
			P4EST_FREE(data_);
			data_ = NULL;
		}
		data_size_ = 0;
		valid_ = false;
	}

	// Mark the current generation stale. Call after any topology change
	// (p4est_refine/coarsen/balance/partition). Using or exchanging the
	// session before the next initialize()/rebuild() is a debug abort.
	void invalidate_after_topology_change()
	{
		valid_ = false;
		topology_version_++;
	}

	// Rebuild after a topology change: destroy + initialize + exchange.
	void rebuild(p4est_t *forest, p4est_connect_type_t connectivity)
	{
		initialize(forest, connectivity);
	}

	// Wrap p4est_ghost_exchange_data.
	void exchange()
	{
		assert(valid_ && "GhostSession used after topology change without rebuild");
		assert(ghost_ != NULL && data_ != NULL);
		p4est_ghost_exchange_data(forest_, ghost_, data_);
	}

	p4est_ghost_t *get() const
	{
		assert(valid_ && "GhostSession::get after topology change without rebuild");
		return ghost_;
	}

	quad_data_t *data() const
	{
		assert(valid_ && "GhostSession::data after topology change without rebuild");
		return data_;
	}

	// Read-only access to a remote (ghost) cell. M3.4 migrates callbacks to
	// use this instead of writing through a mutable quad_data_t* into the
	// ghost mirror.
	const quad_data_t &remote(p4est_locidx_t ghost_id) const
	{
		assert(valid_ && "GhostSession::remote after topology change without rebuild");
		return data_[ghost_id];
	}

	bool empty() const { return ghost_ == NULL; }
	bool valid_remote_id(p4est_locidx_t ghost_id) const
	{
		assert(valid_ && "GhostSession::valid_remote_id after topology change without rebuild");
		return ghost_id >= 0 && ghost_id < static_cast<p4est_locidx_t>(ghost_->ghosts.elem_count);
	}

	bool valid() const { return valid_; }
	size_t generation() const { return generation_; }
	size_t topology_version() const { return topology_version_; }

	~GhostSession() { destroy(); }

	GhostSession(const GhostSession &) = delete;
	GhostSession &operator=(const GhostSession &) = delete;

private:
	p4est_t *forest_;
	p4est_ghost_t *ghost_;
	quad_data_t *data_;
	size_t data_size_;
	size_t generation_;
	size_t topology_version_;
	bool valid_;
};
