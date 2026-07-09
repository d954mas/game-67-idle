#ifndef GAME_FORMAT_H
#define GAME_FORMAT_H
#include <stddef.h>
#include <stdint.h>
/* L0-лист (шелл, вне features/ -- как game_log/game_save; включается фичами
   вниз легально). "1234"->"1.2K", 1500000->"1.5M", int64-max(~9.2e18)->"9.2Qi";
   |v|<1000 точно. Суффиксы K,M,B,T,Qa,Qi (1e3..1e18). Одна значащая дробь.
   Отрицательные с '-'. out ёмкости >=16 достаточно. Возвращает out. */
char *game_format_i64_abbrev(int64_t v, char *out, size_t cap);
#endif
