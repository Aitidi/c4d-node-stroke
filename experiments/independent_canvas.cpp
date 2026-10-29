#include "c4d.h"
#include "maxon/graph_helper.h"
#include "maxon/nodesgraph.h"

using namespace cinema;
namespace {
constexpr Int32 kCanvasId = 1069996;
constexpr Int32 kRefresh = 1001, kCut = 1002, kConnect = 1003, kArea = 1004, kScroll = 1005;
struct Point { Int32 x = 0, y = 0; };
struct PortView { maxon::GraphNode node; Point at; Bool output = false; };
struct NodeView { maxon::GraphNode node; Int32 x = 0, y = 0, h = 0; };
struct WireView { maxon::GraphNode source, target; Point a, b; };
static Int64 Cross(Point a, Point b, Point c) { return Int64(b.x-a.x)*(c.y-a.y)-Int64(b.y-a.y)*(c.x-a.x); }
static Bool On(Point a, Point b, Point p) {
  return Cross(a,b,p)==0 && p.x>=Min(a.x,b.x) && p.x<=Max(a.x,b.x) && p.y>=Min(a.y,b.y) && p.y<=Max(a.y,b.y);
}
static Bool Intersects(Point a, Point b, Point c, Point d) {
  Int64 x=Cross(a,b,c), y=Cross(a,b,d), z=Cross(c,d,a), w=Cross(c,d,b);
  return (((x>0 && y<0)||(x<0 && y>0)) && ((z>0 && w<0)||(z<0 && w>0))) || On(a,b,c)||On(a,b,d)||On(c,d,a)||On(c,d,b);
}
class Canvas : public GeUserArea {
public:
  maxon::BaseArray<NodeView> nodes;
  maxon::BaseArray<PortView> ports;
  maxon::BaseArray<WireView> wires;
  maxon::BaseArray<Point> stroke;
  maxon::nodes::NodesGraphModelRef graph;
  BaseDocument* doc = nullptr;
  BaseMaterial* material = nullptr;
  maxon::TimeStamp stamp;
  Bool cutMode = true;
  Int32 contentHeight = 500;

  maxon::Result<void> Refresh() {
    iferr_scope;
    graph = maxon::nodes::NodesGraphModelRef();
    doc = nullptr; material = nullptr;
    nodes.Reset(); ports.Reset(); wires.Reset(); stroke.Reset(); Redraw();
    doc = GetActiveDocument();
    if (!doc || !doc->GetActiveMaterial()) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, "Select a material in the Material Manager first."_s);
    material = doc->GetActiveMaterial();
    NodeMaterial* nm = static_cast<NodeMaterial*>(material);
    const maxon::Id space = GetActiveNodeSpaceId();
    if (!nm->HasSpace(space)) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, "The material has no graph in the active node space."_s);
    graph = nm->GetGraph(space) iferr_return;
    nodes.Reset(); ports.Reset(); wires.Reset(); stroke.Reset();
    maxon::BaseArray<maxon::GraphNode> raw;
    graph.GetViewRoot().GetChildren([&raw](const maxon::GraphNode& n) -> maxon::Result<Bool> {
      iferr_scope;
      raw.Append(n) iferr_return;
      return true;
    }, maxon::NODE_KIND::NODE) iferr_return;
    Int32 y=30;
    for (const maxon::GraphNode& n : raw) {
      maxon::BaseArray<maxon::GraphNode> ins, outs;
      n.GetInputs().GetChildren([&ins](const maxon::GraphNode& p) -> maxon::Result<Bool> { iferr_scope; ins.Append(p) iferr_return; return true; }) iferr_return;
      n.GetOutputs().GetChildren([&outs](const maxon::GraphNode& p) -> maxon::Result<Bool> { iferr_scope; outs.Append(p) iferr_return; return true; }) iferr_return;
      Int32 height=38+Int32(Max(ins.GetCount(),outs.GetCount()))*20;
      nodes.Append(NodeView{n, 280, y, height}) iferr_return;
      Int32 i=0;
      for (const maxon::GraphNode& p : ins) { ports.Append(PortView{p,{280,y+35+i++*20},false}) iferr_return; }
      i=0;
      for (const maxon::GraphNode& p : outs) { ports.Append(PortView{p,{560,y+35+i++*20},true}) iferr_return; }
      y+=height+55;
    }
    contentHeight=Max(500,y+30);
    for (const PortView& out : ports) {
      if (!out.output) continue;
      out.node.GetConnections(maxon::PORT_DIR::OUTPUT, [this,&out](const maxon::GraphConnection& connection) -> maxon::Result<Bool> {
        iferr_scope;
        for (const PortView& in : ports) {
          if (!in.output && in.node == connection.first) {
            wires.Append(WireView{out.node,in.node,out.at,in.at}) iferr_return;
          }
        }
        return true;
      }) iferr_return;
    }
    stamp=graph.GetModificationStamp();
    Redraw();
    return maxon::OK;
  }
  Bool GetMinSize(Int32& w, Int32& h) override { w=850; h=contentHeight; return true; }
  void DrawMsg(Int32 x1, Int32 y1, Int32 x2, Int32 y2, const BaseContainer&) override {
    OffScreenOn(); SetClippingRegion(x1,y1,x2,y2);
    DrawSetPen(Vector(.10)); DrawRectangle(x1,y1,x2,y2);
    DrawSetTextCol(Vector(1.0),COLOR_TRANS); DrawText(cutMode ? "CUT MODE"_s : "CONNECT MODE"_s, 12, 8);
    DrawSetPen(Vector(.9,.65,.18));
    for (const WireView& wire : wires) DrawLine(wire.a.x,wire.a.y,wire.b.x,wire.b.y);
    for (const NodeView& n : nodes) {
      DrawSetPen(Vector(.20)); DrawRectangle(n.x,n.y,n.x+280,n.y+n.h);
      DrawSetPen(Vector(.25,.37,.52)); DrawRectangle(n.x,n.y,n.x+280,n.y+24);
      DrawSetTextCol(Vector(1.0),COLOR_TRANS);
      DrawText(n.node.GetId().ToString(),n.x+6,n.y+4);
    }
    for (const PortView& p : ports) {
      DrawSetPen(p.output ? Vector(.85,.68,.3) : Vector(.45,.75,.85));
      DrawRectangle(p.at.x-4,p.at.y-4,p.at.x+4,p.at.y+4);
      DrawSetTextCol(Vector(.85),COLOR_TRANS);
      DrawText(p.node.GetId().ToString(),p.output ? p.at.x+8 : p.at.x-240,p.at.y-8);
    }
    DrawSetPen(Vector(1,.25,.2));
    for (Int i=1; i<stroke.GetCount(); ++i) DrawLine(stroke[i-1].x,stroke[i-1].y,stroke[i].x,stroke[i].y);
  }
  maxon::Result<void> Apply() {
    iferr_scope;
    if (!doc || !graph || doc!=GetActiveDocument() || doc->GetActiveMaterial()!=material || graph.GetModificationStamp()!=stamp)
      return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, "The material or graph changed. Refresh the canvas and try again."_s);
    if (stroke.GetCount()<2) return maxon::OK;
    if (cutMode) {
      maxon::BaseArray<WireView> hits;
      for (const WireView& wire : wires) {
        for (Int i=1; i<stroke.GetCount(); ++i) {
          if (Intersects(stroke[i-1],stroke[i],wire.a,wire.b)) { hits.Append(wire) iferr_return; break; }
        }
      }
      if (hits.IsEmpty()) return maxon::OK;
      maxon::GraphTransaction tx=graph.BeginTransaction() iferr_return;
      for (const WireView& wire : hits) { maxon::GraphModelHelper::RemoveConnection(wire.source,wire.target) iferr_return; }
      tx.Commit() iferr_return;
    } else {
      const PortView* out=nullptr; const PortView* in=nullptr;
      Point first=stroke[0], last=stroke[stroke.GetCount()-1];
      for (const PortView& p : ports) {
        Int64 dx=Int64(p.at.x-first.x), dy=Int64(p.at.y-first.y);
        if (p.output && dx*dx+dy*dy<=144) out=&p;
        dx=Int64(p.at.x-last.x); dy=Int64(p.at.y-last.y);
        if (!p.output && dx*dx+dy*dy<=144) in=&p;
      }
      if (!out || !in) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, "Draw from an output square to an input square."_s);
      Bool allowed=out->node.IsConnectable(in->node) iferr_return;
      if (!allowed) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, "These ports cannot be connected."_s);
      maxon::GraphTransaction tx=graph.BeginTransaction() iferr_return;
      out->node.Connect(in->node) iferr_return;
      tx.Commit() iferr_return;
    }
    EventAdd(); Refresh() iferr_return;
    return maxon::OK;
  }
  Bool InputEvent(const BaseContainer& msg) override {
    if (msg.GetInt32(BFM_INPUT_DEVICE)!=BFM_INPUT_MOUSE || msg.GetInt32(BFM_INPUT_CHANNEL)!=BFM_INPUT_MOUSELEFT) return false;
    iferr_scope_handler { MessageDialog(err.GetMessage()); stroke.Reset(); Redraw(); return true; };
    stroke.Reset();
    Int32 x=msg.GetInt32(BFM_INPUT_X), y=msg.GetInt32(BFM_INPUT_Y); Global2Local(&x,&y);
    stroke.Append(Point{x,y}) iferr_return;
    BaseContainer state;
    Bool haveState = false;
    while ((haveState = GetInputState(BFM_INPUT_MOUSE,BFM_INPUT_MOUSELEFT,state)) && state.GetInt32(BFM_INPUT_VALUE)) {
      x=state.GetInt32(BFM_INPUT_X); y=state.GetInt32(BFM_INPUT_Y); Global2Local(&x,&y);
      const Point& p=stroke[stroke.GetCount()-1];
      if (p.x!=x || p.y!=y) { stroke.Append(Point{x,y}) iferr_return; Redraw(); }
    }
    if (haveState) {
      x=state.GetInt32(BFM_INPUT_X); y=state.GetInt32(BFM_INPUT_Y); Global2Local(&x,&y);
      stroke.Append(Point{x,y}) iferr_return;
    }
    Apply() iferr_return;
    stroke.Reset(); Redraw(); return true;
  }
};
class CanvasDialog : public GeDialog {
  Canvas area;
public:
  Bool CreateLayout() override {
    SetTitle("NodeStroke - Experimental Canvas"_s);
    GroupBegin(0,BFH_SCALEFIT,3,0,String(),0);
    AddButton(kRefresh,BFH_LEFT,0,0,"Refresh material"_s);
    AddButton(kCut,BFH_LEFT,0,0,"Cut mode"_s);
    AddButton(kConnect,BFH_LEFT,0,0,"Connect mode"_s);
    GroupEnd();
    AddStaticText(0,BFH_LEFT,0,0,"Cut: cross wires. Connect: drag output square to input square. Refresh after graph changes."_s,0);
    ScrollGroupBegin(kScroll,BFH_SCALEFIT|BFV_SCALEFIT,SCROLLGROUP_VERT|SCROLLGROUP_HORIZ);
    AddUserArea(kArea,BFH_SCALEFIT|BFV_SCALEFIT,850,500); AttachUserArea(area,kArea);
    GroupEnd(); return true;
  }
  Bool Command(Int32 id,const BaseContainer&) override {
    if (id==kCut) { area.cutMode=true; area.Redraw(); return true; }
    if (id==kConnect) { area.cutMode=false; area.Redraw(); return true; }
    if (id==kRefresh) {
      iferr_scope_handler { MessageDialog(err.GetMessage()); return true; };
      area.Refresh() iferr_return;
      LayoutChanged(kScroll); return true;
    }
    return true;
  }
};
class CanvasCommand : public CommandData {
  CanvasDialog dialog;
public:
  Bool Execute(BaseDocument*,GeDialog*) override { return dialog.Open(DLG_TYPE::ASYNC,kCanvasId,-1,-1,1000,700); }
  Bool RestoreLayout(void* secret) override { return dialog.RestoreLayout(kCanvasId,0,secret); }
};
}
Bool RegisterWireCanvas() {
  return RegisterCommandPlugin(kCanvasId,"NodeStroke: Experimental Canvas"_s,0,nullptr,"Open a separate graph canvas for cutting and connecting wires."_s,NewObjClear(CanvasCommand));
}



