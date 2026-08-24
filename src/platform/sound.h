#pragma once

#include <stdbool.h>

/* PicoCalc mainboard V2 routes stereo PWM audio to GP26 (left) and GP27
 * (right). The HP 48 exposes a one-bit speaker line through OUT bit 11. */
void platform_sound_init(void);
void platform_sound_set_level(bool high, unsigned long instruction_count);
void platform_sound_silence(void);
void platform_sound_startup_beep(void);
