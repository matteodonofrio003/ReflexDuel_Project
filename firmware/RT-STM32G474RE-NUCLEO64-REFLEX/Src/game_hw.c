#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "game_hw.h"

#define FAULT_BLINK_COUNT   3
#define FAULT_BLINK_MS      200

void game_hw_init(void) {
  /* Configure USART2 pins for serial output (VCP via ST-Link).
   * PA2 = USART2_TX (AF7), PA3 = USART2_RX (AF7). */
  palSetPadMode(GPIOA, 2U, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3U, PAL_MODE_ALTERNATE(7));
  sdStart(&SD2, NULL); /* default: 38400 baud */

  /* Configure LED pins as outputs. */
  palSetLineMode(LINE_LED_RED,  PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_LED_BLUE, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_LED_GRN,  PAL_MODE_OUTPUT_PUSHPULL);

  /* Configure button pins as inputs with pull-up. */
  palSetLineMode(LINE_BTN_P1, PAL_MODE_INPUT_PULLUP);
  palSetLineMode(LINE_BTN_P2, PAL_MODE_INPUT_PULLUP);

  /* Start TRNG driver. */
  trngStart(&TRNGD1, NULL);

  game_hw_leds_off();
}

void game_hw_print_banner(void) {
  chprintf((BaseSequentialStream *)&SD2,
           "\r\n=== Reflex Duel ===\r\n"
           "Type 'start' to begin, 'stop' to pause.\r\n");
}

void game_hw_leds_off(void) {
  palClearLine(LINE_LED_RED);
  palClearLine(LINE_LED_BLUE);
  palClearLine(LINE_LED_GRN);
}

uint32_t game_hw_get_random(void) {
  uint32_t val;
  trngGenerate(&TRNGD1, sizeof(uint32_t), (uint8_t *)&val);
  return val;
}

void game_hw_fault_blink(void) {
  for (int i = 0; i < FAULT_BLINK_COUNT; i++) {
    palSetLine(LINE_LED_RED);
    chThdSleepMilliseconds(FAULT_BLINK_MS);
    palClearLine(LINE_LED_RED);
    chThdSleepMilliseconds(FAULT_BLINK_MS);
  }
}
