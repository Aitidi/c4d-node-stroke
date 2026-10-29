#pragma once
#include "graph_ops.h"

namespace wire_rs {
// Built-in Redshift workflow rules: which input a Redshift user expects a source to
// feed (material -> Output surface, roughness texture -> material roughness, ...).
// `helperAsset`, when set, is a node to insert in between (e.g. a Bump Map for a
// normal texture), wired helperIn/helperOut.
struct Suggestion {
  maxon::GraphNode out, in;
  maxon::String helperAsset, helperIn, helperOut;
  maxon::Int helperInputType = -1;
  cinema::Bool IsValid() const { return out.IsValid() && in.IsValid(); }
};
// fixedOut may be invalid. Returns an invalid suggestion when no rule applies.
maxon::Result<Suggestion> Suggest(const maxon::GraphNode& source, const maxon::GraphNode& fixedOut, const maxon::GraphNode& target);
// Wraps an explicit out -> in link: when `in` is a material bump input or the Output
// displacement input and nothing upstream of `out` is already a Bump Map /
// Displacement node, the matching helper node is inserted in between.
maxon::Result<Suggestion> Exact(const maxon::GraphNode& out, const maxon::GraphNode& in);
// Applies a valid suggestion. Must run inside a graph transaction.
maxon::Result<void> Apply(const maxon::nodes::NodesGraphModelRef& graph, const Suggestion& suggestion);
// Keeps inserted helper nodes where Apply placed them (see rs_rules.cpp).
cinema::Bool RegisterPlacement();
void EndPlacement();
}
