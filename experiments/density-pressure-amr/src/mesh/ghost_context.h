#pragma once
#include "mesh/ghost_session.h"

// M8.1: GhostCallbackContext — shared callback user-data wrapper. Extracted
// from main.cpp so header-only callback modules can decode it consistently.

struct GhostCallbackContext
{
	GhostSession *session;
};
