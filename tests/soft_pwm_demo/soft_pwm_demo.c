
/*



avr-gcc \
  -mmcu=atmega1284p \
  -DF_CPU=8000000UL \
  -Os \
  -Wall -Wextra \
  -std=gnu11 \
  soft_pwm_demo.c \
  -o soft_pwm_demo.elf

  avr-objcopy -O ihex -R .eeprom \
    soft_pwm_demo.elf \
    soft_pwm_demo.hex

    avr-size --mcu=atmega1284p soft_pwm_demo.elf

    avrdude \
      -c atmelice_isp \
      -P usb \
      -p m1284p \
      -U flash:w:soft_pwm_demo.hex:i

 *
 */


/*
 * soft_pwm_demo.c
 *
 * Target: ATmega1284P @ 8 MHz
 *
 * Hardware:
 *  - Anti-parallel red/green LED
 *  - PA0 / PA1 drive LED directly
 *
 * Polarity (CONFIRMED):
 *  - GREEN: PA0 = 1, PA1 = 0
 *  - RED  : PA0 = 0, PA1 = 1
 *  - OFF  : PA0 = 0, PA1 = 0
 *
 * Notes:
 *  - Software PWM only (NO timers)
 *  - Deterministic, blocking demo
 *  - Smooth breathing, no flicker
 */

 #include <avr/io.h>
 #include <util/delay.h>
 #include <stdint.h>

 /* ------------------------------------------------------------
  * PWM / timing parameters
  * ------------------------------------------------------------ */

 #define PWM_STEPS          256
 #define PWM_DELAY_US       20      /* MUST be non-zero */
 #define FRAMES_PER_STEP    10       /* breathing speed */
 #define CYCLE_PAUSE_MS     250     /* pause between colors */

 /* ------------------------------------------------------------
  * Perceptual brightness ramps
  * ------------------------------------------------------------ */

 /* GREEN ramp — already correct */
 static const uint8_t ramp_green[] = {
       0,  1,  2,  4,  7, 11, 16, 22,
      29, 37, 46, 56, 67, 79, 92,106,
     121,137,154,172,191,211,232,255,
     232,211,191,172,154,137,121,106,
      92, 79, 67, 56, 46, 37, 29, 22,
      16, 11,  7,  4,  2,  1
 };

 /* RED ramp — boosted mid/high range for visual balance */
 static const uint8_t ramp_red[] = {
       0,  4,  7, 11, 16, 23, 31, 40,
      50, 61, 73, 86,100,115,131,148,
     166,185,205,225,245,252,255,255,
     252,245,225,205,185,166,148,131,
     115,100, 86, 73, 61, 50, 40, 31,
      23, 16, 11,  7,  4
 };

 #define RAMP_LEN (sizeof(ramp_green))

 /* ------------------------------------------------------------
  * Low-level LED helpers (LOCKED)
  * ------------------------------------------------------------ */

 static inline void led_off(void)
 {
     PORTA &= ~((1 << PA0) | (1 << PA1));
 }

 /* ------------------------------------------------------------
  * Software PWM breathing cores
  * ------------------------------------------------------------ */

 static void breathe_green(void)
 {
     for (uint8_t step = 0; step < RAMP_LEN; step++) {

         uint8_t duty = ramp_green[step];

         for (uint8_t frame = 0; frame < FRAMES_PER_STEP; frame++) {

             for (uint16_t i = 0; i < PWM_STEPS; i++) {

                 if (i < duty)
                     PORTA |=  (1 << PA0);   /* GREEN ON */
                 else
                     PORTA &= ~(1 << PA0);   /* GREEN OFF */

                 PORTA &= ~(1 << PA1);       /* force RED low */
                 _delay_us(PWM_DELAY_US);
             }
         }
     }
 }

 static void breathe_red(void)
 {
     for (uint8_t step = 0; step < RAMP_LEN; step++) {

         uint8_t duty = ramp_red[step];

         for (uint8_t frame = 0; frame < FRAMES_PER_STEP; frame++) {

             for (uint16_t i = 0; i < PWM_STEPS; i++) {

                 if (i < duty)
                     PORTA |=  (1 << PA1);   /* RED ON */
                 else
                     PORTA &= ~(1 << PA1);   /* RED OFF */

                 PORTA &= ~(1 << PA0);       /* force GREEN low */
                 _delay_us(PWM_DELAY_US);
             }
         }
     }
 }

 /* ------------------------------------------------------------
  * Main
  * ------------------------------------------------------------ */

 int main(void)
 {
     /* PA0 / PA1 as outputs */
     DDRA |= (1 << PA0) | (1 << PA1);
     led_off();

     for (;;) {
         breathe_green();
         led_off();
         _delay_ms(CYCLE_PAUSE_MS);

         breathe_red();
         led_off();
         _delay_ms(CYCLE_PAUSE_MS);
     }
 }
