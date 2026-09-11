#ifndef FEATURE_UI_KIT_REACH_H
#define FEATURE_UI_KIT_REACH_H

#include <stdbool.h>

// Whether the player is touching this screen or pointing at it.
//
// It decides NO size. Sizes come from the viewport alone, so the same window
// draws the same interface everywhere and a phone's layout is something a
// reviewer can look at on a monitor. This is for what a size cannot express: a
// control that moves under the thumb, a hover-only affordance that has to
// become a visible button, room around something a finger has to hit.
//
// The browser answers it directly: `pointer: coarse` means the primary input is
// a finger, which a phone and a tablet report in both orientations while a
// touchscreen laptop still reports its mouse. Native desktop builds are a mouse
// unless a reviewer says otherwise.
bool ui_screen_in_hand(void);

#endif /* FEATURE_UI_KIT_REACH_H */
