/*
 * Reflex Duel — Two-player reaction time game with Shell
 * Target: STM32G474RE (NUCLEO-G474RE)
 * RTOS:   ChibiOS RT 21.11.x
 */

#include "ch.h"
#include "hal.h"
#include "shell.h"

#include "game_hw.h"
#include "game_logic.h"
#include "game_shell.h"

int main(void) {
  /* System initializations. */
  halInit();
  chSysInit();

  /* Hardware initialization (Pins, UART, TRNG). */
  game_hw_init();

  /* Print welcome banner before starting concurrent threads. */
  game_hw_print_banner();

  /* Initialize sub-systems and spawn dedicated threads. */
  shellInit();
  game_shell_init();
  game_logic_init();

  /* The main thread has completed system setup.
   * It transitions to an idle monitor state. */
  while (true) {
    chThdSleepMilliseconds(1000);
  }
}
