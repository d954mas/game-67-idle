#ifndef LL_FX_H
#define LL_FX_H
#include <stdbool.h>
// Juice/FX for Little Lives: world particles, screen shake, squash-stretch.
// No alpha blending available — particles are small opaque shaded shapes.
void  ll_fx_init(void);
void  ll_fx_update(float dt);                 // advance particles + shake (call once per frame)
void  ll_fx_draw_world(float daylight);       // draw 3D particles (called inside the depth-on world pass)
// Event spawns (world coords, Y-up):
void  ll_fx_dust(float x, float z);                       // soft ground puff (placement / footstep)
void  ll_fx_sparkle(float x, float y, float z);           // need-complete / reward twinkle
void  ll_fx_coins(float x, float y, float z, int count);  // work payout coins arc up
void  ll_fx_pop(float x, float y, float z, const float rgb[3]); // level-up / confirm burst
// Screen shake:
void  ll_fx_shake(float strength);            // kick a short decaying shake (strength ~0.2..1.0)
void  ll_fx_shake_world(float out_xyz[3]);    // current world-space camera jitter (0,0,0 if idle)
// Squash-stretch easing helper (pure): given seconds since an action started,
// write a nonuniform scale (x,y,z multipliers) that overshoots then settles to
// 1,1,1 by ~0.32s. Returns true while still animating, false once settled.
bool  ll_fx_pop_scale(float elapsed, float out_scale[3]);
#endif
