#ifndef FEATURE_SOFTWARE_CURSOR_SAMPLES_H
#define FEATURE_SOFTWARE_CURSOR_SAMPLES_H

#include "features/software_cursor/software_cursor.h"

typedef enum software_cursor_sample_visual_t {
    SOFTWARE_CURSOR_SAMPLE_POINTER_IDLE = 1,
    SOFTWARE_CURSOR_SAMPLE_POINTER_PRESS,
    SOFTWARE_CURSOR_SAMPLE_FINGER_OPEN,
    SOFTWARE_CURSOR_SAMPLE_FINGER_PRESS,
} software_cursor_sample_visual_t;

software_cursor_theme_t software_cursor_sample_pointer_theme(void);
software_cursor_theme_t software_cursor_sample_finger_theme(void);

#endif
