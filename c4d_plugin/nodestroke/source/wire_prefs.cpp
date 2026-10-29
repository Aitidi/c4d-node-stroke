#include "wire_prefs.h"
#include "c4d_libs/lib_prefs.h"
#include "c4d_libs/lib_description.h"

// Opens the connection rules editor (rules_dialog.cpp).
void OpenRulesEditor();

using namespace cinema;

namespace {
// Plugin ID registered with Maxon (Plugin Cafe, account aitidi).
constexpr Int32 kPrefsPageId = 1072547;
constexpr Int32 kSwitches[] = {NODESTROKE_PREFS_GESTURES, NODESTROKE_PREFS_CONNECT, NODESTROKE_PREFS_CUT,
                               NODESTROKE_PREFS_SHAKE, NODESTROKE_PREFS_DROP, NODESTROKE_PREFS_LOG};
// Gesture switches are on unless turned off; the diagnostic log is off unless turned on.
Bool DefaultOf(Int32 option) { return option != NODESTROKE_PREFS_LOG; }
Bool IsSwitch(Int32 id) {
  for (const Int32 s : kSwitches) if (s == id) return true;
  return false;
}

// The page shows the switches and a button to open the rules file. Values live in
// a sub-container of the world container, so Cinema 4D saves them with its prefs.
class WirePrefs final : public PrefsDialogObject {
public:
  static NodeData* Alloc() { return NewObjClear(WirePrefs); }

  Bool GetDDescription(const GeListNode* node, Description* description, DESCFLAGS_DESC& flags) const override {
    if (!description || !description->LoadDescription("prefsnodestroke"_s)) return false;
    flags |= DESCFLAGS_DESC::LOADED;
    return PrefsDialogObject::GetDDescription(node, description, flags);
  }
  Bool GetDParameter(const GeListNode* node, const DescID& id, GeData& data, DESCFLAGS_GET& flags) const override {
    const Int32 option = id[0].id;
    if (!IsSwitch(option)) return PrefsDialogObject::GetDParameter(node, id, data, flags);
    data.SetInt32(wire_prefs::Enabled(option));
    flags |= DESCFLAGS_GET::PARAM_GET;
    return true;
  }
  Bool SetDParameter(GeListNode* node, const DescID& id, const GeData& data, DESCFLAGS_SET& flags) override {
    const Int32 option = id[0].id;
    if (!IsSwitch(option)) return PrefsDialogObject::SetDParameter(node, id, data, flags);
    wire_prefs::SetEnabled(option, data.GetBool());
    flags |= DESCFLAGS_SET::PARAM_SET;
    GeUpdateUI();
    return true;
  }
  // The single gestures turn off when gestures as a whole are off.
  Bool GetDEnabling(const GeListNode* node, const DescID& id, const GeData& data, DESCFLAGS_ENABLE flags,
                    const BaseContainer* itemdesc) const override {
    const Int32 option = id[0].id;
    if (IsSwitch(option) && option != NODESTROKE_PREFS_GESTURES && option != NODESTROKE_PREFS_LOG)
      return wire_prefs::Enabled(NODESTROKE_PREFS_GESTURES);
    return PrefsDialogObject::GetDEnabling(node, id, data, flags, itemdesc);
  }
  Bool Message(GeListNode* node, Int32 type, void* data) override {
    if (type == MSG_DESCRIPTION_COMMAND && data && static_cast<DescriptionCommand*>(data)->_descId[0].id == NODESTROKE_PREFS_EDIT_RULES)
      OpenRulesEditor();
    return PrefsDialogObject::Message(node, type, data);
  }
};
}

namespace wire_prefs {
Bool Enabled(Int32 option) {
  BaseContainer* world = GetWorldContainerInstance();
  const BaseContainer* settings = world ? world->GetContainerInstance(kPrefsPageId) : nullptr;
  return settings ? settings->GetBool(option, DefaultOf(option)) : DefaultOf(option);
}
void SetEnabled(Int32 option, Bool value) {
  BaseContainer* world = GetWorldContainerInstance();
  if (!world) return;
  BaseContainer* settings = world->GetContainerInstanceWritable(kPrefsPageId);
  if (!settings) {
    world->SetContainer(kPrefsPageId, BaseContainer());
    settings = world->GetContainerInstanceWritable(kPrefsPageId);
  }
  if (settings) settings->SetBool(option, value);
  ApplyGesturePrefs();
}
}

Bool RegisterWirePrefs() {
  return PrefsDialogObject::Register(kPrefsPageId, WirePrefs::Alloc, "NodeStroke"_s, "prefsnodestroke"_s, 0, 0) != nullptr;
}
