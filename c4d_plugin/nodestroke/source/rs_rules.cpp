#include "rs_rules.h"
#include "native_layout.h"
#include "maxon/derived_nodeattributes.h"
#include "maxon/derived_portattributes.h"
#include "maxon/nodeslib.h"
#include "maxon/node_undo.h"
#include "../res/c4d_symbols.h"
#include <functional>

using namespace cinema;
namespace wire_rs {
namespace {
using maxon::GraphNode;

// Built on demand: global maxon::String objects would be constructed before the
// maxon runtime is up and keep the plugin from loading.
maxon::String Core(const char* name) { return maxon::String("com.redshift3d.redshift4c4d.nodes.core.") + maxon::String(name); }
maxon::String OutputAsset() { return maxon::String("com.redshift3d.redshift4c4d.node.output"); }

enum class Role { GENERIC, SURFACE, BUMP, DISPLACEMENT, BASE, ROUGHNESS, METALNESS, OPACITY, EMISSION, AO, REFLECTION, NORMAL_MAP, HEIGHT_MAP, DISPLACEMENT_MAP, UV_CONTEXT };

maxon::Result<maxon::String> AssetOf(const GraphNode& node) {
  iferr_scope;
  const maxon::IdAndVersion asset = node.GetValue(maxon::nodes::AssetId).GetOrDefault() iferr_return;
  return asset.first.ToString();
}
Bool Has(const maxon::String& text, const char* word) {
  return text.FindIndex(maxon::String(word)) >= 0;
}

// What a texture is for, guessed from its file name the way texture sets are named.
maxon::Result<Role> TextureRole(const GraphNode& texture) {
  iferr_scope;
  const GraphNode inputs = texture.GetInputs() iferr_return;
  maxon::InternedId tex0, path;
  tex0.Init(Core("texturesampler.tex0")) iferr_return;
  path.Init("path") iferr_return;
  const GraphNode bundle = inputs.FindChild(tex0) iferr_return;
  if (!bundle.IsValid()) return Role::BASE;
  const GraphNode pathPort = bundle.FindChild(path) iferr_return;
  if (!pathPort.IsValid()) return Role::BASE;
  const maxon::Url url = pathPort.GetPortValue<maxon::Url>().GetValueOr(maxon::Url()) iferr_return;
  const maxon::String name = url.GetName().ToLower();
  if (Has(name, "normal") || Has(name, "nrm") || Has(name, "_nor")) return Role::NORMAL_MAP;
  if (Has(name, "disp")) return Role::DISPLACEMENT_MAP;
  if (Has(name, "bump") || Has(name, "height")) return Role::HEIGHT_MAP;
  if (Has(name, "rough") || Has(name, "gloss")) return Role::ROUGHNESS;
  if (Has(name, "metal")) return Role::METALNESS;
  if (Has(name, "opac") || Has(name, "alpha") || Has(name, "mask") || Has(name, "transp")) return Role::OPACITY;
  if (Has(name, "emis") || Has(name, "emit") || Has(name, "glow")) return Role::EMISSION;
  if (Has(name, "occlusion") || Has(name, "_ao") || Has(name, "ao_") || Has(name, "ambient")) return Role::AO;
  if (Has(name, "spec") || Has(name, "refl")) return Role::REFLECTION;
  return Role::BASE;
}

maxon::Result<Role> SourceRole(const GraphNode& source, const maxon::String& asset) {
  iferr_scope;
  if (asset == Core("bumpmap")) return Role::BUMP;
  if (asset == Core("displacement")) return Role::DISPLACEMENT;
  if (asset == Core("uvcontextprojection")) return Role::UV_CONTEXT;
  if (asset == Core("texturesampler")) return TextureRole(source);
  if (Has(asset, "material")) return Role::SURFACE;  // standard, openpbr, materialblender, ...
  return Role::GENERIC;
}

// Candidate input names on the target for a role; the first free one wins, else the first.
struct Choice { const char* inputs[8] = {}; Int count = 0; const char* helper = nullptr; const char* helperIn = nullptr; const char* helperOut = nullptr; Int helperType = -1; };

void Candidates(Role role, const maxon::String& target, Bool colorSource, Choice& c) {
  auto add = [&c](std::initializer_list<const char*> names) {
    for (const char* n : names) if (c.count < 8) c.inputs[c.count++] = n;
  };
  const Bool standard = target == Core("standardmaterial");
  const Bool openpbr = target == Core("openpbrmaterial");
  if (role == Role::UV_CONTEXT) { add({"uv_context"}); return; }  // only UV Context inputs
  if (target == OutputAsset()) {
    if (role == Role::SURFACE) add({"surface"});
    else if (role == Role::DISPLACEMENT) add({"displacement"});
    else if (role == Role::DISPLACEMENT_MAP || role == Role::HEIGHT_MAP) {
      add({"displacement"});
      c.helper = "displacement"; c.helperIn = "texmap"; c.helperOut = "out";
    }
  } else if (standard || openpbr) {
    switch (role) {
      case Role::BASE: add({"base_color"}); break;
      case Role::ROUGHNESS: add({standard ? "refl_roughness" : "specular_roughness"}); break;
      case Role::METALNESS: add({standard ? "metalness" : "base_metalness"}); break;
      case Role::OPACITY: add({standard ? "opacity_color" : "geometry_opacity"}); break;
      case Role::EMISSION: add({"emission_color"}); break;
      case Role::REFLECTION: add({standard ? "refl_color" : "specular_color"}); break;
      case Role::AO: if (standard) add({"overall_color"}); break;
      case Role::BUMP: add({standard ? "bump_input" : "geometry_normal"}); break;
      case Role::NORMAL_MAP:
      case Role::HEIGHT_MAP:
        add({standard ? "bump_input" : "geometry_normal"});
        c.helper = "bumpmap"; c.helperIn = "input"; c.helperOut = "out";
        c.helperType = role == Role::NORMAL_MAP ? 1 : 0;  // tangent-space normal : height field
        break;
      case Role::GENERIC: if (colorSource) add({"base_color"}); break;
      default: break;
    }
  } else if (target == Core("bumpmap")) {
    add({"input"});
    if (role == Role::NORMAL_MAP) c.helperType = 1;
  } else if (target == Core("displacement")) {
    add({"texmap"});
  } else if (target == Core("materialblender")) {
    if (role == Role::SURFACE) add({"basecolor", "layercolor1", "layercolor2", "layercolor3", "layercolor4", "layercolor5", "layercolor6"});
    else add({"blendcolor1", "blendcolor2", "blendcolor3", "blendcolor4", "blendcolor5", "blendcolor6"});
  } else if (target == Core("rscolorlayer")) {
    add({"base_color", "layer1_color", "layer2_color", "layer3_color", "layer4_color", "layer5_color", "layer6_color", "layer7_color"});
  } else if (target == Core("rscolormix")) {
    add({"input1", "input2"});
  } else if (target == Core("rscolorcorrection")) {
    add({"input"});
  } else if (Has(target, "nodes.core.rsmath")) {
    add({"input1", "input2"});
  }
}

maxon::Result<GraphNode> Port(const GraphNode& list, const maxon::String& asset, const maxon::String& name) {
  iferr_scope;
  maxon::InternedId id;
  id.Init(asset + "."_s + name) iferr_return;
  GraphNode port = list.FindChild(id) iferr_return;
  return port;
}
maxon::Result<Bool> Occupied(const GraphNode& in) {
  iferr_scope;
  Bool any = false;
  in.GetConnections(maxon::PORT_DIR::INPUT, [&any](const maxon::GraphConnection&) -> maxon::Result<Bool> { any = true; return false; }) iferr_return;
  return any;
}

// The node editor places a node it sees appear itself (centred in its view) and
// overwrites any layout record the node had. The helper is therefore placed twice:
// once when it is created, and again once the editor has placed it, without an undo
// entry of its own (see PlacementMessage).
// The record is a copy of an existing one (its view keys are opaque) with only the
// position changed. Graph units; the record y is the top of the port list.
constexpr Int32 kPlacementMessageId = 1072549;
constexpr Float32 kHelperWidth = 128;
constexpr Float32 kGap = 40;
struct Box { Float32 left, top, right, bottom; };
Box Footprint(Float32 x, Float32 y, Float32 width, Float32 below) { return Box{x - 20, y - 120, x + width + 20, y + below}; }
Bool Overlaps(const Box& a, const Box& b) { return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom; }

maxon::Result<maxon::InternedId> LayoutAttribute() {
  iferr_scope;
  maxon::InternedId attribute;
  attribute.Init("widgetDataBlackBox") iferr_return;
  return attribute;
}
maxon::Result<maxon::DataDictionary> LayoutRecord(const GraphNode& node) {
  iferr_scope;
  const maxon::InternedId attribute = LayoutAttribute() iferr_return;
  return node.GetValue<maxon::DataDictionary>(attribute).GetValueOrNull();
}
// Writes `record`'s structure to node with every position replaced. Must run inside
// a graph transaction.
maxon::Result<void> WriteLayout(const GraphNode& node, const maxon::DataDictionary& record, maxon::Vector2d32 position) {
  iferr_scope;
  maxon::DataDictionary placed;
  for (const auto& outer : record) {
    const maxon::Data outerKey = outer.first.GetCopy() iferr_return;
    const maxon::Data value = outer.second.GetCopy() iferr_return;
    const maxon::DataDictionary* views = value.GetPtr<maxon::DataDictionary>();
    if (!views) return maxon::OK;
    maxon::DataDictionary moved;
    for (const auto& item : *views) {
      const maxon::Data key = item.first.GetCopy() iferr_return;
      moved.Set(key, maxon::Tuple<maxon::Vector2d32, Float32>(position, kHelperWidth)) iferr_return;
    }
    placed.Set(outerKey, moved) iferr_return;
  }
  if (placed.IsEmpty()) return maxon::OK;
  const maxon::InternedId attribute = LayoutAttribute() iferr_return;
  maxon::Data data;
  data.Set(std::move(placed)) iferr_return;
  node.SetValue(attribute, maxon::ForwardingDataPtr(std::move(data))) iferr_return;
  return maxon::OK;
}
struct PendingPlacement {
  GraphNode helper;
  maxon::DataDictionary record;
  maxon::Vector2d32 position;
  Int32 since;
  BaseDocument* document;
  Bool resynced = false;
  Int rewrites = 0, resyncs = 0;
};
// Built on first use (see Core above).
maxon::BaseArray<PendingPlacement>& Pending() {
  static maxon::BaseArray<PendingPlacement> pending;
  return pending;
}

maxon::Result<void> PlaceHelper(const GraphNode& helper, const GraphNode& source, const GraphNode& target) {
  iferr_scope;
  iferr (const maxon::BaseArray<wire_native::NodeRecord> s = wire_native::ReadNodePositions(source)) return maxon::OK;
  iferr (const maxon::BaseArray<wire_native::NodeRecord> t = wire_native::ReadNodePositions(target)) return maxon::OK;
  if (s.IsEmpty() || t.IsEmpty()) return maxon::OK;
  // One node slot to the right of the source (the node feeding the port), level with it.
  const Float32 x = s[0].position.x + s[0].width + kGap;
  Float32 y = s[0].position.y;
  // Step up or down, nearest first, until no other node is covered. A free spot more
  // than three node widths away is too far from the source: then the helper stays
  // right of the source even if it covers another node.
  maxon::BaseArray<Box> others;
  const GraphNode container = target.GetParent() iferr_return;
  container.GetChildren([&](const GraphNode& n) -> maxon::Result<Bool> {
    iferr_scope;
    if (n == helper) return true;
    iferr (const maxon::BaseArray<wire_native::NodeRecord> r = wire_native::ReadNodePositions(n)) return true;
    if (!r.IsEmpty()) { others.Append(Footprint(r[0].position.x, r[0].position.y, r[0].width, 160)) iferr_return; }
    return true;
  }, maxon::NODE_KIND::NODE) iferr_return;
  for (Int step = 0; Float32((step + 1) / 2) * kGap <= 3 * kHelperWidth; ++step) {
    const Float32 candidate = y + Float32((step + 1) / 2) * kGap * (step % 2 ? 1.0f : -1.0f);
    const Box box = Footprint(x, candidate, kHelperWidth, 60);
    Bool free = true;
    for (const Box& o : others) if (Overlaps(box, o)) { free = false; break; }
    if (free) { y = candidate; break; }
  }
  const maxon::DataDictionary record = LayoutRecord(target) iferr_return;
  WriteLayout(helper, record, maxon::Vector2d32(x, y)) iferr_return;
  Pending().Append(PendingPlacement{helper, record, maxon::Vector2d32(x, y), GeGetTimer(), GetActiveDocument()}) iferr_return;
  SpecialEventAdd(kPlacementMessageId);
  return maxon::OK;
}

maxon::Result<void> WithoutUndo(const GraphNode& node, const std::function<maxon::Result<void>()>& change) {
  iferr_scope;
  // No undo entry of its own: undoing the connection removes the helper anyway.
  maxon::DataDictionary noUndo;
  noUndo.Set(maxon::nodes::UndoMode, maxon::nodes::UNDO_MODE::NONE) iferr_return;
  maxon::GraphTransaction transaction = node.GetGraph().BeginTransaction(noUndo) iferr_return;
  change() iferr_return;
  transaction.Commit() iferr_return;
  return maxon::OK;
}

// For a few seconds after a helper is added, keeps it where Apply placed it. The
// editor shows the new node at the centre of its view and may also write that spot
// into the layout record; the record is then written back. The editor takes node
// positions from the records only when it resynchronises with the graph, which an
// undo makes it do. (Undoing the step that added the helper would not help: redoing
// it adds the node anew, and the editor centres it again.) So once the editor has
// seen the node, a step that only sets a marker on the helper is added and undone
// right away: the graph is unchanged, the view stays, the step that added the helper
// is the newest undo step again, and the editor shows the helper at its record.
constexpr Int32 kSeenMs = 300;
// Every rewrite and resynchronisation changes the graph, and each graph change makes
// the material recompile; a record the editor keeps changing must not turn into a
// loop of them, so both are capped. Positions within kTolerance count as placed (the
// editor may round them).
constexpr Int kMaxRewrites = 3, kMaxResyncs = 2;
constexpr Float32 kTolerance = 2.0f;
maxon::Result<void> Resynchronise(const GraphNode& helper, BaseDocument* document) {
  iferr_scope;
  maxon::InternedId marker;
  marker.Init("net.nodestroke.placed") iferr_return;
  maxon::GraphTransaction transaction = helper.GetGraph().BeginTransaction() iferr_return;
  maxon::Data value;
  value.Set(true) iferr_return;
  helper.SetValue(marker, maxon::ForwardingDataPtr(std::move(value))) iferr_return;
  transaction.Commit() iferr_return;
  document->DoUndo();
  return maxon::OK;
}
class PlacementMessage final : public MessageData {
public:
  Bool CoreMessage(Int32 id, const BaseContainer&) override {
    if (id != kPlacementMessageId) return true;
    iferr_scope_handler { Pending().Flush(); return true; };
    auto& pending = Pending();
    for (Int i = pending.GetCount() - 1; i >= 0; --i) {
      PendingPlacement& p = pending[i];
      if (!p.helper.IsValid() || GeGetTimer() - p.since > 3000) { pending.Erase(i) iferr_return; continue; }
      iferr (const maxon::BaseArray<wire_native::NodeRecord> now = wire_native::ReadNodePositions(p.helper)) continue;
      const Bool right = !now.IsEmpty() && Abs(now[0].position.x - p.position.x) < kTolerance &&
                         Abs(now[0].position.y - p.position.y) < kTolerance;
      // Shown at its record after a resynchronisation: done.
      if (right && p.resynced) { pending.Erase(i) iferr_return; continue; }
      if (!right) {
        if (p.rewrites >= kMaxRewrites) { pending.Erase(i) iferr_return; continue; }
        // The editor's own record carries its current view keys; prefer it as the template.
        const maxon::DataDictionary own = LayoutRecord(p.helper) iferr_return;
        WithoutUndo(p.helper, [&]() { return WriteLayout(p.helper, own.IsEmpty() ? p.record : own, p.position); }) iferr_return;
        ++p.rewrites;
        p.resynced = false;
      }
      if (p.resynced || GeGetTimer() - p.since < kSeenMs) continue;
      // Only in the document that got the helper, while it is the active one.
      if (p.document != GetActiveDocument() || p.resyncs >= kMaxResyncs) { pending.Erase(i) iferr_return; continue; }
      Resynchronise(p.helper, p.document) iferr_return;
      EventAdd();
      ++p.resyncs;
      p.resynced = true;
    }
    if (!pending.IsEmpty()) SpecialEventAdd(kPlacementMessageId);
    return true;
  }
};
}

maxon::Result<Suggestion> Suggest(const GraphNode& source, const GraphNode& fixedOut, const GraphNode& target) {
  iferr_scope;
  Suggestion result;
  const maxon::String sourceAsset = AssetOf(source) iferr_return;
  const maxon::String targetAsset = AssetOf(target) iferr_return;
  GraphNode out = fixedOut;
  if (!out.IsValid()) {
    const GraphNode outputs = source.GetOutputs() iferr_return;
    const maxon::BaseArray<GraphNode> ports = wire_ops::EditorOrderPorts(outputs, maxon::NODE_KIND::OUTPORT) iferr_return;
    if (ports.IsEmpty()) return result;
    out = ports[0];
  }
  const maxon::DataType outType = out.GetValue(maxon::nodes::PortType).GetOrDefault() iferr_return;
  const Bool colorSource = outType && Has(outType.GetId().ToString(), "col<");
  const Role role = SourceRole(source, sourceAsset) iferr_return;
  Choice choice;
  Candidates(role, targetAsset, colorSource, choice);
  const GraphNode inputs = target.GetInputs() iferr_return;
  GraphNode first;
  for (Int i = 0; i < choice.count; ++i) {
    const char* name = choice.inputs[i];
    const GraphNode in = Port(inputs, targetAsset, maxon::String(name)) iferr_return;
    if (!in.IsValid()) continue;
    if (!choice.helper) {
      const Bool ok = out.IsConnectable(in) iferr_return;
      if (!ok) continue;
    }
    if (!first.IsValid()) first = in;
    const Bool occupied = Occupied(in) iferr_return;
    if (!occupied) { first = in; break; }
  }
  if (!first.IsValid()) return result;
  result.out = out;
  result.in = first;
  result.helperInputType = choice.helperType;
  if (choice.helper) {
    result.helperAsset = Core(choice.helper);
    result.helperIn = maxon::String(choice.helperIn);
    result.helperOut = maxon::String(choice.helperOut);
  }
  return result;
}

maxon::Result<Suggestion> Exact(const GraphNode& out, const GraphNode& in) {
  iferr_scope;
  Suggestion result;
  result.out = out;
  result.in = in;
  const maxon::String port = in.GetId().ToString();
  const GraphNode target = in.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
  const maxon::String targetAsset = AssetOf(target) iferr_return;
  const Bool bump = port.EndsWith(".bump_input"_s) || port.EndsWith(".geometry_normal"_s);
  const Bool displacement = targetAsset == OutputAsset() && port.EndsWith(".displacement"_s);
  if (!bump && !displacement) return result;
  const maxon::String helper = bump ? Core("bumpmap") : Core("displacement");
  // Look upstream (a few levels) for a node of the helper's kind.
  const GraphNode source = out.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
  maxon::BaseArray<GraphNode> frontier, seen;
  frontier.Append(source) iferr_return;
  for (Int depth = 0; depth < 8 && !frontier.IsEmpty(); ++depth) {
    maxon::BaseArray<GraphNode> next;
    for (const GraphNode& n : frontier) {
      const maxon::String asset = AssetOf(n) iferr_return;
      if (asset == helper) return result;
      Bool known = false;
      for (const GraphNode& m : seen) if (m == n) known = true;
      if (known) continue;
      seen.Append(n) iferr_return;
      maxon::GraphModelHelper::GetDirectPredecessors(n, maxon::NODE_KIND::NODE, [&next](const GraphNode& p) -> maxon::Result<Bool> {
        iferr_scope;
        next.Append(p) iferr_return;
        return true;
      }) iferr_return;
    }
    frontier = std::move(next);
  }
  result.helperAsset = helper;
  result.helperIn = bump ? "input"_s : "texmap"_s;
  result.helperOut = "out"_s;
  if (bump) {
    const maxon::String sourceAsset = AssetOf(source) iferr_return;
    const Role role = SourceRole(source, sourceAsset) iferr_return;
    result.helperInputType = role == Role::NORMAL_MAP ? 1 : 0;  // tangent-space normal : height field
  }
  return result;
}

maxon::Result<void> Apply(const maxon::nodes::NodesGraphModelRef& graph, const Suggestion& s) {
  iferr_scope;
  if (s.helperAsset.IsEmpty()) {
    wire_ops::ConnectReplacing(s.out, s.in) iferr_return;
    if (s.helperInputType >= 0) {
      // Target is itself a Bump Map: set its input type to match the texture.
      const GraphNode bump = s.in.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
      const GraphNode inputs = bump.GetInputs() iferr_return;
      const GraphNode type = Port(inputs, Core("bumpmap"), "inputtype"_s) iferr_return;
      if (type.IsValid()) { type.SetPortValue(maxon::Int64(s.helperInputType)) iferr_return; }
    }
    return maxon::OK;
  }
  BaseDocument* doc = GetActiveDocument();
  if (!doc) return maxon::NullptrError(MAXON_SOURCE_LOCATION);
  const maxon::AssetRepositoryRef& repository = doc->GetSceneRepository(true) iferr_return;
  maxon::Id assetId;
  assetId.Init(s.helperAsset) iferr_return;
  const maxon::nodes::NodeTemplate nodeTemplate = maxon::nodes::NodesLib::LoadTemplate(repository, assetId) iferr_return;
  // AddChild adds to the root of a graph view; for a target inside a group, use a
  // view rooted at that group and find the new node again in the main graph.
  const GraphNode target = s.in.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
  const GraphNode container = target.GetParent() iferr_return;
  GraphNode helper;
  if (container == graph.GetViewRoot()) {
    helper = graph.AddChild(maxon::Id(), nodeTemplate) iferr_return;
  } else {
    maxon::nodes::NodesGraphModelRef group = graph.CreateView(maxon::nodes::NodeSystemManagerInterface::FILTER::NONE, container.GetPath()) iferr_return;
    maxon::GraphTransaction transaction = group.BeginTransaction() iferr_return;
    const GraphNode added = group.AddChild(maxon::Id(), nodeTemplate) iferr_return;
    const maxon::NodePath path = added.GetPath();
    transaction.Commit() iferr_return;
    helper = graph.GetNode(path);
    if (!helper.IsValid())
      return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_HELPER_GROUP));
  }
  const GraphNode helperInputs = helper.GetInputs() iferr_return;
  const GraphNode helperOutputs = helper.GetOutputs() iferr_return;
  const GraphNode in = Port(helperInputs, s.helperAsset, s.helperIn) iferr_return;
  const GraphNode out = Port(helperOutputs, s.helperAsset, s.helperOut) iferr_return;
  if (!in.IsValid() || !out.IsValid())
    return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_HELPER_PORTS));
  wire_ops::ConnectReplacing(s.out, in) iferr_return;
  wire_ops::ConnectReplacing(out, s.in) iferr_return;
  const GraphNode source = s.out.GetAncestor(maxon::NODE_KIND::NODE) iferr_return;
  PlaceHelper(helper, source, target) iferr_return;
  if (s.helperInputType >= 0) {
    const GraphNode type = Port(helperInputs, s.helperAsset, "inputtype"_s) iferr_return;
    if (type.IsValid()) { type.SetPortValue(maxon::Int64(s.helperInputType)) iferr_return; }
  }
  return maxon::OK;
}

Bool RegisterPlacement() {
  return RegisterMessagePlugin(kPlacementMessageId, "NodeStroke Placement"_s, 0, NewObjClear(PlacementMessage));
}
void EndPlacement() { Pending().Reset(); }
}
