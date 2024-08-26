/*******************************************
Attiny85v

Functions controlling the Atyiny85v
*******************************************/

#include "Attiny85v.h"

void init()
{
    // Program clock prescaller to divide + frequency by 1
    // Write CLKPCE 1 and other bits 0
    CLKPR = _BV(CLKPCE);

    // Write prescaler value with CLKPCE = 0
    CLKPR = 0;

    // Disable unused modules to save power
    PRR  = _BV(PRTIM1) | _BV(PRUSI) | _BV(PRADC);
    ACSR = _BV(ACD);

    // Configure pin change interrupt
    MCUCR = _BV(ISC01) | _BV(ISC00); // Set INT0 for falling edge detection
    GIMSK = _BV(INT0) | _BV(PCIE);   // Added INT0
    PCMSK = _BV(PIN_DIAL) | _BV(PIN_PULSE);

    // Enable interrupts
    sei();
}

void set_port_b_pull_up(uint8_t pin)
{
    PORTB |= _BV(pin);
}

void pwm_init(uint8_t pwm_pin)
{
    // his line enables the Timer/Counter0 Overflow Interrupt. The _BV(TOIE0) macro sets the TOIE0 bit in the Timer Interrupt Mask Register (TIMSK), allowing the timer overflow interrupt to occur.
    TIMSK = _BV(TOIE0); // Int T0 Overflow enabled

    // This sets the Waveform Generation Mode bits (WGM00 and WGM01) in the Timer/Counter Control Register A (TCCR0A) to configure Timer/Counter0 for 8-bit Fast PWM mode.
    TCCR0A = _BV(WGM00) | _BV(WGM01); // 8Bit PWM; Compare/match output mode configured later

    // This sets the clock source and prescaler for Timer/Counter0. TIMER_CLK_DIV1 means the timer is clocked at the CPU frequency without any prescaling.
    TCCR0B = TIMER_PRESCALE_MASK0 & TIMER_CLK_DIV1;

    // This initializes the Timer/Counter0 register to 0.
    TCNT0 = 0;

    // This initializes the Output Compare Register A (OCR0A) to 0, which will be used to set the PWM duty cycle.
    OCR0A = 0;

    // This sets the data direction of the PWM output pin (OC0A) to output. The _BV(PIN_PWM_OUT) macro sets the corresponding bit in the Data Direction Register B (DDRB).
    DDRB |= _BV(pwm_pin); // PWM output (OC0A pin)
}

void set_pwm_duty_cycle(uint8_t duty_cycle)
{
    OCR0A = duty_cycle;
}

void read_from_eeprom(int8_t* data, int* eeprom_address, uint8 size)
{
    eeprom_read_block(data, eeprom_address, size);
}

void write_to_eeprom(int8_t* data, int* eeprom_address, uint8 size)
{
    eeprom_update_block(data, eeprom_address, size);
}

void wdt_timer_start(uint8_t delay)
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
            WDTCR = _BV(WDIE) | _BV(WDP1) | _BV(WDP0) | _BV(WDP2); // 2048ms
            break;
    }
    sei();
}

void wdt_stop(void)
{
    wdt_reset();
    cli();
    MCUSR = 0x00;
    WDTCR |= _BV(WDCE) | _BV(WDE);
    WDTCR = 0x00;
    sei();
}

void start_sleep(void)
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
void enable_pwm(void)
{
    TCCR0A |= _BV(COM0A1);
    TCCR0A &= ~_BV(COM0A0);
}

// Disable PWM output (compare match mode 0) and force it to 0
void disable_pwm(void)
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

void enable_interrupt_0(void)
{
    GIMSK |= _bv(INT0);
}
void enable_pin_change_interrupt(void)
{
    GIMSK |= _bv(PCIE);
}

void disable_interrupts(void)
{
    GIMSK = 0;
}