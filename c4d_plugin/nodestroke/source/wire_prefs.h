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
cinema::Bool RegisterWirePrefs();
// Re-reads the settings into the canvas gesture hook (canvas_gesture.cpp).
void ApplyGesturePrefs();
