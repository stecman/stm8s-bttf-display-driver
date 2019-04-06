#include "stm8s.h"

#include <stdbool.h>
#include <string.h>

#include "ascii.h"
#include "interface.h"

// Pin assignments
#define PD2_DIGIT_SHIFT_CLK (1<<2)
#define PD3_DIGIT_STORE_CLK (1<<3)
#define PD6_DIGIT_ENABLE (1<<6)

#define PB4_SEG_SHIFT_CLK (1<<4)
#define PB5_SEG_STORE_CLK (1<<5)
#define PC7_SEG_ENABLE (1<<7)

#define PA1_SHARED_DATA (1<<1)

#define PD4_TIME_SEPARATOR (1<<4)
#define PA2_SEG_DP_ENABLE (1<<2)
#define PC3_SEG_AM_ENABLE (1<<3)
#define PC4_SEG_PM_ENABLE (1<<4)

#define PD5_LIGHT_SENSE (1<<5)

// Global data
#define NUM_DIGITS 13

// Store of the current segment values for each digit
static uint16_t segmentData[NUM_DIGITS];

// Mask representing which digits have their decimal point enabled
// Digits are stored LSB to MSB (digit 0 is bit 0)
static uint16_t decimalPointData = 0x0;

// Configure all pins and set their default values
inline void setup()
{
    // Shared serial data line as output
    GPIOA->DDR |= PA1_SHARED_DATA;
    GPIOA->CR1 |= PA1_SHARED_DATA;
    GPIOA->CR2 |= PA1_SHARED_DATA;

    // Decimal point enable (active low)
    GPIOA->DDR |= PA2_SEG_DP_ENABLE;
    GPIOA->CR1 |= PA2_SEG_DP_ENABLE;
    GPIOA->CR2 |= PA2_SEG_DP_ENABLE;
    GPIOA->ODR |= PA2_SEG_DP_ENABLE;

    // Segment clocks as outputs (true open drain pins with external pull-up)
    GPIOB->DDR |= PB4_SEG_SHIFT_CLK | PB5_SEG_STORE_CLK;

    // Default PORTB high to avoid sinking current from the pull-ups
    GPIOB->ODR |= PB4_SEG_SHIFT_CLK | PB5_SEG_STORE_CLK;

    // Segment enable as output
    GPIOC->DDR |= PC7_SEG_ENABLE;
    GPIOC->CR1 |= PC7_SEG_ENABLE;
    GPIOC->CR2 |= PC7_SEG_ENABLE;

    // Digit clocks as outputs
    GPIOD->DDR |= PD2_DIGIT_SHIFT_CLK | PD3_DIGIT_STORE_CLK | PD6_DIGIT_ENABLE;
    GPIOD->CR1 |= PD2_DIGIT_SHIFT_CLK | PD3_DIGIT_STORE_CLK | PD6_DIGIT_ENABLE;
    GPIOD->CR2 |= PD2_DIGIT_SHIFT_CLK | PD3_DIGIT_STORE_CLK | PD6_DIGIT_ENABLE;

    // Default enables off (active low)
    GPIOC->ODR |= PC7_SEG_ENABLE;
    GPIOD->ODR |= PD6_DIGIT_ENABLE;

    // Time separator (active high)
    GPIOD->DDR |= PD4_TIME_SEPARATOR;
    GPIOD->CR1 |= PD4_TIME_SEPARATOR;
    GPIOD->CR2 |= PD4_TIME_SEPARATOR;

    // AM LED (active high)
    GPIOC->DDR |= PC3_SEG_AM_ENABLE;
    GPIOC->CR1 |= PC3_SEG_AM_ENABLE;
    GPIOC->CR2 |= PC3_SEG_AM_ENABLE;

    // PM LED (active high)
    GPIOC->DDR |= PC4_SEG_PM_ENABLE;
    GPIOC->CR1 |= PC4_SEG_PM_ENABLE;
    GPIOC->CR2 |= PC4_SEG_PM_ENABLE;

    // Light sensor (input)
    ADC1->TDRL |= PD5_LIGHT_SENSE;
    ADC1->TDRH |= PD5_LIGHT_SENSE;
}

// Bring the ENABLE pins of all shift registers low to drive their outputs
inline void enableOutputs()
{
    GPIOC->ODR &= ~PC7_SEG_ENABLE;
    GPIOD->ODR &= ~PD6_DIGIT_ENABLE;
}

// Bring the ENABLE pins of all shift registers high to tristate their outputs
inline void disableOutputs()
{
    GPIOC->ODR |= PC7_SEG_ENABLE;
    GPIOD->ODR |= PD6_DIGIT_ENABLE;
}

// Clock the digit STORE clock once to move the shift register contents to its outputs
inline void digitStore()
{
    GPIOD->ODR |= PD3_DIGIT_STORE_CLK;
    GPIOD->ODR &= ~PD3_DIGIT_STORE_CLK;
}

// Configure the digit register to sink the digit at the given index
// Sinks above 14-16 aren't connected, so these bits are ignored
inline void digitSelect(int8_t digit)
{
    for (int8_t i = 12; i >= 0; --i) {
        GPIOD->ODR &= ~PD2_DIGIT_SHIFT_CLK;

        if (i == digit) {
            GPIOA->ODR |= PA1_SHARED_DATA;
        } else {
            GPIOA->ODR &= ~PA1_SHARED_DATA;
        }

        GPIOD->ODR |= PD2_DIGIT_SHIFT_CLK;
    }
}

// Clock the segment STORE clock once to move the shift register contents to its outputs
inline void segmentStore()
{
    // Stop floating pin
    GPIOB->DDR |= PB5_SEG_STORE_CLK;

    // Inverted as the PORTB pins need to idle high to avoid sinking current from their pull-ups
    GPIOB->ODR &= ~PB5_SEG_STORE_CLK;
    GPIOB->ODR |= PB5_SEG_STORE_CLK;

    // Float pin
    GPIOB->DDR &= ~PB5_SEG_STORE_CLK;
}

// Clock out a byte to the segment register (MSB first)
inline void segmentSend(uint16_t data)
{
    // Stop floating pin
    GPIOB->DDR |= PB4_SEG_SHIFT_CLK;

    for (uint8_t bit = 0; bit < 16; ++bit) {
        GPIOB->ODR &= ~PB4_SEG_SHIFT_CLK;

        if ((data & 0x8000) != 0) {
            // __asm__("bset 0x5000, #1");
            GPIOA->ODR |= PA1_SHARED_DATA;
        } else {
            // __asm__("bres 0x5000, #1");
            GPIOA->ODR &= ~PA1_SHARED_DATA;
        }

        GPIOB->ODR |= PB4_SEG_SHIFT_CLK;

        data <<= 1;
    }

    // Idle segment shift clock high to avoid sinking current from pull-ups
    GPIOB->ODR |= PB4_SEG_SHIFT_CLK;

    // Float pin
    GPIOB->DDR &= ~PB4_SEG_SHIFT_CLK;
}

inline void enableDecimalPoint() {
    GPIOA->ODR &= ~PA2_SEG_DP_ENABLE;
}

inline void disableDecimalPoint() {
    GPIOA->ODR |= PA2_SEG_DP_ENABLE;
}

// The segment registers on the board are ordered for simpler PCB layout.
// This maps logical order (A, B, C, ...) to the order the shift registers are wired
uint16_t mapSegmentsToHardwareOrder(uint16_t segments)
{
    // Map bit in input (array index) to bit in output
    static const uint8_t logicalIndexToHardwareBit[] = {
        8,  // A
        0,  // B
        2,  // C
        5,  // D
        6,  // E
        7,  // F
        14, // G
        10, // H
        9,  // K
        15, // M
        1,  // N
        3,  // P
        4,  // R
        12, // S
        13, // T
        11, // U
    };

    uint16_t output = 0;

    for (uint8_t i = 0; i < 16; ++i) {
        output |= ((segments & 0x1) << logicalIndexToHardwareBit[i]);
        segments >>= 1;
    }

    // Invert the output as segments drivers are active low
    return ~output;
}

// Set the entire segment memory to its OFF state
inline void clearSegmentMemory()
{
    memset(segmentData, 0xFF, sizeof(segmentData));
    segmentSend(0xFFFF);
    segmentStore();
}

inline void setOutputDigit(int8_t digit)
{
    // Select digit
    digitSelect(digit);

    // Send segment data from memory
    segmentSend(segmentData[digit]);

    // Disable digit sinking
    GPIOD->ODR |= PD6_DIGIT_ENABLE;

    digitStore();
    segmentStore();

    // Enable digit sinking to start display
    GPIOD->ODR &= ~PD6_DIGIT_ENABLE;

    // Draw decimal point if enabled
    if (decimalPointData & (1 << digit)) {
        enableDecimalPoint();
    } else {
        disableDecimalPoint();
    }
}

void enableTimeSeparator(bool enabled)
{
    // Enable/disable PWM output on time separator LEDs
    if (enabled) {
        TIM2->CCER1 |= TIM2_CCER1_CC1E;
    } else {
        TIM2->CCER1 &= ~TIM2_CCER1_CC1E;
    }
}

/**
 * Set which of the AM/PM leds are turned on
 */
void enablePeriodLed(BTTF_Period period)
{
    // Enable/disable PWM output for AM LED
    if (period & BTTF_Period_AM) {
        TIM1->CCER2 |= TIM1_CCER2_CC3E;
    } else {
        TIM1->CCER2 &= ~TIM1_CCER2_CC3E;
    }

    // Enable/disable PWM output for PM LED
    if (period & BTTF_Period_PM) {
        TIM1->CCER2 |= TIM1_CCER2_CC4E;
    } else {
        TIM1->CCER2 &= ~TIM1_CCER2_CC4E;
    }
}

/**
 * Configure the PWM duty cycle for the AM, PM and time-separator LEDs
 * @param brightness - value from 0 to 7
 */
void setLedBrightness(uint8_t brightness)
{
    // CIE1931 brightness table to provide linear brightness steps
    static const uint16_t cie_brightness_table[8] = {
        1, 146, 465, 1070, 2054, 3507, 5523, 8192
    };

    // Clamp brightness to available levels
    if (brightness >= 8) {
        brightness = 7;
    }

    const uint16_t compareValue = cie_brightness_table[brightness];

    // Set the value TIM1 and TIM2 will count to
    {
        const uint16_t overflow = 8192;

        TIM1->ARRH = overflow >> 8;
        TIM1->ARRL = overflow;
        TIM2->ARRH = overflow >> 8;
        TIM2->ARRL = overflow;
    }

    // Set the timer value the LEDs will be enabled under (duty cycle)
    {
        const uint8_t compareValueHigh = compareValue >> 8;
        const uint8_t compareValueLow = compareValue & 0xFF;

        TIM1->CCR3H = compareValueHigh;
        TIM1->CCR3L = compareValueLow;
        TIM1->CCR4H = compareValueHigh;
        TIM1->CCR4L = compareValueLow;
        TIM2->CCR1H = compareValueHigh;
        TIM2->CCR1L = compareValueLow;
    }

    // Set the output mode for each LED
    TIM1->CCMR3 = TIM1_OCMODE_PWM1; // AM LED
    TIM1->CCMR4 = TIM1_OCMODE_PWM1; // PM LED
    TIM2->CCMR1 = TIM2_OCMODE_PWM1; // Time separator

    // Allow TIM1 outputs to work (the "main output enable" bit defaults to off on this timer)
    TIM1->BKR |= TIM1_BKR_MOE;

    // Enable timers
    TIM1->CR1 |= TIM1_CR1_CEN;
    TIM2->CR1 |= TIM2_CR1_CEN;
}

void configureDisplayTimer()
{
    // Prescale by 128 to a 125KHz clock (assuming 16MHz main clock)
    TIM4->PSCR = TIM4_PRESCALER_128;

    // Load refresh delay into timer (multiples of 8uS)
    // 5KHz gives a non-flickery display without updating unnecessarily often
    TIM4->ARR = 25;

    // Enable timer
    TIM4->CR1 |= TIM4_CR1_CEN;
}

int main(void)
{
    // Temporary phrase to load onto the display
    static const char phrase[] = "OCT2619850122";

    // Configure the clock for maximum speed on the 16MHz HSI oscillator
    // (After reset the clock output is divided by 8)
    CLK->CKDIVR = 0x0;

    // Setup pins
    setup();

    // Set segment registers all high (active low)
    clearSegmentMemory();

    // Convert string to segment output data for display
    for (uint8_t i = 0; i < NUM_DIGITS; ++i) {
        if (phrase[i] == '\0') {
            break;
        }

        segmentData[i] = mapSegmentsToHardwareOrder(
            charTo16Segment(phrase[i])
        );
    }

    // Setup display refresh rate
    configureDisplayTimer();

    // Default LED brightness to a low level
    setLedBrightness(1);

    // Enable shift register outputs
    enableOutputs();

    // Main loop
    while (1) {
        static int8_t digit = 0;

        // If display timer has overflowed, switch output to the next digit
        if (TIM4->SR1 & TIM4_SR1_UIF) {
            TIM4->SR1 &= ~TIM4_SR1_UIF;

            setOutputDigit(digit);

            // Increment to the next digit
            ++digit;
            if (digit >= NUM_DIGITS) {
                digit = 0;
            }
        }
    }
}
