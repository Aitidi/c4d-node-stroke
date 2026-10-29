#pragma once
#include "c4d.h"
#include "maxon/graph.h"
#include "maxon/datadictionary.h"
namespace wire_native {
// These structures describe unsupported, version-specific internal data.
// No entry is assumed to be the active editor merely by its index or name.
struct ViewRecord { maxon::Id key; maxon::Float32 zoom; maxon::Vector2d32 offset; };
struct NodeRecord { maxon::Id key; maxon::Vector2d32 position; maxon::Float32 width; };
maxon::Result<maxon::BaseArray<ViewRecord>> ReadViews(const maxon::GraphNode& root);
maxon::Result<maxon::BaseArray<NodeRecord>> ReadNodePositions(const maxon::GraphNode& node);
}
