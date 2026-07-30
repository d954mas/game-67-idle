#ifndef GAME_STORAGE_WEB_H
#define GAME_STORAGE_WEB_H

int game_storage_web_key_exists(const char *key);
int game_storage_web_save(const char *key, const char *text);
char *game_storage_web_load(const char *key, int *status);
int game_storage_web_quarantine(const char *key);
int game_storage_web_probe(const char *key);

#endif /* GAME_STORAGE_WEB_H */
