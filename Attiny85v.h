/*******************************************
Attiny85v

Functions controlling the Atyiny85v
*******************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <avr/eeprom.h>

#ifndef ATTINY85V_H
#define ATTINY85V_H

#define SLEEP_64MS  0x00
#define SLEEP_128MS 0x01
#define SLEEP_2S    0x02

#define PIN_PWM_OUT PB0 // PB0 (OC0A) as PWM output
#define PIN_DIAL    PB1
#define PIN_PULSE   PB2

// PWM frequency = 4Mhz/256 = 15625Hz; overflow cycles per MS = 15
#define T0_OVERFLOW_PER_MS 15

#define TIMER_CLK_DIV1       0x01 ///< Timer clocked at F_CPU
#define TIMER_PRESCALE_MASK0 0x07 ///< Timer Prescaler Bit-Mask

/************ Variables ************/

/************ Functions ************/

void init(uint8_t pin1, uint8_t pin2);

// interrupt settings
void enable_interrupts(void);
void disable_interrupts(void);

void set_port_b_pull_up(uint8_t pin); // the pin to put the pull up on
void pwm_init(uint8_t pwm_pin);

void set_pwm_duty_cycle(uint8_t duty_cycle);

// write and read from eeprom
void read_from_eeprom(int8_t* data, int8_t* eeprom_address, uint8_t size);
void write_to_eeprom(int8_t* data, int8_t* data_location, uint8_t size);

// watchdog timer start
void wdt_timer_start(uint8_t delay);

// watchdog timer stop
void wdt_stop(void);

// go to sleep till an interrupt triggers
void start_sleep(void);
void power_down(void);

// enable or disable the pwm
void enable_pwm(void);
void disable_pwm(uint8_t pin);

// sleep for a set amount of milliseconds
void sleep_ms(uint16_t msec);

void enable_interrupt_0(void);
void enable_pin_change_interrupt(void);
void disable_pin_change_interrupts(void);

void reset_delay_counter(void);
void increase_delay_counter(void);

#endif /* ATTINY85V_H */