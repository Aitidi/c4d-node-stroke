#pragma once

// The collection must provide GetCount() and operator[]. A view is bindable only
// when the same unique set of keys exists in both samples and exactly one value
// changes. Returning -1 rejects new/deleted/duplicate records and ambiguity.
namespace wire_binding {
template <class Records, class KeyEqual, class ValueEqual>
long FindUniqueChanged(const Records& before, const Records& after, KeyEqual keyEqual, ValueEqual valueEqual) {
  auto count = before.GetCount();
  if (count == 0 || count != after.GetCount()) return -1;
  long changed = -1;
  for (decltype(count) i = 0; i < count; ++i) {
    for (decltype(count) j = i + 1; j < count; ++j)
      if (keyEqual(before[i], before[j]) || keyEqual(after[i], after[j])) return -1;
    long match = -1;
    for (decltype(count) j = 0; j < count; ++j)
      if (keyEqual(after[i], before[j])) match = static_cast<long>(j);
    if (match < 0) return -1;
    if (!valueEqual(after[i], before[match])) {
      if (changed >= 0) return -1;
      changed = static_cast<long>(i);
    }
  }
  return changed;
}
}
