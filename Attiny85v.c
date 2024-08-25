/*******************************************
Attiny85v

Functions controlling the Atyiny85v
*******************************************/

#include "Attiny85v.h"

static void init(void)
{
    // Program clock prescaller to divide + frequency by 1
    // Write CLKPCE 1 and other bits 0
    CLKPR = _BV(CLKPCE);

    // Write prescaler value with CLKPCE = 0
    CLKPR = 0;

    // Enable pull-ups
    PORTB |= (_BV(PIN_DIAL) | _BV(PIN_PULSE));

    // Disable unused modules to save power
    PRR = _BV(PRTIM1) | _BV(PRUSI) | _BV(PRADC);
    ACSR = _BV(ACD);

    // Configure pin change interrupt
    MCUCR = _BV(ISC01) | _BV(ISC00); // Set INT0 for falling edge detection
    GIMSK = _BV(INT0) | _BV(PCIE);   // Added INT0
    PCMSK = _BV(PIN_DIAL) | _BV(PIN_PULSE);

    // Enable interrupts
    sei();
}

static void read_from_eeprom(int8_t *data, int *data_location, uint8 size)
{
    eeprom_read_block(data, &data_location, size);
}

static void write_to_eeprom(int8_t *data, int *data_location, uint8 size)
{
    eeprom_update_block(data, &data_location, size);
}

static void wdt_timer_start(uint8_t delay)
{
    wdt_reset();
    cli();
    MCUSR = 0x00;
    WDTCR |= _BV(WDCE) | _BV(WDE);
    switch (delay)
    {
    case SLEEP_64MS:
        WDTCR = _BV(WDIE) | _BV(WDP1);
        break;
    case SLEEP_128MS:
        WDTCR = _BV(WDIE) | _BV(WDP1) | _BV(WDP0);
        break;
    case SLEEP_2S:
        WDTCR = _BV(WDIE) | _BV(WDP0) | _BV(WDP1) | _BV(WDP2); // 2048ms
        break;
    }
    sei();
}

static void wdt_stop(void)
{
    wdt_reset();
    cli();
    MCUSR = 0x00;
    WDTCR |= _BV(WDCE) | _BV(WDE);
    WDTCR = 0x00;
    sei();
}

static void start_sleep(void)
{
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli(); // stop interrupts to ensure the BOD timed sequence executes as required
    sleep_enable();
    sleep_bod_disable(); // disable brown-out detection (good for 20-25µA)
    sei();               // ensure interrupts enabled so we can wake up again
    sleep_cpu();         // go to sleep
    sleep_disable();     // wake up here
}

// Enable PWM output by configuring compare match mode - non inverting PWM
static void enable_pwm(void)
{
    TCCR0A |= _BV(COM0A1);
    TCCR0A &= ~_BV(COM0A0);
}

// Disable PWM output (compare match mode 0) and force it to 0
static void disable_pwm(void)
{
    TCCR0A &= ~_BV(COM0A1);
    TCCR0A &= ~_BV(COM0A0);
    PORTB &= ~_BV(PIN_PWM_OUT);
}

// Wait x ms
void sleep_ms(uint16_t msec)
{
    _g_delay_counter = 0;
    set_sleep_mode(SLEEP_MODE_IDLE);
    while (_g_delay_counter <= msec * T0_OVERFLOW_PER_MS)
    {
        sleep_mode();
    }
}