#include "sound.h"

#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "sound_timing.h"

#define PICOCALC_AUDIO_LEFT_GPIO 26u
#define PICOCALC_AUDIO_RIGHT_GPIO 27u
#define AUDIO_PWM_WRAP 255u
#define AUDIO_LEVEL_LOW 104u
#define AUDIO_LEVEL_HIGH 152u
#define AUDIO_LEVEL_IDLE 128u
#define AUDIO_SAMPLE_PERIOD_US 40u
#define AUDIO_SAMPLE_RATE_HZ (1000000u / AUDIO_SAMPLE_PERIOD_US)
#define AUDIO_EDGE_HOLD_MS 20u
#define AUDIO_EDGE_HOLD_SAMPLES \
  ((AUDIO_SAMPLE_RATE_HZ * AUDIO_EDGE_HOLD_MS) / 1000u)
#define STARTUP_BEEP_HZ 2600u
#define STARTUP_BEEP_MS 140u

static uint s_audio_slice;
static bool s_initialized;
static struct repeating_timer s_audio_timer;
static sound_timing_t s_sound_timing;
static volatile uint32_t s_phase;
static volatile uint32_t s_phase_step;
static volatile uint32_t s_hold_samples;
static volatile bool s_output_known;
static volatile bool s_output_high;

static void set_stereo_level(uint16_t level) {
  pwm_set_gpio_level(PICOCALC_AUDIO_LEFT_GPIO, level);
  pwm_set_gpio_level(PICOCALC_AUDIO_RIGHT_GPIO, level);
}

static void start_tone(uint32_t frequency_hz) {
  uint32_t irq_state = save_and_disable_interrupts();
  s_phase_step = (uint32_t)(((uint64_t)frequency_hz << 32) /
                            AUDIO_SAMPLE_RATE_HZ);
  s_hold_samples = AUDIO_EDGE_HOLD_SAMPLES;
  restore_interrupts(irq_state);
}

static bool audio_timer_callback(struct repeating_timer *timer) {
  (void)timer;
  if (!s_initialized) return true;

  uint32_t remaining = s_hold_samples;
  uint32_t step = s_phase_step;
  if (remaining != 0 && step != 0) {
    s_hold_samples = remaining - 1u;
    s_phase += step;
    bool high = (s_phase & 0x80000000u) != 0;
    if (!s_output_known || s_output_high != high) {
      s_output_known = true;
      s_output_high = high;
      set_stereo_level(high ? AUDIO_LEVEL_HIGH : AUDIO_LEVEL_LOW);
    }
  } else if (s_output_known) {
    s_output_known = false;
    set_stereo_level(AUDIO_LEVEL_IDLE);
  }
  return true;
}

static void configure_sample_output(void) {
  pwm_config config = pwm_get_default_config();
  pwm_config_set_wrap(&config, AUDIO_PWM_WRAP);
  pwm_config_set_clkdiv(&config, 1.0f);
  pwm_init(s_audio_slice, &config, false);
  set_stereo_level(AUDIO_LEVEL_IDLE);
  pwm_set_enabled(s_audio_slice, true);
  s_output_known = false;
}

void platform_sound_init(void) {
  gpio_set_function(PICOCALC_AUDIO_LEFT_GPIO, GPIO_FUNC_PWM);
  gpio_set_function(PICOCALC_AUDIO_RIGHT_GPIO, GPIO_FUNC_PWM);

  s_audio_slice = pwm_gpio_to_slice_num(PICOCALC_AUDIO_LEFT_GPIO);
  configure_sample_output();
  sound_timing_reset(&s_sound_timing);
  s_initialized = true;
  add_repeating_timer_us(-(int64_t)AUDIO_SAMPLE_PERIOD_US,
                         audio_timer_callback, NULL, &s_audio_timer);
}

void platform_sound_set_level(bool high, unsigned long instruction_count) {
  if (!s_initialized) return;

  /* A 256-instruction core batch can contain several speaker edges followed
   * by a pacing sleep. Decode their calculator-time spacing and let the 25 kHz
   * timer emit a continuous square wave independently of that batching. */
  uint32_t frequency_hz;
  if (sound_timing_observe(&s_sound_timing, high,
                           (uint32_t)instruction_count, &frequency_hz))
    start_tone(frequency_hz);
}

void platform_sound_silence(void) {
  if (!s_initialized) return;
  uint32_t irq_state = save_and_disable_interrupts();
  s_hold_samples = 0;
  s_phase_step = 0;
  sound_timing_reset(&s_sound_timing);
  s_output_known = false;
  set_stereo_level(AUDIO_LEVEL_IDLE);
  restore_interrupts(irq_state);
}

void platform_sound_startup_beep(void) {
  if (!s_initialized) return;
  /* The +/-24 PWM swing is 60% of dev.5's +/-40 amplitude. Use the same
   * asynchronous path as ROM audio so startup is a direct hardware reference
   * for the reconstructed diagnostic beep. */
  start_tone(STARTUP_BEEP_HZ);
  sleep_ms(STARTUP_BEEP_MS);
  platform_sound_silence();
}
