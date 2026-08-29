#ifndef GAME_HW_H
#define GAME_HW_H

#include "hal.h"

#define LINE_LED_RED    PAL_LINE(GPIOB, 4U)   /* Start signal (Arduino D5) */
#define LINE_LED_BLUE   PAL_LINE(GPIOB, 5U)   /* Player 1 win (Arduino D4) */
#define LINE_LED_GRN    PAL_LINE(GPIOB, 10U)  /* Player 2 win (Arduino D6) */
#define LINE_BTN_P1     PAL_LINE(GPIOA, 10U)  /* Button P1 (Arduino D2)    */
#define LINE_BTN_P2     PAL_LINE(GPIOA, 8U)   /* Button P2 (Arduino D7)    */

void game_hw_init(void);
void game_hw_print_banner(void);
void game_hw_leds_off(void);
uint32_t game_hw_get_random(void);
void game_hw_fault_blink(void);

#endif /* GAME_HW_H */
