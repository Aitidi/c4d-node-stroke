#include "graph_ops.h"
#include "rs_rules.h"
#include "native_layout.h"
#include "maxon/derived_nodeattributes.h"
#include "maxon/derived_portattributes.h"
#include "../res/c4d_symbols.h"

using namespace cinema;
namespace wire_ops {
namespace {
// Local development IDs, not registered for distribution.
constexpr Int32 kChainId = 1069993;
constexpr Int32 kFanOutId = 1069992;
constexpr Int32 kInsertId = 1069991;
constexpr Int32 kAddRuleId = 1069990;
constexpr Int32 kOpenRulesId = 1069989;
constexpr Int32 kFanInId = 1069988;

using maxon::GraphNode;
using NodeArray = maxon::BaseArray<GraphNode>;

maxon::Result<maxon::String> AssetOf(const GraphNode& node) {
  iferr_scope;
  const maxon::IdAndVersion asset = node.GetValue(maxon::nodes::AssetId).GetOrDefault() iferr_return;
  return asset.first.ToString();
}
maxon::Result<Bool> HasInput(const GraphNode& in) {
  iferr_scope;
  Bool any = false;
  in.GetConnections(maxon::PORT_DIR::INPUT, [&any](const maxon::GraphConnection&) -> maxon::Result<Bool> {
    any = true;
    return false;
  }) iferr_return;
  return any;
}
maxon::Result<Bool> AlreadyLinked(const GraphNode& source, const GraphNode& target) {
  iferr_scope;
  Bool linked = false;
  maxon::GraphModelHelper::GetDirectSuccessors(source, maxon::NODE_KIND::NODE, [&](const GraphNode& n) -> maxon::Result<Bool> {
    if (n == target) linked = true;
    return !linked;
  }) iferr_return;
  return linked;
}
// Ports in the order the node editor lists them (falls back to enumeration order).
maxon::Result<NodeArray> Ports(const GraphNode& list, maxon::NODE_KIND kind) {
  iferr_scope;
  NodeArray out;
  maxon::BaseArray<Int> order;
  list.GetChildren([&out, &order](const GraphNode& p) -> maxon::Result<Bool> {
    iferr_scope;
    const Int index = p.GetValue(maxon::nodes::ReqOrderIndex).GetOrDefault() iferr_return;
    out.Append(p) iferr_return;
    order.Append(index) iferr_return;
    return true;
  }, kind) iferr_return;
  for (Int i = 1; i < out.GetCount(); ++i) {
    for (Int j = i; j > 0 && order[j - 1] > order[j]; --j) {
      maxon::Swap(order[j - 1], order[j]);
      maxon::Swap(out[j - 1], out[j]);
    }
  }
  return out;
}
// Coarse type family so col<3> and col<4> (or float32/float64) count as a match:
// "net.maxon.parametrictype.col<4,float64>" -> "net.maxon.parametrictype.col".
maxon::Result<maxon::String> TypeFamily(const GraphNode& port) {
  iferr_scope;
  const maxon::DataType type = port.GetValue(maxon::nodes::PortType).GetOrDefault() iferr_return;
  if (!type) return maxon::String();
  maxon::BaseArray<Char> chars = type.GetId().ToString().GetCString() iferr_return;
  Int end = 0;
  while (end < chars.GetCount() && chars[end] != '<' && chars[end] != 0) ++end;
  while (end > 0 && chars[end - 1] >= '0' && chars[end - 1] <= '9') --end;
  maxon::String family;
  family.SetCString(chars.GetFirst(), end) iferr_return;
  return family;
}
maxon::Result<GraphNode> FindPort(const GraphNode& list, const maxon::String& id) {
  iferr_scope;
  maxon::InternedId iid;
  iid.Init(id) iferr_return;
  GraphNode port = list.FindChild(iid) iferr_return;
  return port;
}

// Picks the port pair for source -> target. Either side may be fixed. Custom rules
// win; then a free input of the same type family, then any free connectable input,
// then (if allowed) an occupied input that will be replaced. Ports are tried in the
// order the node editor lists them, so obscure ports of a matching type lose.
maxon::Result<PortPair> ChoosePorts(const GraphNode& source, const GraphNode& target, const GraphNode& fixedOut,
  const GraphNode& fixedIn, const maxon::BaseArray<Rule>& rules, Bool allowOccupied) {
  iferr_scope;
  const maxon::String sourceAsset = AssetOf(source) iferr_return;
  const maxon::String targetAsset = AssetOf(target) iferr_return;
  const GraphNode sourceOutputs = source.GetOutputs() iferr_return;
  const GraphNode targetInputs = target.GetInputs() iferr_return;
  for (const Rule& r : rules) {
    if (r.sourceAsset != sourceAsset || r.targetAsset != targetAsset) continue;
    GraphNode out = FindPort(sourceOutputs, r.sourcePort) iferr_return;
    GraphNode in = FindPort(targetInputs, r.targetPort) iferr_return;
    if (!out.IsValid() || !in.IsValid()) continue;
    if (fixedOut.IsValid() && !(out == fixedOut)) continue;
    if (fixedIn.IsValid() && !(in == fixedIn)) continue;
    const Bool ok = out.IsConnectable(in) iferr_return;
    if (ok) return PortPair{out, in};
  }
  NodeArray outs, ins;
  if (fixedOut.IsValid()) { outs.Append(fixedOut) iferr_return; }
  else { outs = Ports(sourceOutputs, maxon::NODE_KIND::OUTPORT) iferr_return; }
  if (fixedIn.IsValid()) { ins.Append(fixedIn) iferr_return; }
  else { ins = Ports(targetInputs, maxon::NODE_KIND::INPORT) iferr_return; }
  for (Int pass = 0; pass < (allowOccupied ? 3 : 2); ++pass) {
    for (const GraphNode& out : outs) {
      const maxon::String outFamily = TypeFamily(out) iferr_return;
      for (const GraphNode& in : ins) {
        const Bool occupied = HasInput(in) iferr_return;
        if (occupied && pass < 2 && !fixedIn.IsValid()) continue;
        if (pass == 0) {
          const maxon::String inFamily = TypeFamily(in) iferr_return;
          if (outFamily.IsEmpty() || outFamily != inFamily) continue;
        }
        const Bool ok = out.IsConnectable(in) iferr_return;
        if (ok) return PortPair{out, in};
      }
    }
  }
  return PortPair{};
}

// Graph-space x of a node from the undocumented 2027 layout data, used only to
// order a selection left to right. Falls back to selection order when unavailable.
Float32 NodeX(const GraphNode& node, Int fallback) {
  iferr (const maxon::BaseArray<wire_native::NodeRecord> records = wire_native::ReadNodePositions(node))
    return Float32(fallback) * 1e6f;
  return records.IsEmpty() ? Float32(fallback) * 1e6f : records[0].position.x;
}
maxon::Result<NodeArray> SelectedNodesLeftToRight(const maxon::nodes::NodesGraphModelRef& graph) {
  iferr_scope;
  NodeArray nodes;
  maxon::GraphModelHelper::GetSelectedNodes(graph, maxon::NODE_KIND::NODE, [&nodes](const GraphNode& n) -> maxon::Result<Bool> {
    iferr_scope;
    nodes.Append(n) iferr_return;
    return true;
  }) iferr_return;
  maxon::BaseArray<Float32> xs;
  for (Int i = 0; i < nodes.GetCount(); ++i) { xs.Append(NodeX(nodes[i], i)) iferr_return; }
  for (Int i = 1; i < nodes.GetCount(); ++i) {
    for (Int j = i; j > 0 && xs[j - 1] > xs[j]; --j) {
      maxon::Swap(xs[j - 1], xs[j]);
      maxon::Swap(nodes[j - 1], nodes[j]);
    }
  }
  return nodes;
}

enum class Op { CHAIN, FAN_OUT, FAN_IN, INSERT, ADD_RULE };

maxon::Result<void> Insert(const maxon::nodes::NodesGraphModelRef& graph, const maxon::BaseArray<Rule>& rules) {
  iferr_scope;
  maxon::BaseArray<PortPair> wires;
  maxon::GraphModelHelper::GetSelectedConnections(graph, [&wires](const GraphNode& s, const GraphNode& t, const maxon::Wires&) -> maxon::Result<Bool> {
    iferr_scope;
    wires.Append(PortPair{s, t}) iferr_return;
    return true;
  }) iferr_return;
  NodeArray candidates = SelectedNodesLeftToRight(graph) iferr_return;
  // Without a selected wire, use the single wire running between selected nodes,
  // e.g. select A, X and B where A feeds B to insert X.
  if (wires.IsEmpty()) {
    for (const GraphNode& target : candidates) {
      const GraphNode inputs = target.GetInputs() iferr_return;
      const NodeArray ports = Ports(inputs, maxon::NODE_KIND::INPORT) iferr_return;
      for (const GraphNode& in : ports) {
        maxon::BaseArray<GraphNode> sources;
        in.GetConnections(maxon::PORT_DIR::INPUT, [&sources](const maxon::GraphConnection& c) -> maxon::Result<Bool> {
          iferr_scope;
          sources.Append(c.first) iferr_return;
          return true;
        }) iferr_return;
        for (const GraphNode& out : sources) {
          const GraphNode owner = out.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
          for (const GraphNode& n : candidates) {
            if (n == owner) { wires.Append(PortPair{out, in}) iferr_return; }
          }
        }
      }
    }
  }
  if (wires.GetCount() != 1)
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_INSERT_SELECTION));
  const PortPair wire = wires[0];
  const GraphNode wireSource = wire.out.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
  const GraphNode wireTarget = wire.in.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
  GraphNode node;
  for (const GraphNode& n : candidates) {
    if (n == wireSource || n == wireTarget) continue;
    if (node.IsValid())
      return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_INSERT_ONE));
    node = n;
  }
  if (!node.IsValid())
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_INSERT_WITH_WIRE));
  const PortPair first = ChoosePorts(wireSource, node, wire.out, GraphNode(), rules, true) iferr_return;
  const PortPair second = ChoosePorts(node, wireTarget, GraphNode(), wire.in, rules, true) iferr_return;
  if (!first.out.IsValid() || !second.out.IsValid())
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_INSERT_NO_PORTS));
  maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
  maxon::GraphModelHelper::RemoveConnection(wire.out, wire.in) iferr_return;
  ConnectReplacing(first.out, first.in) iferr_return;
  ConnectReplacing(second.out, second.in) iferr_return;
  transaction.Commit() iferr_return;
  return maxon::OK;
}

maxon::Result<void> AppendRule(const Rule& rule) {
  iferr_scope;
  const maxon::String line = rule.sourceAsset + " "_s + rule.sourcePort + " "_s + rule.targetAsset + " "_s + rule.targetPort + "\n"_s;
  const maxon::BaseArray<Char> bytes = line.GetCString() iferr_return;
  AutoAlloc<BaseFile> file;
  const Filename path = RulesFile();
  if (!file || !file->Open(path, GeFExist(path) ? FILEOPEN::APPEND : FILEOPEN::WRITE))
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_RULES_WRITE));
  Int length = bytes.GetCount();
  if (length > 0 && bytes[length - 1] == 0) --length;
  if (!file->WriteBytes(bytes.GetFirst(), length))
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_RULES_WRITE));
  return maxon::OK;
}

maxon::Result<void> AddRule(const maxon::nodes::NodesGraphModelRef& graph) {
  iferr_scope;
  GraphNode out, in;
  Int count = 0;
  maxon::GraphModelHelper::GetSelectedNodes(graph, maxon::NODE_KIND::PORT_MASK, [&](const GraphNode& p) -> maxon::Result<Bool> {
    ++count;
    if (p.GetKind() == maxon::NODE_KIND::OUTPORT) out = p;
    if (p.GetKind() == maxon::NODE_KIND::INPORT) in = p;
    return true;
  }) iferr_return;
  if (count != 2 || !out.IsValid() || !in.IsValid())
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_SELECT_PORTS));
  const GraphNode source = out.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
  const GraphNode target = in.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
  const Bool ok = out.IsConnectable(in) iferr_return;
  if (!ok) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_NOT_CONNECTABLE));
  Rule rule;
  rule.sourceAsset = AssetOf(source) iferr_return;
  rule.targetAsset = AssetOf(target) iferr_return;
  rule.sourcePort = out.GetId().ToString();
  rule.targetPort = in.GetId().ToString();
  maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
  ConnectReplacing(out, in) iferr_return;
  transaction.Commit() iferr_return;
  AppendRule(rule) iferr_return;
  return maxon::OK;
}

maxon::Result<void> Apply(BaseDocument* doc, Op op) {
  iferr_scope;
  const maxon::nodes::NodesGraphModelRef graph = ActiveGraph(doc) iferr_return;
  const maxon::BaseArray<Rule> rules = LoadRules() iferr_return;
  if (op == Op::INSERT) return Insert(graph, rules);
  if (op == Op::ADD_RULE) return AddRule(graph);
  const NodeArray nodes = SelectedNodesLeftToRight(graph) iferr_return;
  if (nodes.GetCount() < 2)
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_SELECT_TWO_NODES));
  maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
  const Int last = nodes.GetCount() - 1;
  for (Int i = 1; i < nodes.GetCount(); ++i) {
    if (op == Op::CHAIN) { LinkNodes(nodes[i - 1], nodes[i], rules, true) iferr_return; }
    else if (op == Op::FAN_OUT) { LinkNodes(nodes[0], nodes[i], rules, true) iferr_return; }
    else { LinkNodes(nodes[i - 1], nodes[last], rules, false) iferr_return; }
  }
  transaction.Commit() iferr_return;
  return maxon::OK;
}

class GraphOpCommand final : public CommandData {
  Op _op;
public:
  explicit GraphOpCommand(Op op) : _op(op) {}
  Bool Execute(BaseDocument* doc, GeDialog*) override {
    iferr_scope_handler {
      MessageDialog(err.GetMessage());
      return false;
    };
    Apply(doc, _op) iferr_return;
    EventAdd();
    return true;
  }
};

class OpenRulesCommand final : public CommandData {
public:
  Bool Execute(BaseDocument*, GeDialog*) override { return OpenRulesFile(); }
};
}

maxon::Result<Bool> HasRule(const maxon::GraphNode& source, const maxon::GraphNode& target, const maxon::BaseArray<Rule>& rules) {
  iferr_scope;
  const maxon::String sourceAsset = AssetOf(source) iferr_return;
  const maxon::String targetAsset = AssetOf(target) iferr_return;
  for (const Rule& r : rules) {
    if (r.sourceAsset == sourceAsset && r.targetAsset == targetAsset) return true;
  }
  return false;
}
maxon::Result<PortPair> ChoosePortPair(const maxon::GraphNode& source, const maxon::GraphNode& target,
  const maxon::GraphNode& fixedOut, const maxon::GraphNode& fixedIn, const maxon::BaseArray<Rule>& rules, Bool allowOccupied) {
  return ChoosePorts(source, target, fixedOut, fixedIn, rules, allowOccupied);
}
maxon::Result<maxon::BaseArray<maxon::GraphNode>> EditorOrderPorts(const maxon::GraphNode& list, maxon::NODE_KIND kind) {
  return Ports(list, kind);
}
maxon::Result<void> LinkNodes(const maxon::GraphNode& source, const maxon::GraphNode& target, const maxon::BaseArray<Rule>& rules, Bool allowOccupied) {
  iferr_scope;
  const Bool linked = AlreadyLinked(source, target) iferr_return;
  if (linked) return maxon::OK;
  const Bool ruled = HasRule(source, target, rules) iferr_return;
  if (!ruled) {
    const wire_rs::Suggestion suggestion = wire_rs::Suggest(source, GraphNode(), target) iferr_return;
    if (suggestion.IsValid()) {
      const maxon::nodes::NodesGraphModelRef graph = ActiveGraph(GetActiveDocument()) iferr_return;
      wire_rs::Apply(graph, suggestion) iferr_return;
      return maxon::OK;
    }
  }
  const PortPair pair = ChoosePorts(source, target, GraphNode(), GraphNode(), rules, allowOccupied) iferr_return;
  if (!pair.out.IsValid())
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_NO_COMPATIBLE));
  ConnectReplacing(pair.out, pair.in) iferr_return;
  return maxon::OK;
}

maxon::Result<maxon::nodes::NodesGraphModelRef> ActiveGraph(BaseDocument* doc) {
  iferr_scope;
  if (!doc || !doc->GetActiveMaterial())
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_NO_MATERIAL));
  NodeMaterial* material = static_cast<NodeMaterial*>(doc->GetActiveMaterial());
  const maxon::Id space = GetActiveNodeSpaceId();
  if (!material->HasSpace(space))
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_NO_GRAPH));
  const maxon::nodes::NodesGraphModelRef& graph = material->GetGraph(space) iferr_return;
  if (graph.IsReadOnly())
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_READ_ONLY));
  return graph;
}

Filename RulesFile() {
  return GeGetC4DPath(C4D_PATH_PREFS) + Filename("nodestroke_rules.txt"_s);
}
static const char kRulesHeader[] =
  "# NodeStroke connection rules, one per line:\n"
  "# <source asset id> <source output port id> <target asset id> <target input port id>\n"
  "# Edit them in Preferences > NodeStroke > Edit Rules.\n";
Bool OpenRulesFile() {
  const Filename path = RulesFile();
  if (!GeFExist(path)) {
    AutoAlloc<BaseFile> file;
    if (!file || !file->Open(path, FILEOPEN::WRITE) || !file->WriteBytes(kRulesHeader, sizeof(kRulesHeader) - 1)) {
      MessageDialog(GeLoadString(IDS_ERR_RULES_CREATE));
      return false;
    }
  }
  return GeExecuteFile(path);
}
maxon::Result<void> SaveRules(const maxon::BaseArray<Rule>& rules) {
  iferr_scope;
  maxon::String text(kRulesHeader);
  for (const Rule& r : rules) text += r.sourceAsset + " "_s + r.sourcePort + " "_s + r.targetAsset + " "_s + r.targetPort + "\n"_s;
  const maxon::BaseArray<Char> bytes = text.GetCString() iferr_return;
  Int length = bytes.GetCount();
  if (length > 0 && bytes[length - 1] == 0) --length;
  AutoAlloc<BaseFile> file;
  if (!file || !file->Open(RulesFile(), FILEOPEN::WRITE) || !file->WriteBytes(bytes.GetFirst(), length))
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_RULES_WRITE));
  return maxon::OK;
}
maxon::Result<void> AddRuleFromSelection(BaseDocument* doc) {
  return Apply(doc, Op::ADD_RULE);
}

maxon::Result<maxon::BaseArray<Rule>> LoadRules() {
  iferr_scope;
  maxon::BaseArray<Rule> rules;
  const Filename path = RulesFile();
  if (!GeFExist(path)) return rules;
  AutoAlloc<BaseFile> file;
  if (!file || !file->Open(path, FILEOPEN::READ))
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_RULES_READ));
  maxon::BaseArray<Char> data;
  data.Resize(file->GetLength()) iferr_return;
  if (file->ReadBytes(data.GetFirst(), data.GetCount()) != data.GetCount())
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_RULES_READ));
  data.Append('\n') iferr_return;
  maxon::BaseArray<maxon::String> tokens;
  maxon::BaseArray<Char> token;
  Bool comment = false;
  for (const Char c : data) {
    const Bool space = c == ' ' || c == '\t' || c == '\r' || c == '\n';
    if (!comment && !space && c == '#' && token.IsEmpty()) comment = true;
    if (!comment && !space) { token.Append(c) iferr_return; }
    if ((space || c == '\n') && !token.IsEmpty()) {
      maxon::String s;
      s.SetCString(token.GetFirst(), token.GetCount()) iferr_return;
      tokens.Append(std::move(s)) iferr_return;
      token.Flush();
    }
    if (c == '\n') {
      // Malformed lines are ignored rather than failing every command.
      if (tokens.GetCount() == 4) {
        rules.Append(Rule{tokens[0], tokens[1], tokens[2], tokens[3]}) iferr_return;
      }
      tokens.Flush();
      comment = false;
    }
  }
  return rules;
}

maxon::Result<void> ConnectReplacing(const maxon::GraphNode& out, const maxon::GraphNode& in) {
  iferr_scope;
  maxon::BaseArray<maxon::GraphNode> existing;
  in.GetConnections(maxon::PORT_DIR::INPUT, [&existing](const maxon::GraphConnection& c) -> maxon::Result<Bool> {
    iferr_scope;
    existing.Append(c.first) iferr_return;
    return true;
  }) iferr_return;
  for (const maxon::GraphNode& source : existing) {
    maxon::GraphModelHelper::RemoveConnection(source, in) iferr_return;
  }
  out.Connect(in) iferr_return;
  return maxon::OK;
}

maxon::Result<void> ExtractNode(const maxon::GraphNode& node) {
  iferr_scope;
  struct Link { GraphNode out, in; };
  maxon::BaseArray<Link> incoming, outgoing;
  const GraphNode inputs = node.GetInputs() iferr_return;
  const NodeArray ins = Ports(inputs, maxon::NODE_KIND::INPORT) iferr_return;
  for (const GraphNode& in : ins) {
    in.GetConnections(maxon::PORT_DIR::INPUT, [&](const maxon::GraphConnection& c) -> maxon::Result<Bool> {
      iferr_scope;
      incoming.Append(Link{c.first, in}) iferr_return;
      return true;
    }) iferr_return;
  }
  const GraphNode outputs = node.GetOutputs() iferr_return;
  const NodeArray outs = Ports(outputs, maxon::NODE_KIND::OUTPORT) iferr_return;
  for (const GraphNode& out : outs) {
    out.GetConnections(maxon::PORT_DIR::OUTPUT, [&](const maxon::GraphConnection& c) -> maxon::Result<Bool> {
      iferr_scope;
      outgoing.Append(Link{out, c.first}) iferr_return;
      return true;
    }) iferr_return;
  }
  maxon::GraphModelHelper::RemoveAllConnections(node) iferr_return;
  // Bridge each downstream input to an upstream output that carries the same kind of
  // value as the node's output (or the input), first matching input in editor order.
  for (const Link& down : outgoing) {
    const maxon::String outFamily = TypeFamily(down.out) iferr_return;
    const maxon::String inFamily = TypeFamily(down.in) iferr_return;
    const GraphNode target = down.in.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
    for (const Link& up : incoming) {
      const GraphNode source = up.out.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
      if (source == target) continue;
      const maxon::String family = TypeFamily(up.out) iferr_return;
      if (family.IsEmpty() || (family != outFamily && family != inFamily)) continue;
      const Bool ok = up.out.IsConnectable(down.in) iferr_return;
      if (!ok) continue;
      up.out.Connect(down.in) iferr_return;
      break;
    }
  }
  return maxon::OK;
}
}

Bool RegisterGraphOps() {
  using namespace wire_ops;
  if (!RegisterCommandPlugin(kChainId, GeLoadString(IDS_CMD_CHAIN), PLUGINFLAG_HIDEPLUGINMENU, nullptr,
    GeLoadString(IDS_CMD_CHAIN_HELP), NewObjClear(GraphOpCommand, Op::CHAIN))) return false;
  if (!RegisterCommandPlugin(kFanOutId, GeLoadString(IDS_CMD_FAN_OUT), PLUGINFLAG_HIDEPLUGINMENU, nullptr,
    GeLoadString(IDS_CMD_FAN_OUT_HELP), NewObjClear(GraphOpCommand, Op::FAN_OUT))) return false;
  if (!RegisterCommandPlugin(kFanInId, GeLoadString(IDS_CMD_FAN_IN), PLUGINFLAG_HIDEPLUGINMENU, nullptr,
    GeLoadString(IDS_CMD_FAN_IN_HELP), NewObjClear(GraphOpCommand, Op::FAN_IN))) return false;
  if (!RegisterCommandPlugin(kInsertId, GeLoadString(IDS_CMD_INSERT), PLUGINFLAG_HIDEPLUGINMENU, nullptr,
    GeLoadString(IDS_CMD_INSERT_HELP), NewObjClear(GraphOpCommand, Op::INSERT))) return false;
  if (!RegisterCommandPlugin(kAddRuleId, GeLoadString(IDS_CMD_ADD_RULE), PLUGINFLAG_HIDEPLUGINMENU, nullptr,
    GeLoadString(IDS_CMD_ADD_RULE_HELP), NewObjClear(GraphOpCommand, Op::ADD_RULE))) return false;
  return RegisterCommandPlugin(kOpenRulesId, "NodeStroke: Open Rules File"_s, PLUGINFLAG_HIDE, nullptr,
    "Open the custom connection rules file."_s, NewObjClear(OpenRulesCommand));
}
