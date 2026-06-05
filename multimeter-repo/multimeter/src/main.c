#include "msp430g2553.h"
// This code was developed by Mohammed Niazul Haque
// ----------------------------------------------------------------------------
// CONSTANT DEFINITIONS & SYSTEM MACROS
// ----------------------------------------------------------------------------

// Main operating modes for the meter
#define MODE_VOLTAGE      0   // Voltage measurement mode
#define MODE_RESISTANCE   1   // Resistance measurement mode
#define MODE_CONTINUITY   2   // Continuity test mode

// Voltage sub-modes
#define VOLT_V            0   // Show voltage in V
#define VOLT_MV           1   // Show voltage in mV
#define VOLT_BT           2   // Battery test mode

// Resistance sub-modes
#define RES_OHM           0   // Show resistance in ohms
#define RES_KOHM          1   // Show resistance in kilo-ohms

// Calibration values used in calculations
#define VREF_MV           3285UL   // Measured supply/reference voltage in mV
#define PULLUP_OHMS       39000UL  // Approximate internal pull-up resistance

// Hysteresis values to reduce flicker and sound glitches
#define CONT_SHORT_ON      50      // Short detected below this ADC value
#define CONT_SHORT_OFF     80      // Short cleared above this ADC value
#define BATT_GOOD_ON      1551UL   // Battery considered good above this voltage
#define BATT_BAD_ON       1499UL   // Battery considered bad below this voltage

// Number of samples used for averaging
#define ADC_SAMPLES        16

// Battery states
#define BATT_UNKNOWN       0
#define BATT_GOOD          1
#define BATT_BAD           2

// ----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
// ----------------------------------------------------------------------------
void init_device(void);
void delay_ms(unsigned int ms);
void lcd_init(void);
void lcd_write_cmd(unsigned char data);
void lcd_write_data(unsigned char data);
void lcd_write_string(char *str);
void lcd_set_cursor(unsigned char line, unsigned char position);
void lcd_clear_line(unsigned char line);
unsigned int adc_read(void);
unsigned int adc_read_averaged(void);
void update_display(unsigned char mode, unsigned char submode, unsigned int adc_code);
void speaker_beep_continuity(void);
void speaker_beep_good(void);
void speaker_beep_bad(void);
void speaker_off(void);

// ----------------------------------------------------------------------------
// GLOBAL VARIABLES
// ----------------------------------------------------------------------------
unsigned char g_mode       = MODE_VOLTAGE;   // Start in voltage mode
unsigned char g_submode    = VOLT_V;         // Default voltage format
unsigned char g_shorted    = 0;              // Continuity state flag
unsigned char g_batt_state = BATT_UNKNOWN;   // Battery test state flag

// ============================================================================
// MAIN PROGRAM
// ============================================================================
int main(void)
{
    unsigned int  adc_val;                    // Stores ADC reading
    unsigned char pb1_last = 1, pb2_last = 1;  // Previous button states
    unsigned char pb1_now,  pb2_now;           // Current button states

    // Basic startup setup
    init_device();
    lcd_init();

    // Default screen shown when the device starts
    lcd_set_cursor(1, 0);
    lcd_write_string("VOLTAGE         ");
    lcd_set_cursor(2, 0);
    lcd_write_string("0.00V           ");

    while (1)
    {
        // Read buttons here.
        // Buttons are active low, so pressed = 0 and released = 1.
        pb1_now = (P1IN & BIT1) ? 1 : 0;
        pb2_now = (P1IN & BIT2) ? 1 : 0;

        // --------------------------------------------------------------------
        // Button 1: change main mode
        // --------------------------------------------------------------------
        if (pb1_now == 0 && pb1_last == 1)
        {
            delay_ms(15); // Simple debounce delay
            pb1_now = (P1IN & BIT1) ? 1 : 0;

            if (pb1_now == 0)
            {
                // Move to next mode
                g_mode++;
                if (g_mode > MODE_CONTINUITY) g_mode = MODE_VOLTAGE;

                // Reset sub-settings when mode changes
                g_submode    = 0;
                g_shorted    = 0;
                g_batt_state = BATT_UNKNOWN;
                speaker_off();

                // Turn off ADC before changing pin settings
                ADC10CTL0 &= ~(ENC | ADC10ON);
                ADC10AE0  &= ~BIT0;

                // In voltage mode the input should be high impedance.
                // In resistance/continuity mode we use the pull-up resistor.
                if (g_mode == MODE_VOLTAGE)
                {
                    P1DIR &= ~BIT0;
                    P1REN &= ~BIT0;
                    P1OUT &= ~BIT0;
                }
                else
                {
                    P1DIR &= ~BIT0;
                    P1REN |=  BIT0;
                    P1OUT |=  BIT0;
                }

                // Re-enable ADC on A0
                ADC10AE0  |= BIT0;
                ADC10CTL0 |= ADC10ON | ENC;
                delay_ms(10); // Let the signal settle

                // Update top line of LCD
                lcd_set_cursor(1, 0);
                if      (g_mode == MODE_VOLTAGE)    lcd_write_string("VOLTAGE         ");
                else if (g_mode == MODE_RESISTANCE) lcd_write_string("RESISTANCE      ");
                else                                 lcd_write_string("CONTINUITY      ");

                lcd_clear_line(2);

                // Wait until button is released
                while ((P1IN & BIT1) == 0)
                {
                    delay_ms(1);
                }
                delay_ms(15);
                pb1_last = 1;
            }
        }
        else
        {
            pb1_last = pb1_now;
        }

        // --------------------------------------------------------------------
        // Button 2: change sub-mode
        // --------------------------------------------------------------------
        if (pb2_now == 0 && pb2_last == 1)
        {
            delay_ms(15); // Debounce
            pb2_now = (P1IN & BIT2) ? 1 : 0;

            if (pb2_now == 0)
            {
                if (g_mode == MODE_VOLTAGE)
                {
                    g_submode++;
                    if (g_submode > VOLT_BT) g_submode = VOLT_V;

                    // Battery test uses the pull-down to load the battery slightly
                    if (g_submode == VOLT_BT)
                    {
                        P1REN |=  BIT0;
                        P1OUT &= ~BIT0;
                    }
                    else
                    {
                        P1REN &= ~BIT0;
                    }

                    g_batt_state = BATT_UNKNOWN;
                    speaker_off();
                }
                else if (g_mode == MODE_RESISTANCE)
                {
                    g_submode++;
                    if (g_submode > RES_KOHM) g_submode = RES_OHM;
                }

                // Wait until button is released
                while ((P1IN & BIT2) == 0)
                {
                    delay_ms(1);
                }
                delay_ms(15);
                pb2_last = 1;
            }
        }
        else
        {
            pb2_last = pb2_now;
        }

        // --------------------------------------------------------------------
        // Read ADC
        // --------------------------------------------------------------------
        // Averaging is used for noisy modes to make the display steadier.
        if (g_mode == MODE_RESISTANCE || g_mode == MODE_CONTINUITY || (g_mode == MODE_VOLTAGE && g_submode == VOLT_BT))
            adc_val = adc_read_averaged();
        else
            adc_val = adc_read();

        // Update LCD and buzzer behavior
        update_display(g_mode, g_submode, adc_val);

        delay_ms(150); // Small pause so the display is readable
    }
}

// ============================================================================
// DEVICE INITIALIZATION
// ============================================================================
void init_device(void)
{
    WDTCTL = WDTPW + WDTHOLD; // Stop watchdog so it does not reset the MCU

    // Set DCO clock to 8 MHz using factory calibration values
    BCSCTL1 = CALBC1_8MHZ;
    DCOCTL  = CALDCO_8MHZ;

    // Make all pins GPIO first
    P1SEL  = 0x00; P1SEL2 = 0x00;
    P2SEL  = 0x00; P2SEL2 = 0x00;

    // Port 2 is used for the LCD data bus
    P2DIR  = 0xFF;
    P2OUT  = 0x00;

    // Port 1 outputs: buzzer, LCD E, LCD RS
    // Port 1 inputs: analog input, push buttons
    P1DIR  |=  (BIT5 | BIT6 | BIT7);
    P1DIR  &= ~(BIT0 | BIT1 | BIT2);
    P1OUT  &= ~(BIT5 | BIT6 | BIT7);

    // Enable pull-ups on the two buttons
    P1REN  |=  (BIT1 | BIT2);
    P1OUT  |=  (BIT1 | BIT2);

    // Start analog input in a clean state
    P1REN  &= ~BIT0;
    P1OUT  &= ~BIT0;

    // ADC10 setup for A0
    ADC10CTL1 = INCH_0 | ADC10SSEL_3;  // Channel A0, SMCLK source
    ADC10CTL0 = ADC10SHT_3 | ADC10ON;   // Long sample time, ADC on
    ADC10AE0  = BIT0;                   // Enable analog function on P1.0

    // TimerA used for tone generation on the buzzer
    TA0CTL  = TASSEL_2 | MC_1 | TACLR;  // SMCLK, up mode, clear timer
    TA0CCR0 = 3999;                     // Timer period for a base tone
}

// ============================================================================
// DELAY ROUTINE
// ============================================================================
void delay_ms(unsigned int ms)
{
    while (ms--)
        __delay_cycles(8000); // 8 MHz clock gives about 1 ms delay here
}

// ============================================================================
// ADC FUNCTIONS
// ============================================================================
unsigned int adc_read(void)
{
    ADC10CTL0 &= ~ENC;
    ADC10CTL0 |=  ENC | ADC10SC;
    while (ADC10CTL1 & ADC10BUSY);
    return ADC10MEM;
}

unsigned int adc_read_averaged(void)
{
    unsigned long sum = 0;
    unsigned char i;

    // Take several readings and average them
    for (i = 0; i < ADC_SAMPLES; i++)
    {
        ADC10CTL0 &= ~ENC;
        ADC10CTL0 |=  ENC | ADC10SC;
        while (ADC10CTL1 & ADC10BUSY);
        sum += ADC10MEM;
        delay_ms(1);
    }

    return (unsigned int)(sum / ADC_SAMPLES);
}

// ============================================================================
// BUZZER FUNCTIONS
// ============================================================================
void speaker_beep_continuity(void)
{
    TA0CCR0 = 3999;
    while (!(TA0CTL & TAIFG));
    TA0CTL &= ~TAIFG;
    P1OUT  ^= BIT5;
}

void speaker_beep_good(void)
{
    unsigned int period;
    unsigned int cycles;

    // Rising sweep sound for a good battery
    for (period = 8000; period >= 2000; period -= 200)
    {
        TA0CCR0 = period - 1;
        for (cycles = 0; cycles < 30; cycles++)
        {
            while (!(TA0CTL & TAIFG));
            TA0CTL &= ~TAIFG;
            P1OUT  ^= BIT5;
        }
    }
    speaker_off();
}

void speaker_beep_bad(void)
{
    unsigned int period;
    unsigned int cycles;

    // Falling sweep sound for a bad battery
    for (period = 2000; period <= 8000; period += 200)
    {
        TA0CCR0 = period - 1;
        for (cycles = 0; cycles < 30; cycles++)
        {
            while (!(TA0CTL & TAIFG));
            TA0CTL &= ~TAIFG;
            P1OUT  ^= BIT5;
        }
    }
    speaker_off();
}

void speaker_off(void)
{
    P1OUT &= ~BIT5;
}

// ============================================================================
// DISPLAY PROCESSING
// ============================================================================
void update_display(unsigned char mode, unsigned char submode, unsigned int adc_code)
{
    unsigned long mv;
    unsigned int  v, d1, d2;
    unsigned long ohms;
    char buf[17];
    unsigned char i;
    unsigned char new_batt;

    // Clear the buffer first so old characters do not stay on the LCD
    for (i = 0; i < 16; i++) buf[i] = ' ';
    buf[16] = '\0';

    // Convert ADC value into millivolts
    mv = (unsigned long)adc_code * VREF_MV / 1023UL;

    // ------------------------------------------------------------------------
    // Voltage mode
    // ------------------------------------------------------------------------
    if (mode == MODE_VOLTAGE)
    {
        v  = (unsigned int)(mv / 1000UL);
        d1 = (unsigned int)((mv % 1000UL) / 100UL);
        d2 = (unsigned int)((mv % 100UL)  / 10UL);

        if (submode == VOLT_V)
        {
            // Example format: 2.47V
            buf[0] = '0' + v;
            buf[1] = '.';
            buf[2] = '0' + d1;
            buf[3] = '0' + d2;
            buf[4] = 'V';
            speaker_off();
        }
        else if (submode == VOLT_MV)
        {
            // Example format: 2475mV
            unsigned int mv_int = (unsigned int)mv;
            buf[0] = '0' + (mv_int / 1000);
            buf[1] = '0' + ((mv_int % 1000) / 100);
            buf[2] = '0' + ((mv_int % 100) / 10);
            buf[3] = '0' + (mv_int % 10);
            buf[4] = 'm';
            buf[5] = 'V';
            speaker_off();
        }
        else
        {
            // Battery test mode
            if (mv < 200UL)
                new_batt = BATT_UNKNOWN;
            else if (mv >= BATT_GOOD_ON)
                new_batt = BATT_GOOD;
            else if (mv < BATT_BAD_ON)
                new_batt = BATT_BAD;
            else
                new_batt = g_batt_state;

            buf[0] = '0' + v;
            buf[1] = '.';
            buf[2] = '0' + d1;
            buf[3] = '0' + d2;
            buf[4] = 'V';
            buf[5] = '-';

            if (mv < 200UL)
            {
                buf[6]='-'; buf[7]='-'; buf[8]='-'; buf[9]='-'; buf[10]='-'; buf[11]='-';
                speaker_off();
                g_batt_state = BATT_UNKNOWN;
            }
            else if (new_batt == BATT_GOOD)
            {
                buf[6]='B'; buf[7]='a'; buf[8]='t'; buf[9]='t'; buf[10]='G'; buf[11]='o'; buf[12]='o'; buf[13]='d';

                lcd_set_cursor(2, 0);
                lcd_write_string(buf);

                if (g_batt_state != BATT_GOOD)
                {
                    g_batt_state = BATT_GOOD;
                    speaker_beep_good();
                }
                return;
            }
            else if (new_batt == BATT_BAD)
            {
                buf[6]='B'; buf[7]='a'; buf[8]='t'; buf[9]='t'; buf[10]='B'; buf[11]='a'; buf[12]='d';

                lcd_set_cursor(2, 0);
                lcd_write_string(buf);

                if (g_batt_state != BATT_BAD)
                {
                    g_batt_state = BATT_BAD;
                    speaker_beep_bad();
                }
                return;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Resistance mode
    // ------------------------------------------------------------------------
    else if (mode == MODE_RESISTANCE)
    {
        speaker_off();

        if (adc_code >= 1015)
        {
            // Very high ADC means open circuit
            buf[0] = 'O'; buf[1] = 'p'; buf[2] = 'e'; buf[3] = 'n';
        }
        else if (adc_code <= 5)
        {
            // Very low ADC means almost 0 ohms
            buf[0] = '0';
            buf[1] = ' ';
            buf[2] = (char)0xF4; // Omega symbol on this LCD character set
        }
        else
        {
            // Ohms calculation from voltage divider formula
            ohms = PULLUP_OHMS * (unsigned long)adc_code / (1023UL - adc_code);

            if (submode == RES_OHM)
            {
                unsigned long tmp = ohms;
                char digits[6];
                unsigned char nd = 0, d, pos = 0;

                if (tmp == 0)
                {
                    digits[nd++] = '0';
                }
                else
                {
                    while (tmp > 0 && nd < 6)
                    {
                        digits[nd++] = '0' + (tmp % 10);
                        tmp /= 10;
                    }
                }

                // Reverse digits because they were stored backwards
                for (d = nd; d > 0; d--)
                    buf[pos++] = digits[d-1];

                buf[pos] = (char)0xF4; // Omega
            }
            else
            {
                // Example format: 12.3kΩ
                unsigned int kohms_w = (unsigned int)(ohms / 1000UL);
                unsigned int kohms_f = (unsigned int)((ohms % 1000UL) / 100UL);

                buf[0] = '0' + (kohms_w / 10);
                buf[1] = '0' + (kohms_w % 10);
                buf[2] = '.';
                buf[3] = '0' + kohms_f;
                buf[4] = 'k';
                buf[5] = (char)0xF4;
            }
        }
    }

    // ------------------------------------------------------------------------
    // Continuity mode
    // ------------------------------------------------------------------------
    else
    {
        // Use hysteresis so the buzzer does not chatter near the threshold
        if (!g_shorted && adc_code < CONT_SHORT_ON)
            g_shorted = 1;
        else if (g_shorted && adc_code > CONT_SHORT_OFF)
            g_shorted = 0;

        if (g_shorted)
        {
            buf[0] = 'S'; buf[1] = 'h'; buf[2] = 'o'; buf[3] = 'r'; buf[4] = 't';
            lcd_set_cursor(2, 0);
            lcd_write_string(buf);
            speaker_beep_continuity();
            return;
        }
        else
        {
            buf[0]='N'; buf[1]='o'; buf[2]=' '; buf[3]='S'; buf[4]='h'; buf[5]='o'; buf[6]='r'; buf[7]='t';
            speaker_off();
        }
    }

    lcd_set_cursor(2, 0);
    lcd_write_string(buf);
}

// ============================================================================
// LCD DRIVER
// ============================================================================
void lcd_write_cmd(unsigned char data)
{
    P1OUT &= ~BIT7; // RS = 0 means command
    P2OUT  =  data;
    P1OUT |=  BIT6; // E pulse
    delay_ms(1);
    P1OUT &= ~BIT6;
    delay_ms(2);
}

void lcd_write_data(unsigned char data)
{
    P1OUT |=  BIT7; // RS = 1 means data
    P2OUT  =  data;
    P1OUT |=  BIT6;
    delay_ms(1);
    P1OUT &= ~BIT6;
    delay_ms(2);
}

void lcd_init(void)
{
    delay_ms(50);        // Wait for LCD to power up
    lcd_write_cmd(0x38); // 8-bit mode, 2 lines
    lcd_write_cmd(0x08); // Display off
    lcd_write_cmd(0x01); // Clear display
    delay_ms(2);
    lcd_write_cmd(0x06); // Cursor moves right after each character
    lcd_write_cmd(0x0C); // Display on, cursor off
}

void lcd_write_string(char *str)
{
    while (*str)
        lcd_write_data(*str++);
}

void lcd_set_cursor(unsigned char line, unsigned char position)
{
    if (line == 1)
        lcd_write_cmd(0x80 + position);
    else
        lcd_write_cmd(0xC0 + position);
}

void lcd_clear_line(unsigned char line)
{
    lcd_set_cursor(line, 0);
    lcd_write_string("                ");
}
