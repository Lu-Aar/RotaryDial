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

#define SLEEP_64MS 0x00
#define SLEEP_128MS 0x01
#define SLEEP_2S 0x02

/************ Variables ************/

/************ Functions ************/

void init();
void set_port_b_pull_up(uint8_t pin); // the pin to put the pull up on
void pwm_init(uint8_t pwm_pin);

// write and read from eeprom
void read_from_eeprom(int8_t *data, int *eeprom_address, uint8 size);
void write_to_eeprom(int8_t *data, int *data_location, uint8 size);

// watchdog timer start
void wdt_timer_start(uint8_t delay);

// watchdog timer stop
void wdt_stop(void);

// go to sleep till an interrupt triggers
void start_sleep(void);

// enable or disable the pwm
void enable_pwm(void);
void disable_pwm(void);

// sleep for a set amount of milliseconds
void sleep_ms(uint16_t msec);

#endif /* ATTINY85V_H */