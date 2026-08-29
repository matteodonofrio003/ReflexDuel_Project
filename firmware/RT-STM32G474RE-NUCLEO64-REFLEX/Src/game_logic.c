#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "game_hw.h"
#include "game_logic.h"

/* Timing constants (milliseconds). */
#define DELAY_MIN_MS        1000
#define DELAY_MAX_MS        10000
#define RESULT_DISPLAY_MS   3000

typedef enum {
  GAME_IDLE,
  GAME_READY,
  GAME_RESULT,
  GAME_FAULT
} game_state_t;

/* Shared volatile state — accessed from ISR and game thread. */
static volatile game_state_t game_state;
static volatile winner_t winner;
static volatile systime_t start_time;
static volatile sysinterval_t reaction_time;
static volatile bool game_active = false;

/* Statistics protected by mutex. */
static uint32_t wins_p1 = 0;
static uint32_t wins_p2 = 0;
static uint32_t best_time_ms = 0xFFFFFFFF;
static winner_t best_player = WINNER_NONE;
static mutex_t stats_mtx;

/* Binary semaphore to wake up game thread from ISR (READY phase only). */
static binary_semaphore_t sem_game_event;

/*===========================================================================*/
/* PAL callbacks (ISR context).                                              */
/*===========================================================================*/

static void btn_fault_cb(void *arg) {
  (void)arg;
  chSysLockFromISR();
  if (game_state == GAME_IDLE) {
    game_state = GAME_FAULT;
    palDisableLineEventI(LINE_BTN_P1);
    palDisableLineEventI(LINE_BTN_P2);
  }
  chSysUnlockFromISR();
}

static void btn_p1_cb(void *arg) {
  (void)arg;
  chSysLockFromISR();
  if (game_state == GAME_READY) {
    reaction_time = chVTTimeElapsedSinceX(start_time);
    winner = WINNER_P1;
    game_state = GAME_RESULT;
    palDisableLineEventI(LINE_BTN_P1);
    palDisableLineEventI(LINE_BTN_P2);
    chBSemSignalI(&sem_game_event);
  }
  chSysUnlockFromISR();
}

static void btn_p2_cb(void *arg) {
  (void)arg;
  chSysLockFromISR();
  if (game_state == GAME_READY) {
    reaction_time = chVTTimeElapsedSinceX(start_time);
    winner = WINNER_P2;
    game_state = GAME_RESULT;
    palDisableLineEventI(LINE_BTN_P1);
    palDisableLineEventI(LINE_BTN_P2);
    chBSemSignalI(&sem_game_event);
  }
  chSysUnlockFromISR();
}

/*===========================================================================*/
/* Game Thread.                                                              */
/*===========================================================================*/

static THD_WORKING_AREA(waGameThread, 1024);
static THD_FUNCTION(GameThread, arg) {
  (void)arg;
  chRegSetThreadName("game");

  while (true) {
    /* Wait for the game to be started via shell command. */
    if (!game_active) {
      chThdSleepMilliseconds(100);
      continue;
    }

    game_state = GAME_IDLE;
    winner = WINNER_NONE;
    game_hw_leds_off();

    uint32_t rng = game_hw_get_random();
    sysinterval_t delay_ticks = chTimeMS2I(DELAY_MIN_MS + (rng % (DELAY_MAX_MS - DELAY_MIN_MS)));
    systime_t idle_start = chVTGetSystemTimeX();

    palSetLineCallback(LINE_BTN_P1, btn_fault_cb, NULL);
    palSetLineCallback(LINE_BTN_P2, btn_fault_cb, NULL);
    palEnableLineEvent(LINE_BTN_P1, PAL_EVENT_MODE_FALLING_EDGE);
    palEnableLineEvent(LINE_BTN_P2, PAL_EVENT_MODE_FALLING_EDGE);

    while (chVTTimeElapsedSinceX(idle_start) < delay_ticks) {
      if (game_state == GAME_FAULT || !game_active) break;
      chThdSleepMilliseconds(1);
    }

    if (!game_active) {
      palDisableLineEvent(LINE_BTN_P1);
      palDisableLineEvent(LINE_BTN_P2);
      continue;
    }

    if (game_state == GAME_FAULT) {
      chprintf((BaseSequentialStream *)&SD2, "FALSE START! Restarting...\r\n");
      game_hw_fault_blink();
      continue;
    }

    palDisableLineEvent(LINE_BTN_P1);
    palDisableLineEvent(LINE_BTN_P2);

    game_state = GAME_READY;
    palSetLine(LINE_LED_RED);
    start_time = chVTGetSystemTimeX();

    /* Ensure semaphore is not signaled from any stray interrupts */
    chBSemWaitTimeout(&sem_game_event, TIME_IMMEDIATE);

    palSetLineCallback(LINE_BTN_P1, btn_p1_cb, NULL);
    palSetLineCallback(LINE_BTN_P2, btn_p2_cb, NULL);
    palEnableLineEvent(LINE_BTN_P1, PAL_EVENT_MODE_FALLING_EDGE);
    palEnableLineEvent(LINE_BTN_P2, PAL_EVENT_MODE_FALLING_EDGE);

    chBSemWait(&sem_game_event);

    palClearLine(LINE_LED_RED);
    uint32_t rt_ms = (uint32_t)chTimeI2MS(reaction_time);

    /* Safely update statistics using mutex. */
    chMtxLock(&stats_mtx);
    if (winner == WINNER_P1) {
      palSetLine(LINE_LED_BLUE);
      wins_p1++;
      chprintf((BaseSequentialStream *)&SD2, "Winner: Player 1 | Reaction time: %u ms\r\n", rt_ms);
    } else if (winner == WINNER_P2) {
      palSetLine(LINE_LED_GRN);
      wins_p2++;
      chprintf((BaseSequentialStream *)&SD2, "Winner: Player 2 | Reaction time: %u ms\r\n", rt_ms);
    }

    if (rt_ms < best_time_ms) {
      best_time_ms = rt_ms;
      best_player = winner;
      chprintf((BaseSequentialStream *)&SD2, "*** NEW BEST TIME! ***\r\n");
    }
    chMtxUnlock(&stats_mtx);

    chThdSleepMilliseconds(RESULT_DISPLAY_MS);
  }
}

/*===========================================================================*/
/* API Functions.                                                            */
/*===========================================================================*/

void game_logic_init(void) {
  chBSemObjectInit(&sem_game_event, true);
  chMtxObjectInit(&stats_mtx);
  chThdCreateStatic(waGameThread, sizeof(waGameThread), NORMALPRIO, GameThread, NULL);
}

void game_logic_start(void) { game_active = true; }
void game_logic_stop(void) { game_active = false; }

void game_logic_get_stats(uint32_t *w1, uint32_t *w2) {
  chMtxLock(&stats_mtx);
  *w1 = wins_p1;
  *w2 = wins_p2;
  chMtxUnlock(&stats_mtx);
}

void game_logic_get_best_time(uint32_t *time_ms, winner_t *player) {
  chMtxLock(&stats_mtx);
  *time_ms = best_time_ms;
  *player = best_player;
  chMtxUnlock(&stats_mtx);
}
