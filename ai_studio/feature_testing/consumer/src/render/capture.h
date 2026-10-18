#ifndef GAME_RENDER_CAPTURE_H
#define GAME_RENDER_CAPTURE_H

// Read the current back buffer and write it as a PPM (P6). A tiny utility for
// screenshot/visual verification — run the game with `--capture <path.ppm>`.
void capture_write_ppm(const char *path);

#endif /* GAME_RENDER_CAPTURE_H */
