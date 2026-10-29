#include "../c4d_plugin/nodestroke/source/view_binding.h"
#include <cstdlib>
#include <iostream>
#include <vector>
struct Record { int key; int value; };
struct Records : std::vector<Record> { using std::vector<Record>::vector; std::size_t GetCount() const { return size(); } };
struct GroupRecord { int group, key, value; };
struct GroupRecords : std::vector<GroupRecord> { using std::vector<GroupRecord>::vector; std::size_t GetCount() const { return size(); } };
long changed(const Records& before, const Records& after) {
  return wire_binding::FindUniqueChanged(before, after,
    [](const Record& a, const Record& b) { return a.key == b.key; },
    [](const Record& a, const Record& b) { return a.value == b.value; });
}
void check(bool ok, const char* name) { if (!ok) { std::cerr << name << '\n'; std::exit(1); } }
int main() {
  check(changed({{1,10},{2,20}}, {{2,30},{1,10}}) == 0, "changed view after reorder");
  check(changed({{1,10}}, {{1,20}}) == 0, "single view must change");
  check(changed({{1,10}}, {{1,10}}) == -1, "no change rejected");
  check(changed({{1,10},{2,20}}, {{1,11},{2,21}}) == -1, "multiple changes rejected");
  check(changed({{1,10},{2,20}}, {{1,10},{3,30}}) == -1, "replaced key rejected");
  check(changed({{1,10}}, {{1,10},{2,20}}) == -1, "new view rejected");
  check(changed({{1,10},{1,20}}, {{1,10},{2,30}}) == -1, "duplicate before rejected");
  check(changed({{1,10},{2,20}}, {{1,10},{1,30}}) == -1, "duplicate after rejected");
  const GroupRecords nestedBefore{{0,0,10},{1,0,20},{2,0,30}};
  const GroupRecords nestedAfter{{2,0,30},{0,0,10},{1,0,21}};
  auto groupChanged = wire_binding::FindUniqueChanged(nestedBefore, nestedAfter,
    [](const GroupRecord& a, const GroupRecord& b) { return a.group == b.group && a.key == b.key; },
    [](const GroupRecord& a, const GroupRecord& b) { return a.value == b.value; });
  check(groupChanged == 2, "same view key in different groups identifies changed container");
}
