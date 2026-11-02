#include "c4d.h"
#include "c4d_resource.h"
#include "maxon/graph_helper.h"
#include "maxon/nodesgraph.h"
#include "maxon/node_undo.h"
#include "graph_ops.h"
#include "wire_prefs.h"
#include "rs_rules.h"
#include "../res/c4d_symbols.h"

using namespace cinema;
cinema::Bool RegisterCanvasGestures();
cinema::Bool RegisterRulesDialog();
void StartCanvasGestures();
void StopCanvasGestures();
namespace {
// Plugin ID registered with Maxon (Plugin Cafe, account aitidi).
constexpr Int32 kDisconnectId = 1072552;
constexpr Int32 kCutId = 1072553;
constexpr Int32 kConnectId = 1072551;
constexpr Int32 kExtractId = 1072554;

maxon::Result<void> ApplySelection(BaseDocument* doc, Int32 mode)
{
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
  if (mode == 2) {
    maxon::BaseArray<maxon::GraphNode> ports;
    maxon::GraphModelHelper::GetSelectedNodes(graph, maxon::NODE_KIND::PORT_MASK, [&ports](const maxon::GraphNode& port) -> maxon::Result<Bool> {
      iferr_scope;
      ports.Append(port) iferr_return;
      return true;
    }) iferr_return;
    if (ports.GetCount() != 2)
      return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_SELECT_PORTS));
    maxon::GraphNode source, target;
    for (const maxon::GraphNode& port : ports) {
      if (port.GetKind() == maxon::NODE_KIND::OUTPORT) source = port;
      if (port.GetKind() == maxon::NODE_KIND::INPORT) target = port;
    }
    if (!source.IsValid() || !target.IsValid())
      return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_SELECT_PORTS));
    const Bool allowed = source.IsConnectable(target) iferr_return;
    if (!allowed)
      return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_NOT_CONNECTABLE));
    maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
    wire_ops::ConnectReplacing(source, target) iferr_return;
    transaction.Commit() iferr_return;
  } else if (mode == 1) {
    struct Connection { maxon::GraphNode source; maxon::GraphNode target; };
    maxon::BaseArray<Connection> selected;
    maxon::GraphModelHelper::GetSelectedConnections(graph, [&selected](const maxon::GraphNode& source, const maxon::GraphNode& target, const maxon::Wires&) -> maxon::Result<Bool> {
      iferr_scope;
      selected.Append(Connection{source, target}) iferr_return;
      return true;
    }) iferr_return;
    if (selected.IsEmpty())
      return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_SELECT_WIRES));
    maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
    for (const Connection& c : selected) {
      maxon::GraphModelHelper::RemoveConnection(c.source, c.target) iferr_return;
    }
    transaction.Commit() iferr_return;
  } else {
    maxon::BaseArray<maxon::GraphNode> selected;
    maxon::GraphModelHelper::GetSelectedNodes(graph, maxon::NODE_KIND::NODE, [&selected](const maxon::GraphNode& node) -> maxon::Result<Bool> {
      iferr_scope;
      selected.Append(node) iferr_return;
      return true;
    }) iferr_return;
    if (selected.IsEmpty())
      return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, GeLoadString(IDS_ERR_SELECT_NODES));
    maxon::GraphTransaction transaction = graph.BeginTransaction() iferr_return;
    for (const maxon::GraphNode& node : selected) {
      if (mode == 3) { wire_ops::ExtractNode(node) iferr_return; }
      else { maxon::GraphModelHelper::RemoveAllConnections(node) iferr_return; }
    }
    transaction.Commit() iferr_return;
  }
  EventAdd();
  return maxon::OK;
}
class SelectionCommand final : public CommandData {
  Int32 _mode;
public:
  explicit SelectionCommand(Int32 mode) : _mode(mode) {}
public:
  Bool Execute(BaseDocument* doc, GeDialog*) override {
    iferr_scope_handler {
      MessageDialog(err.GetMessage());
      return false;
    };
    ApplySelection(doc, _mode) iferr_return;
    return true;
  }
};

// Extensions > NodeStroke with the plugin icon: the gesture switch and the settings.
// Cinema 4D's own per-folder plugin submenus can't show an icon, so the commands
// hide from them (PLUGINFLAG_HIDEPLUGINMENU) and are listed here.
constexpr Int32 kToggleId = 1072550;  // canvas_gesture.cpp
BaseContainer NodeStrokeMenu() {
  BaseContainer sub;
  sub.InsData(MENURESOURCE_SUBTITLE, "NodeStroke"_s);
  sub.InsData(MENURESOURCE_SUBTITLE_ICONID, kToggleId);
  for (const Int32 id : {kToggleId, kSettingsCommandId})
    sub.InsData(MENURESOURCE_COMMAND, FormatString("PLUGIN_CMD_@", id));
  return sub;
}
// Plugin managers such as Boghma build a NodeStroke submenu of their own from the
// plugin folder's visible plugins, which hides our commands and has no icon. Gives
// those submenus our icon and commands. Returns the number repaired.
Int FixForeignMenus(BaseContainer& menu) {
  Int fixed = 0;
  for (Int32 i = 0; menu.GetIndexId(i) != NOTOK; ++i) {
    const GeData* data = menu.GetIndexData(i);
    if (!data || data->GetType() != DA_CONTAINER) continue;
    BaseContainer sub = *data->GetContainer();
    if (sub.GetString(MENURESOURCE_SUBTITLE).StartsWith("NodeStroke"_s) && sub.GetInt32(MENURESOURCE_SUBTITLE_ICONID) != kToggleId) {
      menu.SetIndexData(i, GeData(NodeStrokeMenu()));
      ++fixed;
    } else if (const Int count = FixForeignMenus(sub)) {
      menu.SetIndexData(i, GeData(sub));
      fixed += count;
    }
  }
  return fixed;
}
void BuildMenu() {
  BaseContainer* main = GetMenuResource("M_EDITOR"_s);
  if (!main) return;
  FixForeignMenus(*main);
  for (Int32 i = 0; main->GetIndexId(i) != NOTOK; ++i) {
    const GeData* data = main->GetIndexData(i);
    if (!data || data->GetType() != DA_CONTAINER || data->GetContainer()->GetString(MENURESOURCE_SUBTITLE) != "IDS_EDITOR_PIPELINE"_s) continue;
    BaseContainer extensions = *data->GetContainer();
    extensions.InsData(MENURESOURCE_SUBMENU, GeData(NodeStrokeMenu()));
    main->SetIndexData(i, GeData(extensions));
    return;
  }
  main->InsData(MENURESOURCE_SUBMENU, GeData(NodeStrokeMenu()));  // no Extensions menu
}
}
Bool cinema::PluginStart() {
  if (!RegisterWirePrefs()) return false;
  if (!RegisterRulesDialog()) return false;
  if (!wire_rs::RegisterPlacement()) return false;
  if (!RegisterGraphOps()) return false;
  if (!RegisterCanvasGestures()) return false;
  if (!RegisterCommandPlugin(kDisconnectId, GeLoadString(IDS_CMD_DISCONNECT), PLUGINFLAG_HIDEPLUGINMENU, AutoBitmap("icon.png"_s),
    GeLoadString(IDS_CMD_DISCONNECT_HELP), NewObjClear(SelectionCommand, 0))) return false;
  if (!RegisterCommandPlugin(kCutId, GeLoadString(IDS_CMD_CUT), PLUGINFLAG_HIDEPLUGINMENU, AutoBitmap("icon.png"_s),
    GeLoadString(IDS_CMD_CUT_HELP), NewObjClear(SelectionCommand, 1))) return false;
  if (!RegisterCommandPlugin(kExtractId, GeLoadString(IDS_CMD_EXTRACT), PLUGINFLAG_HIDEPLUGINMENU, AutoBitmap("icon.png"_s),
    GeLoadString(IDS_CMD_EXTRACT_HELP), NewObjClear(SelectionCommand, 3))) return false;
  return RegisterCommandPlugin(kConnectId, GeLoadString(IDS_CMD_CONNECT), PLUGINFLAG_HIDEPLUGINMENU, AutoBitmap("icon.png"_s),
    GeLoadString(IDS_CMD_CONNECT_HELP), NewObjClear(SelectionCommand, 2));
}
void cinema::PluginEnd() {
  StopCanvasGestures();
  wire_rs::EndPlacement();
}
Bool cinema::PluginMessage(Int32 id, void*) {
  if (id == C4DPL_INIT_SYS) return g_resource.Init();
  if (id == C4DPL_BUILDMENU) { BuildMenu(); return true; }
  if (id == C4DPL_PROGRAM_STARTED) {
    // Other plugin managers may build their menus after ours.
    BaseContainer* main = GetMenuResource("M_EDITOR"_s);
    if (main && FixForeignMenus(*main)) UpdateMenus();
    StartCanvasGestures();
    return true;
  }
  return false;
}
