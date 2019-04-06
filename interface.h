#pragma once

/**
 * AM/PM ("period") LED selection
 */
typedef enum BTTF_Period {
    BTTF_Period_None = 0,
    BTTF_Period_AM = 0x1,
    BTTF_Period_PM = 0x2,
} BTTF_Period;