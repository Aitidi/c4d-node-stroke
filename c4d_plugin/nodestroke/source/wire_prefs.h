#pragma once
// User settings shown on the "NodeStroke" page of the Cinema 4D preferences and
// stored there (world container). Gestures are on by default, the diagnostic log off.
#include "c4d.h"
#include "../res/description/prefsnodestroke.h"

namespace wire_prefs {
// option: one of the NODESTROKE_PREFS_* switches.
cinema::Bool Enabled(cinema::Int32 option);
void SetEnabled(cinema::Int32 option, cinema::Bool value);
}
// Registers the preferences page and the command that opens it.
cinema::Bool RegisterWirePrefs();
// The "NodeStroke Settings..." command. Plugin ID registered with Maxon
// (Plugin Cafe, account aitidi) as NodeStrokeSettingsCommand.
constexpr cinema::Int32 kSettingsCommandId = 1073003;
// Re-reads the settings into the canvas gesture hook (canvas_gesture.cpp).
void ApplyGesturePrefs();
