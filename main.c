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
//                Rotatone commercial product.
//
// This code is distributed under the GNU Public License
// which can be found at http://www.gnu.org/licenses/gpl.txt
//
// DTMF generator logic is loosely based on the AVR314 app note from Atmel
//
//*****************************************************************************

// Uncomment to build with reverse dial
// #define NZ_DIAL

#include <stdbool.h>
#include <stdint.h>

#include "Attiny85v.h"
#include "dtmf.h"

#define SPEED_DIAL_SIZE 32

#define STATE_DIAL       0x00
#define STATE_SPECIAL_L1 0x01
#define STATE_SPECIAL_L2 0x02
#define STATE_PROGRAM_SD 0x03

#define F_NONE              0x00
#define F_DETECT_SPECIAL_L1 0x01
#define F_DETECT_SPECIAL_L2 0x02
#define F_WDT_AWAKE         0x04

#define SPEED_DIAL_COUNT  9                      // 9 Positions in total (Redail(3),4,5,6,7,8,9,0, normal dial)
#define SPEED_DIAL_REDIAL (SPEED_DIAL_COUNT - 2) // Redial is always the second last position
#define NORMAL_DIAL       (SPEED_DIAL_COUNT - 1) // Normal dial is always the last position

#define L2_STAR   1
#define L2_POUND  2
#define L2_REDIAL 3

#define DAIL_TIMEOUT_MS 1000 // Timeout for dialed digit in ms

typedef struct
{
    uint8_t  state;
    uint8_t  flags;
    bool     dial_pin_state;
    uint8_t  speed_dial_index;
    uint8_t  speed_dial_digit_index;
    int8_t   speed_dial_digits[SPEED_DIAL_SIZE];
    int8_t   dialed_digit;
    uint16_t dial_timeout_counter;
    bool     dial_timeout_active;
    bool     call_in_progress; // Flag to indicate if a call is in progress
} runstate_t;

static void process_dialed_digit(runstate_t* rs);
static void dial_speed_dial_number(int8_t* speed_dial_digits, int8_t index);
static void write_current_speed_dial(int8_t* speed_dial_digits, int8_t index);
static void dial_number(int8_t* dial_digits);

// Map speed dial numbers to memory locations
const int8_t _g_speed_dial_loc[] = { 0, -1 /* 1 - * */, -1 /* 2 - # */, -1 /* 3 - Redial */, 1, 2, 3, 4, 5, 6 };

int8_t EEMEM _g_speed_dial_eeprom[SPEED_DIAL_COUNT][SPEED_DIAL_SIZE] = { [0 ...(SPEED_DIAL_COUNT - 1)][0 ... SPEED_DIAL_SIZE - 1] = DIGIT_OFF };
runstate_t   _g_run_state;

static uint8_t _g_normal_dial_index = 0; // Index for normal dial

int main(void)
{
    runstate_t* rs = &_g_run_state;
    bool        dial_pin_prev_state;

    init(PIN_DIAL, PIN_PULSE);
    enable_interrupts();

    set_port_b_pull_up(PIN_DIAL);
    set_port_b_pull_up(PIN_PULSE);

    // Wait for the decoupling capacitors to charge
    wdt_timer_start(SLEEP_128MS);
    start_sleep();
    wdt_stop();

    pwm_init(PIN_PWM_OUT);
    dtmf_init();

    // Local dial status variables
    rs->state                  = STATE_DIAL;
    rs->dial_pin_state         = true;
    rs->flags                  = F_NONE;
    rs->speed_dial_digit_index = 0;
    rs->speed_dial_index       = 0;
    dial_pin_prev_state        = true;

    for (uint8_t i = 0; i < SPEED_DIAL_SIZE; i++)
        rs->speed_dial_digits[i] = DIGIT_OFF;

    _g_normal_dial_index    = 0;
    rs->dial_timeout_active = false;

    while (1)
    {
        rs->dial_pin_state = bit_is_set(PINB, PIN_DIAL);

        if (dial_pin_prev_state != rs->dial_pin_state)
        {
            if (!rs->dial_pin_state)
            {
                // Dial just started
                // Enable special function detection
                rs->flags |= F_DETECT_SPECIAL_L1;
                rs->dialed_digit = 0;

                wdt_timer_start(SLEEP_64MS);
                start_sleep();
            }
            else
            {
                // Disable SF detection (should be already disabled)
                rs->flags = F_NONE;

                // Check that we detect a valid digit
                if (rs->dialed_digit <= 0 || rs->dialed_digit > 10)
                {
                    // Should never happen - no pulses detected OR count more than 10 pulses
                    rs->dialed_digit = DIGIT_OFF;

                    // Do nothing
                    wdt_timer_start(SLEEP_64MS);
                    start_sleep();
                }
                else
                {
                    // Got a valid digit - process it
#ifdef NZ_DIAL
                    // NZPO Phones only. 0 is same as GPO but 1-9 are reversed.
                    rs->dialed_digit = (10 - rs->dialed_digit);
#else
                    if (rs->dialed_digit == 10) rs->dialed_digit = 0; // 10 pulses => 0
#endif
                    wdt_timer_start(SLEEP_128MS);
                    start_sleep();
                    wdt_stop();

                    process_dialed_digit(rs);
                    if (!rs->call_in_progress)
                    {
                        rs->dial_timeout_counter = 0;
                        rs->dial_timeout_active  = true;
                        _g_normal_dial_index     = 0;
                    }
                }
            }
        }
        else
        {
            if (rs->dial_pin_state)
            {
                // Rotary dial at the rest position
                // Reset all variables
                rs->state        = STATE_DIAL;
                rs->flags        = F_NONE;
                rs->dialed_digit = DIGIT_OFF;
            }
        }

        dial_pin_prev_state = rs->dial_pin_state;

        // Don't power down if special function detection is active
        if (rs->flags & F_DETECT_SPECIAL_L1)
        {
            // Put MCU to sleep - to be awoken either by pin interrupt or WDT
            wdt_timer_start(SLEEP_2S);
            start_sleep();

            // Special function mode detected?
            if (rs->flags & F_WDT_AWAKE)
            {
                // SF mode detected
                rs->flags &= ~F_WDT_AWAKE;
                rs->state = STATE_SPECIAL_L1;
                rs->flags &= ~F_DETECT_SPECIAL_L1;
                rs->flags |= F_DETECT_SPECIAL_L2;

                // Indicate that we entered L1 SF mode with short beep
                dtmf_generate_tone(DIGIT_BEEP_LOW, 200);
            }
        }
        else if (rs->flags & F_DETECT_SPECIAL_L2)
        {
            // Put MCU to sleep - to be awoken either by pin interrupt or WDT
            wdt_timer_start(SLEEP_2S);
            start_sleep();

            if (rs->flags & F_WDT_AWAKE)
            {
                // SF mode detected
                rs->flags &= ~F_WDT_AWAKE;
                rs->state = STATE_SPECIAL_L2;
                rs->flags &= ~F_DETECT_SPECIAL_L2;

                // Indicate that we entered L2 SF mode with asc tone
                dtmf_generate_tone(DIGIT_TUNE_ASC, 200);
            }
        }
        // else
        // {
        //     // Don't need timer - sleep to power down mode
        //     power_down();
        // }
        else if ((rs->dial_timeout_counter >= DAIL_TIMEOUT_MS) && rs->dial_timeout_active)
        {
            dial_speed_dial_number(rs->speed_dial_digits, NORMAL_DIAL);
            rs->dial_timeout_counter = 0;
            rs->dial_timeout_active  = false;
            _g_normal_dial_index     = 0;
        }
    }

    return 0;
}

static void process_dialed_digit(runstate_t* rs)
{
    switch (rs->state)
    {
        case STATE_DIAL:
        {
            // Standard (no speed dial, no special function) mode
            // Generate DTMF code
            if (!rs->call_in_progress)
            {
                add_number_to_dial(rs->dialed_digit);
            }
            else
            {
                dtmf_generate_tone(rs->dialed_digit, DTMF_DURATION_MS);
            }

            if (rs->speed_dial_digit_index < SPEED_DIAL_SIZE)
            {
                // During regular dial always save into the 'Redial' position of the speed dial memory
                rs->speed_dial_digits[rs->speed_dial_digit_index] = rs->dialed_digit;
                rs->speed_dial_digit_index++;

                write_current_speed_dial(rs->speed_dial_digits, SPEED_DIAL_REDIAL);
            }
            break;
        }
        case STATE_SPECIAL_L1:
        {
            if (rs->dialed_digit == L2_STAR)
            {
                // SF 1-*

                if (!rs->call_in_progress)
                {
                    add_number_to_dial(DIGIT_STAR);
                }
                else
                {
                    dtmf_generate_tone(DIGIT_STAR, DTMF_DURATION_MS);
                }
                rs->state = STATE_DIAL;
            }
            else if (rs->dialed_digit == L2_POUND)
            {
                // SF 2-#
                if (!rs->call_in_progress)
                {
                    add_number_to_dial(DIGIT_POUND);
                }
                else
                {
                    dtmf_generate_tone(DIGIT_POUND, DTMF_DURATION_MS);
                }
                rs->state = STATE_DIAL;
            }
            else if (rs->dialed_digit == L2_REDIAL)
            {
                // SF 3 (Redial)
                dial_speed_dial_number(rs->speed_dial_digits, SPEED_DIAL_REDIAL);
            }
            else if (_g_speed_dial_loc[rs->dialed_digit] >= 0)
            {
                // Call speed dial number
                dial_speed_dial_number(rs->speed_dial_digits, _g_speed_dial_loc[rs->dialed_digit]);
            }
            break;
        }
        case STATE_SPECIAL_L2:
        {
            if (_g_speed_dial_loc[rs->dialed_digit] >= 0)
            {
                rs->speed_dial_index       = _g_speed_dial_loc[rs->dialed_digit];
                rs->speed_dial_digit_index = 0;

                for (uint8_t i = 0; i < SPEED_DIAL_SIZE; i++)
                    rs->speed_dial_digits[i] = DIGIT_OFF;

                rs->state = STATE_PROGRAM_SD;
            }
            else
            {
                // Not a speed dial position. Revert back to ordinary dial
                rs->state = STATE_DIAL;
            }
            break;
        }
        case STATE_PROGRAM_SD:
        {
            // Do we have too many digits entered?
            if (rs->speed_dial_digit_index >= SPEED_DIAL_SIZE)
            {
                // Exit speed dial mode
                rs->state = STATE_DIAL;
                // Beep to indicate that we done
                dtmf_generate_tone(DIGIT_TUNE_DESC, 800);
            }
            else
            {
                // Next digit
                rs->speed_dial_digits[rs->speed_dial_digit_index] = rs->dialed_digit;
                rs->speed_dial_digit_index++;

                // Generic beep - do not gererate DTMF code
                dtmf_generate_tone(DIGIT_BEEP_LOW, DTMF_DURATION_MS);
            }

            // Write SD on every digit so user can hang up to save
            write_current_speed_dial(rs->speed_dial_digits, rs->speed_dial_index);
            break;
        }
        default:
        {
            break;
        }
    }
}

// Dial speed dial number (it erases current SD number in the global structure)
static void dial_speed_dial_number(int8_t* speed_dial_digits, int8_t index)
{
    if (index >= 0 && index < SPEED_DIAL_COUNT)
    {
        read_from_eeprom(speed_dial_digits, &_g_speed_dial_eeprom[index][0], SPEED_DIAL_SIZE);

        // for (uint8_t i = 0; i < SPEED_DIAL_SIZE; i++)
        // {
        //     // Dial the number
        //     // Skip dialing invalid digits
        //     if (speed_dial_digits[i] >= 0 && speed_dial_digits[i] <= DIGIT_POUND)
        //     {
        //         dtmf_generate_tone(speed_dial_digits[i], DTMF_DURATION_MS);
        //         // Pause between DTMF tones
        //         sleep_ms(DTMF_DURATION_MS);
        //     }
        // }
        dial_number(speed_dial_digits);
    }
}

static void dial_number(int8_t* dial_digits)
{
    for (uint8_t i = 0; i < SPEED_DIAL_SIZE; i++)
    {
        // Dial the number
        // Skip dialing invalid digits
        if (dial_digits[i] >= 0 && dial_digits[i] <= DIGIT_POUND)
        {
            dtmf_generate_tone(dial_digits[i], DTMF_DURATION_MS);
            // Pause between DTMF tones
            sleep_ms(DTMF_DURATION_MS);
        }
    }
}

void add_number_to_dial(int8_t digit)
{
    write_to_eeprom(&digit, &_g_speed_dial_eeprom[8][_g_normal_dial_index], 1);
    _g_normal_dial_index++;
}

static void write_current_speed_dial(int8_t* speed_dial_digits, int8_t index)
{
    if (index >= 0 && index < SPEED_DIAL_COUNT)
    {
        // If dialed index SPEED_DIAL_FIRST => using array index 0
        write_to_eeprom(speed_dial_digits, &_g_speed_dial_eeprom[index][0], SPEED_DIAL_SIZE);
    }
}

// Handler for external interrupt on INT0 (PB2, pin 7)
ISR(INT0_vect)
{
    if (!_g_run_state.dial_pin_state)
    {
        // Disabling SF detection
        _g_run_state.flags = F_NONE;
        // A pulse just started
        _g_run_state.dialed_digit++;
    }
}

// Interrupt initiated by pin change on any enabled pin
ISR(PCINT0_vect) {}

// Handler for any unspecified 'bad' interrupts
ISR(BADISR_vect)
{
    // Do nothing, just wake up MCU
}

ISR(WDT_vect)
{
    _g_run_state.flags |= F_WDT_AWAKE;
}

volatile uint16_t t0_ovf_count = 0;
ISR(TIMER0_OVF_vect)
{
    t0_ovf_count++;
    if (t0_ovf_count >= 16)
    { // 16 * 64μs = 1.024ms (close to 1ms)
        t0_ovf_count = 0;
        _g_run_state.dial_timeout_counter++; // if you want to count ms here
    }
}