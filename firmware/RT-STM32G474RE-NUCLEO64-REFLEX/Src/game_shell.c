#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "shell.h"
#include "game_hw.h"
#include "game_logic.h"
#include "game_shell.h"

static void cmd_start(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argc; (void)argv;
  game_logic_start();
  chprintf(chp, "Game STARTED!\r\n");
}

static void cmd_stop(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argc; (void)argv;
  game_logic_stop();
  game_hw_leds_off();
  chprintf(chp, "Game STOPPED!\r\n");
}

static void cmd_winners(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argc; (void)argv;
  uint32_t w1, w2;
  game_logic_get_stats(&w1, &w2);
  chprintf(chp, "Wins - P1: %u | P2: %u\r\n", w1, w2);
}

static void cmd_best_time(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argc; (void)argv;
  uint32_t time_ms;
  winner_t player;
  game_logic_get_best_time(&time_ms, &player);

  if (time_ms == 0xFFFFFFFF) {
    chprintf(chp, "No record yet.\r\n");
  } else {
    chprintf(chp, "Best time: %u ms (Player %d)\r\n", time_ms, (player == WINNER_P1) ? 1 : 2);
  }
}

static void cmd_test_io(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argc; (void)argv;
  chprintf(chp, "Testing LEDs (Red -> Blue -> Green)...\r\n");
  palSetLine(LINE_LED_RED); chThdSleepMilliseconds(300); palClearLine(LINE_LED_RED);
  palSetLine(LINE_LED_BLUE); chThdSleepMilliseconds(300); palClearLine(LINE_LED_BLUE);
  palSetLine(LINE_LED_GRN); chThdSleepMilliseconds(300); palClearLine(LINE_LED_GRN);

  chprintf(chp, "Button live state:\r\n");
  chprintf(chp, "  P1 (D2): %s\r\n", palReadLine(LINE_BTN_P1) == PAL_LOW ? "PRESSED (0)" : "RELEASED (1)");
  chprintf(chp, "  P2 (D7): %s\r\n", palReadLine(LINE_BTN_P2) == PAL_LOW ? "PRESSED (0)" : "RELEASED (1)");
}

static const ShellCommand shell_commands[] = {
  {"start",     cmd_start},
  {"stop",      cmd_stop},
  {"winners",   cmd_winners},
  {"best_time", cmd_best_time},
  {"test_io",   cmd_test_io},
  {NULL, NULL}
};

static const ShellConfig shell_cfg = {
  (BaseSequentialStream *)&SD2,
  shell_commands
};

static THD_WORKING_AREA(waShellThread, 2048);

void game_shell_init(void) {
  /* Spawn the shell thread with static working area. */
  chThdCreateStatic(waShellThread, sizeof(waShellThread), NORMALPRIO, shellThread, (void *)&shell_cfg);
}
