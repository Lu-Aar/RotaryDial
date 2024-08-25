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

/************ Variables ************/

/************ Functions ************/

static void init(void);

static void write_to_eeprom(int8_t *data, int *data_location, uint8 size);

static void wdt_timer_start(uint8_t delay);

static void wdt_stop(void);

static void start_sleep(void);

static void enable_pwm(void);

static void disable_pwm(void);

void sleep_ms(uint16_t msec);

#endif /* ATTINY85V_H */