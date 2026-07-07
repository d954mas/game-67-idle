#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H
/* Hand-written LOGIC for the `settings` fragment (design Р9). The state layer
   (SettingsState / defaults / (de)serialization / schema / descriptor) is
   GENERATED into settings_state.{h,c}; only this feature logic is by hand.
   A game copies this shape for its own smart fragments. */
float settings_master(void);
float settings_music(void);
float settings_sfx(void);
/* clamp to [0,1], write the generated instance, mark the save dirty. */
void  settings_set_master(float value);
void  settings_set_music(float value);
void  settings_set_sfx(float value);
#endif /* GAME_SETTINGS_H */
