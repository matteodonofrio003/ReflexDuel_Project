#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "ch.h"

typedef enum {
  WINNER_NONE = 0,
  WINNER_P1,
  WINNER_P2
} winner_t;

void game_logic_init(void);
void game_logic_start(void);
void game_logic_stop(void);
void game_logic_get_stats(uint32_t *w1, uint32_t *w2);
void game_logic_get_best_time(uint32_t *time_ms, winner_t *player);

#endif /* GAME_LOGIC_H */
