// The connection rules editor: lists the rules of <prefs>/nodestroke_rules.txt with
// the node and port names Cinema 4D shows, adds a rule from the ports selected in the
// node editor, and deletes rules. Opened from Preferences > NodeStroke > Edit Rules
// or the "NodeStroke: Edit Connection Rules" command.
#include "c4d.h"
#include "c4d_listview.h"
#include "../res/c4d_symbols.h"
#include "graph_ops.h"
#include "maxon/assets.h"
#include "maxon/datadescription_data.h"
#include "maxon/datadescription_string.h"
#include "maxon/datadescriptiondatabase.h"
#include "maxon/nodetemplate.h"
#include "maxon/stringresource.h"

using namespace cinema;

namespace {
// Plugin ID registered with Maxon (Plugin Cafe, account aitidi).
constexpr Int32 kRulesDialogId = 1072560;

enum {
  ID_HINT = 1000,
  ID_LIST,
  ID_EMPTY,
  ID_ADD,
  ID_DELETE,
  ID_RELOAD,
  ID_OPEN
};
constexpr Int32 kColumn = 'rule';

// The last part of a dotted identifier, without a trailing "@..." instance suffix.
maxon::String ShortId(const maxon::String& id) {
  maxon::String s = id;
  const Int at = s.FindLastIndex('@');
  if (at >= 0) s = s.GetPart(0, at);
  const Int dot = s.FindLastIndex('.');
  if (dot >= 0) s = s.GetPart(dot + 1, maxon::StringEnd());
  return s.IsEmpty() ? id : s;
}

// Display names of node assets and their ports, as the node editor shows them.
class Names {
public:
  explicit Names(BaseDocument* doc) {
    if (doc) { iferr (_repository = doc->GetSceneRepository(true)) {} }
  }
  maxon::String Node(const maxon::String& asset) {
    iferr_scope_handler { return ShortId(asset); };
    if (!_repository) return ShortId(asset);
    maxon::Id id;
    id.Init(asset) iferr_return;
    const maxon::AssetDescription description =
      _repository.FindLatestAsset(maxon::AssetTypes::NodeTemplate(), id, maxon::Id(), maxon::ASSET_FIND_MODE::LATEST) iferr_return;
    if (!description) return ShortId(asset);
    const maxon::String name = description.GetMetaString(maxon::OBJECT::BASE::NAME, maxon::Resource::GetCurrentLanguage(), ShortId(asset)) iferr_return;
    return name;
  }
  maxon::String Port(const maxon::String& asset, const maxon::String& port) {
    iferr_scope_handler { return ShortId(port); };
    if (!_repository) return ShortId(port);
    maxon::Id id;
    id.Init(asset) iferr_return;
    const maxon::DataDescription strings =
      _repository.LoadDescription(maxon::DATADESCRIPTION_CATEGORY_STRING, maxon::Resource::GetCurrentLanguage(), id) iferr_return;
    maxon::InternedId wanted;
    wanted.Init(port) iferr_return;
    const maxon::BaseArray<maxon::DataDictionary> entries = strings.GetEntries() iferr_return;
    for (const maxon::DataDictionary& e : entries) {
      if (!(e.Get(maxon::DESCRIPTION::BASE::IDENTIFIER, maxon::InternedId()) == wanted)) continue;
      const maxon::String name = e.Get(maxon::DESCRIPTION::STRING::BASE::TRANSLATEDSTRING, maxon::String());
      if (!name.IsEmpty()) return name;
    }
    return ShortId(port);
  }
private:
  maxon::AssetRepositoryRef _repository;
};

class RulesDialog final : public GeDialog {
public:
  Bool CreateLayout() override {
    SetTitle(GeLoadString(IDS_RULES_TITLE));
    GroupBegin(0, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, String(), 0);
    GroupBorderSpace(8, 8, 8, 8);
    AddStaticText(ID_HINT, BFH_SCALEFIT, 0, 0, GeLoadString(IDS_RULES_HINT), 0);
    AddListView(ID_LIST, BFH_SCALEFIT | BFV_SCALEFIT, 560, 220);
    AddStaticText(ID_EMPTY, BFH_SCALEFIT, 0, 0, GeLoadString(IDS_RULES_EMPTY), 0);
    GroupBegin(0, BFH_SCALEFIT, 4, 0, String(), 0);
    AddButton(ID_ADD, BFH_LEFT, 0, 0, GeLoadString(IDS_RULES_ADD));
    AddButton(ID_DELETE, BFH_LEFT, 0, 0, GeLoadString(IDS_RULES_DELETE));
    AddButton(ID_RELOAD, BFH_LEFT, 0, 0, GeLoadString(IDS_RULES_RELOAD));
    AddButton(ID_OPEN, BFH_RIGHT | BFH_SCALE, 0, 0, GeLoadString(IDS_RULES_OPEN));
    GroupEnd();
    GroupEnd();
    return _list.AttachListView(this, ID_LIST);
  }
  Bool InitValues() override {
    BaseContainer layout;
    layout.SetInt32(kColumn, LV_COLUMN_TEXT);
    _list.SetLayout(1, layout);
    _list.SetProperty(SLV_MULTIPLESELECTION, 1);
    Reload();
    return true;
  }
  Bool Command(Int32 id, const BaseContainer& msg) override {
    switch (id) {
      case ID_LIST:
        UpdateButtons();
        break;
      case ID_ADD: {
        iferr (wire_ops::AddRuleFromSelection(GetActiveDocument())) {
          MessageDialog(err.GetMessage());
          break;
        }
        EventAdd();
        StatusSetText(GeLoadString(IDS_RULES_ADDED));
        Reload();
        break;
      }
      case ID_DELETE:
        DeleteSelected();
        break;
      case ID_RELOAD:
        Reload();
        break;
      case ID_OPEN:
        wire_ops::OpenRulesFile();
        break;
    }
    return true;
  }

private:
  void Reload() {
    for (Int32 i = _list.GetItemCount() - 1; i >= 0; --i) {
      Int32 line = 0;
      if (_list.GetItemLine(i, &line, nullptr)) _list.RemoveItem(line);
    }
    _rules.Reset();
    iferr (_rules = wire_ops::LoadRules()) { MessageDialog(err.GetMessage()); }
    Names names(GetActiveDocument());
    for (Int i = 0; i < _rules.GetCount(); ++i) {
      const wire_ops::Rule& r = _rules[i];
      const String text = names.Node(r.sourceAsset) + " / "_s + names.Port(r.sourceAsset, r.sourcePort) +
        "   ->   "_s + names.Node(r.targetAsset) + " / "_s + names.Port(r.targetAsset, r.targetPort);
      BaseContainer data;
      data.SetString(kColumn, text);
      _list.SetItem(Int32(i), data);
    }
    _list.DataChanged();
    HideElement(ID_EMPTY, !_rules.IsEmpty());
    LayoutChanged(0);
    UpdateButtons();
  }
  void UpdateButtons() {
    Enable(ID_DELETE, _selection && _list.GetSelection(_selection) > 0);
  }
  void DeleteSelected() {
    if (!_selection || _list.GetSelection(_selection) <= 0) return;
    maxon::BaseArray<wire_ops::Rule> kept;
    for (Int i = 0; i < _rules.GetCount(); ++i) {
      if (_selection->IsSelected(Int32(i))) continue;
      iferr (kept.Append(_rules[i])) { MessageDialog(err.GetMessage()); return; }
    }
    iferr (wire_ops::SaveRules(kept)) { MessageDialog(err.GetMessage()); return; }
    Reload();
  }

  SimpleListView _list;
  AutoAlloc<BaseSelect> _selection;
  maxon::BaseArray<wire_ops::Rule> _rules;
};

class RulesCommand final : public CommandData {
public:
  Bool Execute(BaseDocument*, GeDialog*) override {
    return _dialog.Open(DLG_TYPE::ASYNC, kRulesDialogId, -1, -1, 600, 320);
  }
  Bool RestoreLayout(void* secret) override {
    return _dialog.RestoreLayout(kRulesDialogId, 0, secret);
  }
private:
  RulesDialog _dialog;
};
}

void OpenRulesEditor() { CallCommand(kRulesDialogId); }

// Kept out of the Extensions menu; opened from the preferences page or the Commander.
Bool RegisterRulesDialog() {
  return RegisterCommandPlugin(kRulesDialogId, GeLoadString(IDS_CMD_RULES), PLUGINFLAG_HIDEPLUGINMENU, nullptr,
    GeLoadString(IDS_CMD_RULES_HELP), NewObjClear(RulesCommand));
}
