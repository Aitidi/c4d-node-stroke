#include "native_layout.h"
#include <cmath>
namespace wire_native {
using namespace cinema;
namespace {
template <typename Pair,typename Record,typename MakeRecord,typename Validate>
maxon::Result<maxon::BaseArray<Record>> ReadRecords(const maxon::GraphNode& node,const char* attribute,MakeRecord make,Validate validate) {
 iferr_scope;
 if (GetC4DVersion()!=2027000)
   return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"Unsupported Cinema 4D version for experimental layout reader."_s);
 if (!node.IsValid()) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"Invalid graph node."_s);
 maxon::BaseArray<Record> out;
 maxon::InternedId attr;
 attr.Init(attribute) iferr_return;
 const maxon::DataDictionary outer=node.GetValue<maxon::DataDictionary>(attr).GetValueOrNull() iferr_return;
 for (const auto& parent : outer) {
   const maxon::Data outerKey=parent.first.GetCopy() iferr_return;
   const maxon::Id* key=outerKey.GetPtr<maxon::Id>();
   if (!key) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"Unexpected layout key type."_s);
   const maxon::Data value=parent.second.GetCopy() iferr_return;
   const maxon::DataDictionary* inner=value.GetPtr<maxon::DataDictionary>();
   if (!inner) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"Unexpected layout dictionary type."_s);
   Int count=0;
   for (const auto& item : *inner) {
     const maxon::Data data=item.second.GetCopy() iferr_return;
     const Pair* pair=data.GetPtr<Pair>();
     if (!pair) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"Unexpected layout value type."_s);
     Record record=make(*key,*pair);
     if (!validate(record)) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"Invalid layout values."_s);
     out.Append(record) iferr_return;
     ++count;
   }
   if (count!=1) return maxon::IllegalStateError(MAXON_SOURCE_LOCATION,"Unexpected layout record count."_s);
 }
 return out;
}
}
maxon::Result<maxon::BaseArray<ViewRecord>> ReadViews(const maxon::GraphNode& root) {
 using Pair=maxon::Tuple<maxon::Float32,maxon::Vector2d32>;
 return ReadRecords<Pair,ViewRecord>(root,"widgetDataBlackBoxOut",
   [](const maxon::Id& k,const Pair& p) { return ViewRecord{k,p.first,p.second}; },
   [](const ViewRecord& r) { return std::isfinite(r.zoom) && r.zoom>0 && std::isfinite(r.offset.x) && std::isfinite(r.offset.y); });
}
maxon::Result<maxon::BaseArray<NodeRecord>> ReadNodePositions(const maxon::GraphNode& node) {
 using Pair=maxon::Tuple<maxon::Vector2d32,maxon::Float32>;
 return ReadRecords<Pair,NodeRecord>(node,"widgetDataBlackBox",
   [](const maxon::Id& k,const Pair& p) { return NodeRecord{k,p.first,p.second}; },
   [](const NodeRecord& r) { return std::isfinite(r.position.x) && std::isfinite(r.position.y) && std::isfinite(r.width) && r.width>0; });
}
}
