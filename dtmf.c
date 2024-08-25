//*****************************************************************************
// Title        : Pulse to tone (DTMF) converter
// Author       : Boris Cherkasskiy
//                http://boris0.blogspot.ca/2013/09/rotary-dial-for-digital-age.html
// Created      : 2011-10-24
//
// Modified     : Arnie Weber 2015-06-22
//                https://bitbucket.org/310weber/rotary_dial/
//                NOTE: This code is not compatible with Boris's original hardware
//                due to changed pin-out (see Eagle files for details)
//
// Modified     : Matthew Millman 2018-05-29
//                http://tech.mattmillman.com/
//                Cleaned up implementation, modified to work more like the
//                Rotatone product.
//
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//
// DTMF generator logic is loosely based on the AVR314 app note from Atmel
//
//*****************************************************************************

#include <stdbool.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "Attiny85v.h"

#include "dtmf.h"

#define TIMER_CLK_DIV1 0x01       ///< Timer clocked at F_CPU
#define TIMER_PRESCALE_MASK0 0x07 ///< Timer Prescaler Bit-Mask
#define NUM_SAMPLES 128           // Number of samples in lookup table

static void enable_pwm(void);

//************************** SIN TABLE *************************************
// Samples table : one period sampled on 128 samples and
// quantized on 7 bit
// auc_sin_param: A lookup table with 128 samples representing one period of a sine wave, quantized to 7 bits.
//**************************************************************************
const uint8_t auc_sin_param[NUM_SAMPLES] = {
    64, 67, 70, 73,
    76, 79, 82, 85,
    88, 91, 94, 96,
    99, 102, 104, 106,
    109, 111, 113, 115,
    117, 118, 120, 121,
    123, 124, 125, 126,
    126, 127, 127, 127,
    127, 127, 127, 127,
    126, 126, 125, 124,
    123, 121, 120, 118,
    117, 115, 113, 111,
    109, 106, 104, 102,
    99, 96, 94, 91,
    88, 85, 82, 79,
    76, 73, 70, 67,
    64, 60, 57, 54,
    51, 48, 45, 42,
    39, 36, 33, 31,
    28, 25, 23, 21,
    18, 16, 14, 12,
    10, 9, 7, 6,
    4, 3, 2, 1,
    1, 0, 0, 0,
    0, 0, 0, 0,
    1, 1, 2, 3,
    4, 6, 7, 9,
    10, 12, 14, 16,
    18, 21, 23, 25,
    28, 31, 33, 36,
    39, 42, 45, 48,
    51, 54, 57, 60};

//***************************  x_SW  ***************************************
// Fck = Xtal/prescaler
// Table of x_SW (excess 8): x_SW = ROUND(8 * N_samples * f * 510 / Fck)
// auc_frequency: A table mapping digits (0-9, *, #) to their corresponding high and low frequencies in DTMF.
//**************************************************************************

// high frequency
// 1209hz  ---> x_SW = 79
// 1336hz  ---> x_SW = 87
// 1477hz  ---> x_SW = 96
// 1633hz  ---> x_SW = 107
//
// low frequency
// 697hz  ---> x_SW = 46
// 770hz  ---> x_SW = 50
// 852hz  ---> x_SW = 56
// 941hz  ---> x_SW = 61
//
//      | 1209 | 1336 | 1477 | 1633
//  697 |   1  |  2   |   3  |   A
//  770 |   4  |  5   |   6  |   B
//  852 |   7  |  8   |   9  |   C
//  941 |   *  |  0   |   #  |   D

const uint8_t auc_frequency[12][2] =
    {
        {87, 61}, // 0
        {79, 46}, // 1
        {87, 46}, // 2
        {96, 46}, // 3
        {79, 50}, // 4
        {87, 50}, // 5
        {96, 50}, // 6
        {79, 56}, // 7
        {87, 56}, // 8
        {96, 56}, // 9
        {79, 61}, // *
        {96, 61}, // #
};

volatile uint32_t _g_delay_counter;           // Delay counter for sleep function
volatile uint8_t _g_stepwidth_high;           // step width of high frequency
volatile uint8_t _g_stepwidth_low;            // step width of low frequency
volatile uint16_t _g_current_sine_value_high; // position freq. A in LUT (extended format)
volatile uint16_t _g_current_sine_value_low;  // position freq. B in LUT (extended format)

void dtmf_init(void)
{
    _g_stepwidth_high = 0x00;
    _g_stepwidth_low = 0x00;

    _g_current_sine_value_high = 0;
    _g_current_sine_value_low = 0;

    _g_delay_counter = 0;
}

// Generate DTMF tone, duration x ms
void dtmf_generate_tone(int8_t digit, uint16_t duration_ms)
{
    // Disable interrupt
    GIMSK = 0;

    if (digit >= 0 && digit <= DIGIT_POUND)
    {
        // Standard digits 0-9, *, #
        _g_stepwidth_high = auc_frequency[digit][0];
        _g_stepwidth_low = auc_frequency[digit][1];
        enable_pwm();

        // Wait x ms
        sleep_ms(duration_ms);
    }
    else if (digit == DIGIT_BEEP)
    {
        // Beep ~1000Hz (66)
        _g_stepwidth_high = 66;
        _g_stepwidth_low = 0;
        enable_pwm();

        // Wait x ms
        sleep_ms(duration_ms);
    }
    else if (digit == DIGIT_BEEP_LOW)
    {
        // Beep ~500Hz (33)
        _g_stepwidth_high = 33;
        _g_stepwidth_low = 0;
        enable_pwm();

        // Wait x ms
        sleep_ms(duration_ms);
    }
    else if (digit == DIGIT_TUNE_ASC)
    {
        _g_stepwidth_high = 34; // C=523.25Hz
        _g_stepwidth_low = 0;
        enable_pwm();

        sleep_ms(duration_ms / 3);
        _g_stepwidth_high = 43; // E=659.26Hz
        sleep_ms(duration_ms / 3);
        _g_stepwidth_high = 51; // G=784Hz
        sleep_ms(duration_ms / 3);
    }
    else if (digit == DIGIT_TUNE_DESC)
    {
        _g_stepwidth_high = 51; // G=784Hz
        _g_stepwidth_low = 0;
        enable_pwm();

        sleep_ms(duration_ms / 3);
        _g_stepwidth_high = 43; // E=659.26Hz
        sleep_ms(duration_ms / 3);
        _g_stepwidth_high = 34; // C=523.25Hz
        sleep_ms(duration_ms / 3);
    }

    // Stop DTMF transmitting
    disable_pwm();
    _g_stepwidth_high = 0;
    _g_stepwidth_low = 0;

    // Set interrupt 0 and pin change interrupt
    GIMSK = _BV(INT0) | _BV(PCIE);
}

// Timer overflow interrupt service routine
ISR(TIMER0_OVF_vect)
{
    uint8_t sine_high;
    uint8_t sine_low;

    // A component (high frequency) is always used
    // move Pointer about step width ahead
    _g_current_sine_value_high += _g_stepwidth_high;
    // normalize Temp-Pointer
    uint16_t temp_sine_value_high = (int8_t)(((_g_current_sine_value_high + 4) >> 3) & (0x007F));
    sine_high = auc_sin_param[temp_sine_value_high];

    // B component (low frequency) is optional
    if (_g_stepwidth_low > 0)
    {
        // move Pointer about step width ahead
        _g_current_sine_value_low += _g_stepwidth_low;

        // normalize Temp-Pointer
        uint16_t temp_sine_value_low = (int8_t)(((_g_current_sine_value_low + 4) >> 3) & (0x007F));
        sine_low = auc_sin_param[temp_sine_value_low];
    }
    else
    {
        sine_low = 0;
    }

    // calculate PWM value: high frequency value + 3/4 low frequency value
    OCR0A = (sine_high + (sine_low - (sine_low >> 2)));
    _g_delay_counter++;
}
