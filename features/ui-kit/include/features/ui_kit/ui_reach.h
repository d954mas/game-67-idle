#ifndef FEATURE_UI_KIT_REACH_H
#define FEATURE_UI_KIT_REACH_H

#include <stdbool.h>

// How the player reaches this screen: with a finger, at arm's length, or with a
// pointing device across a desk. It is the one fact that decides how large a
// share of the screen the interface has to own, and it is NOT the window's
// shape or its pixel count -- a phone held sideways is still a phone, and a
// small window on a monitor is still a monitor.
//
// The browser answers it directly: `pointer: coarse` means the primary input is
// a finger. Native desktop builds are a mouse unless a reviewer says otherwise.
bool ui_screen_in_hand(void);

#endif /* FEATURE_UI_KIT_REACH_H */
