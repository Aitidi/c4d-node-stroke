#include "view_tracking.h"
#include <algorithm>
#include <cmath>
namespace nodestroke {
namespace {
bool close(const ViewRecord& a,const ViewRecord& b,double e) {
 return std::abs(a.zoom-b.zoom)<=e && std::abs(a.offset_x-b.offset_x)<=e && std::abs(a.offset_y-b.offset_y)<=e;
}
bool valid(const ViewRecord& r) {
 return std::isfinite(r.zoom) && std::isfinite(r.offset_x) && std::isfinite(r.offset_y) && r.zoom>0;
}
}
std::optional<ViewChange> identify_view_change(const std::vector<ViewRecord>& before, const std::vector<ViewRecord>& after, double tolerance) {
 if (before.size()!=after.size() || before.empty() || !std::isfinite(tolerance) || tolerance<0) return {};
 for (const auto& r:before) if (!valid(r)) return {};
 for (const auto& r:after) if (!valid(r)) return {};
 // Duplicates make record identity ambiguous, even if just one instance moved.
 for (std::size_t i=0;i<before.size();++i) for (std::size_t j=i+1;j<before.size();++j) if (close(before[i],before[j],tolerance)) return {};
 for (std::size_t i=0;i<after.size();++i) for (std::size_t j=i+1;j<after.size();++j) if (close(after[i],after[j],tolerance)) return {};
 std::vector<bool> matched_after(after.size(),false);
 std::vector<std::size_t> unmatched_before;
 for (std::size_t i=0;i<before.size();++i) {
   std::size_t match=after.size();
   for (std::size_t j=0;j<after.size();++j) if (close(before[i],after[j],tolerance)) {
     if (match!=after.size() || matched_after[j]) return {};
     match=j;
   }
   if (match==after.size()) unmatched_before.push_back(i); else matched_after[match]=true;
 }
 if (unmatched_before.size()!=1) return {};
 std::size_t new_index=after.size();
 for (std::size_t j=0;j<after.size();++j) if (!matched_after[j]) {
   if (new_index!=after.size()) return {};
   new_index=j;
 }
 if (new_index==after.size()) return {};
 return ViewChange{before[unmatched_before[0]],after[new_index]};
}
}
