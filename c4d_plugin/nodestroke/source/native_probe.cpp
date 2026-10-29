#include "c4d.h"
#include "native_layout.h"
#include "maxon/graph_helper.h"
#include "maxon/nodesgraph.h"
#include "maxon/datadictionary.h"
using namespace cinema;
void InspectNativeWindows();
bool ToggleNativeInputProbe();
namespace {
constexpr Int32 kProbeId = 1069995;
constexpr Int32 kInputProbeId = 1069994;
maxon::Result<void> Probe(BaseDocument* doc) {
 iferr_scope;
 if (!doc || !doc->GetActiveMaterial()) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"Select a node material first."_s);
 NodeMaterial* mat=static_cast<NodeMaterial*>(doc->GetActiveMaterial());
 const maxon::Id space=GetActiveNodeSpaceId();
 if (!mat->HasSpace(space)) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"The selected material has no graph in the active node space."_s);
 const maxon::nodes::NodesGraphModelRef& graph=mat->GetGraph(space) iferr_return;
 maxon::InternedId id;
 id.Init("widgetDataBlackBoxOut") iferr_return;
 const maxon::DataDictionary data=graph.GetViewRoot().GetValue<maxon::DataDictionary>(id).GetValueOrNull() iferr_return;
 const maxon::BaseArray<wire_native::ViewRecord> records=wire_native::ReadViews(graph.GetViewRoot()) iferr_return;
 for (const auto& r : records) {
   ApplicationOutput("Safe view reader key=@ zoom=@ offset=(@,@)"_s,r.key,r.zoom,r.offset.x,r.offset.y);
 }
 graph.GetViewRoot().GetChildren([](const maxon::GraphNode& node) -> maxon::Result<Bool> {
   iferr_scope;
   const maxon::BaseArray<wire_native::NodeRecord> positions=wire_native::ReadNodePositions(node) iferr_return;
   for (const auto& r : positions) {
     ApplicationOutput("Safe node reader node=@ key=@ pos=(@,@) width=@"_s,node.GetPath(),r.key,r.position.x,r.position.y,r.width);
   }
   return true;
 },maxon::NODE_KIND::NODE) iferr_return;
 ApplicationOutput("NodeStroke probe: C4D @, space @, view data @"_s,GetC4DVersion(),space,data);
 InspectNativeWindows();
 for (const auto& outer : data) {
   const maxon::Data key=outer.first.GetCopy() iferr_return;
   const maxon::Data value=outer.second.GetCopy() iferr_return;
   ApplicationOutput("View entry key=@ keytype=@ value=@ valuetype=@"_s,key,key.GetType(),value,value.GetType());
   iferr (const maxon::DataDictionary inner=value.Get<maxon::DataDictionary>()) { continue; }
   for (const auto& entry : inner) {
     const maxon::Data ikey=entry.first.GetCopy() iferr_return;
     const maxon::Data ivalue=entry.second.GetCopy() iferr_return;
     ApplicationOutput("  item key=@ keytype=@ value=@ valuetype=@"_s,ikey,ikey.GetType(),ivalue,ivalue.GetType());
     using ViewPair=maxon::Tuple<maxon::Float32, maxon::Vector2d32>;
     const ViewPair* view=ivalue.GetPtr<ViewPair>();
     if (view) {
       ApplicationOutput("  typed view zoom=@ x=@ y=@"_s,view->first,view->second.x,view->second.y);
     } else {
       ApplicationOutput("  typed view: incompatible data type"_s);
     }
   }
 }
 return maxon::OK;
}
class ProbeCommand : public CommandData {
 Bool Execute(BaseDocument* doc,GeDialog*) override {
  iferr_scope_handler { MessageDialog(err.GetMessage()); return false; };
  Probe(doc) iferr_return;
  MessageDialog("Read-only view records written to the Cinema 4D Console."_s); return true;
 }
};
class InputProbeCommand : public CommandData {
 Bool Execute(BaseDocument*, GeDialog*) override {
  const bool enabled = ToggleNativeInputProbe();
  MessageDialog(enabled ? "Read-only mouse-message observation enabled (up to 32 clicks). Run again to stop."_s
                        : "Mouse-message observation stopped or unavailable."_s);
  return true;
 }
};
}
Bool RegisterNativeViewProbe() {
 if (!RegisterCommandPlugin(kProbeId,"NodeStroke: Inspect View Records"_s,PLUGINFLAG_HIDE,nullptr,"Read-only experimental 2027 node-view diagnostics."_s,NewObjClear(ProbeCommand))) return false;
 return RegisterCommandPlugin(kInputProbeId,"NodeStroke: Toggle Input Probe"_s,PLUGINFLAG_HIDE,nullptr,"Temporarily observe mouse messages without consuming them."_s,NewObjClear(InputProbeCommand));
}


