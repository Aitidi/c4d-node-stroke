#pragma once
#include "c4d.h"
#include "maxon/graph_helper.h"
#include "maxon/nodesgraph.h"

namespace wire_ops {
// A user rule: when connecting a node of sourceAsset to a node of targetAsset,
// use sourcePort -> targetPort. Stored one per line in the preferences folder.
struct PortPair { maxon::GraphNode out, in; };
struct Rule { maxon::String sourceAsset, sourcePort, targetAsset, targetPort; };

maxon::Result<maxon::nodes::NodesGraphModelRef> ActiveGraph(cinema::BaseDocument* doc);
cinema::Filename RulesFile();
// Opens the rules file in the system editor, creating it with a header first.
cinema::Bool OpenRulesFile();
maxon::Result<maxon::BaseArray<Rule>> LoadRules();
// Rewrites the rules file with its header and `rules`.
maxon::Result<void> SaveRules(const maxon::BaseArray<Rule>& rules);
// Connects the selected output and input port of the active graph and appends them
// as a rule for these two node types.
maxon::Result<void> AddRuleFromSelection(cinema::BaseDocument* doc);
// Connects out -> in, first removing any wire already feeding `in`
// (GraphNode::Connect adds a second wire instead of replacing it).
maxon::Result<void> ConnectReplacing(const maxon::GraphNode& out, const maxon::GraphNode& in);
// Removes every wire of node and reconnects its upstream sources directly to its
// downstream inputs where the value types match. Must run inside a graph transaction.
maxon::Result<void> ExtractNode(const maxon::GraphNode& node);
// Picks ports for source -> target; either side may be fixed (pass an invalid node
// otherwise). Returns an invalid pair when nothing fits.
maxon::Result<PortPair> ChoosePortPair(const maxon::GraphNode& source, const maxon::GraphNode& target,
  const maxon::GraphNode& fixedOut, const maxon::GraphNode& fixedIn, const maxon::BaseArray<Rule>& rules, cinema::Bool allowOccupied);
maxon::Result<cinema::Bool> HasRule(const maxon::GraphNode& source, const maxon::GraphNode& target, const maxon::BaseArray<Rule>& rules);
// Children of an inputs/outputs list in the order the node editor shows them.
maxon::Result<maxon::BaseArray<maxon::GraphNode>> EditorOrderPorts(const maxon::GraphNode& list, maxon::NODE_KIND kind);
// Connects two nodes with automatically chosen ports (rules first). No-op if they
// are already linked. Must run inside a graph transaction.
maxon::Result<void> LinkNodes(const maxon::GraphNode& source, const maxon::GraphNode& target,
  const maxon::BaseArray<Rule>& rules, cinema::Bool allowOccupied);
}
cinema::Bool RegisterGraphOps();
