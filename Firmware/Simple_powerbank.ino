#include "MP2722.h"
#include <avr/sleep.h>

MP2722 charger;
volatile bool eventDetected = false; // got an interrupt or not

bool faultDetected = false; // read a register, got fault/no fault, used for lighting up fault problem led

const int PIN_BATT  = PIN_PA3;  // Battery divider input
const int Battery_LED_PINS[] = {PIN_PB3, PIN_PA7, PIN_PA6, PIN_PA5}; // LED gpios
const int Problem_LED = PIN_PA4;

// Voltage levels for 1, 2, 3, 4 LEDs
const uint16_t BATT_THRESHOLDS[] = {696, 737, 778, 855}; // Pre-calculated ADC thresholds for: 3.4V, 3.6V, 3.8V, 4.18V

const unsigned long SWITCH_INTERVAL = 2500; // 2500 microseconds = 2.5ms
// Variables for Multiplexing
int currentLedIndex = 0;
unsigned long lastSwitchTime = 0;

// The Hardware ISR
// ISR = Interrupt Service Routine
// PORTA_PORT_vect = The hardware address for ANY interrupt on Port A
ISR(PORTA_PORT_vect) {
    // 1. Check WHICH pin fired (Critical if you have multiple interrupts on Port A)
    // We check if the Interrupt Flag (INTFLAGS) for Pin 2 is set.
    if (PORTA.INTFLAGS & PIN2_bm) {
        
        // 2. Your Logic
        eventDetected = true;

        // 3. Clear the Interrupt Flag (CRITICAL!)
        // Writing '1' to the flag bit actually clears it.
        // If you forget this, the interrupt will fire again immediately forever.
        PORTA.INTFLAGS = PIN2_bm; 
    }
}

void setup() {
    // 1. Setup Interrupt Pin (FALLING EDGE is mandatory for Pulse)
    Wire.begin();
    //PORTB.DIRSET = PIN2_bm; // stat pin, read to find out if  pinB2
    PORTA.DIRCLR = PIN2_bm; // Assuming INT is on PA2
    PORTA.PIN2CTRL = PORT_ISC_FALLING_gc; // to read the falling edge on interrupt pin

    PORTA.DIRSET = PIN5_bm | PIN6_bm | PIN7_bm;
    PORTB.DIRSET = PIN3_bm; // Setting up the leds pins as outputs

    PORTA.OUTCLR = PIN5_bm | PIN6_bm | PIN7_bm;
    PORTB.OUTCLR = PIN3_bm; // Setting low output for the led pins

    PORTA.DIRSET = PIN4_bm; // Problem led pin as output PIN_PA4
    VPORTA.OUT &= ~PIN4_bm; // set problem led pin output to low

    PORTA.DIRCLR = PIN3_bm; // PIN_PA3 set as input, for reading battery voltage divider
    // 1. Set Voltage Reference to Internal 2.5V
    // (Replaces analogReference(INTERNAL2V5))
    VREF.CTRLA |= VREF_ADC0REFSEL_2V5_gc;
    // 2. Configure the ADC Prescaler
    // We want the ADC clock to be between 50kHz and 1.5MHz.
    // Assuming 20MHz CPU clock, DIV32 gives ~625kHz (Perfect).
    ADC0.CTRLC = ADC_PRESC_DIV32_gc; // or DIV16_gc if running at 10MHz
    // 3. Enable the ADC (10-bit resolution is default)
    ADC0.CTRLA = ADC_ENABLE_bm | ADC_RESSEL_10BIT_gc;

    // 2. Start Charger
    if (charger.wasReset()) {
        charger.begin();
    }

    // setup the sleep workarounds of the attiny404
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sei();
    
}

void loop() {
    static unsigned long last_sleep_check = 0;
    showBatteryLevel();
    // 1. Handle Events
    if (eventDetected) {
        eventDetected = false;
        if (charger.processInterrupt()) {
            faultDetected = true;
        }
    }

    if (faultDetected) {
        digitalWrite(Problem_LED, (millis() / 250) % 2); // Fast blink
        if ((millis() / 250) % 10) {
            faultDetected = charger.checkFaultStatus();
        }
    }

    // 2. Periodically check "Canary" (in case Watchdog reset happened silently)
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 10000) {
        if (charger.wasReset()) {
            charger.begin(); // Re-apply settings
        }
        lastCheck = millis();
    }
    
    if (!charger.isChargingDischarging() && millis() - last_sleep_check > 15000) {
        charger.gotosleep();
    }
}

void showBatteryLevel() {
    // 1. Read ADC
    // We take a few samples and average them to remove noise
    unsigned long currentMicros = micros();
    uint16_t rawSum = 0;
    for(uint8_t i = 0; i < 5; i++) {
        rawSum += readBatteryADC(); // Direct hardware read
        delay(2);
    }
    uint16_t rawAvg = rawSum / 5;

    // 3. Update LEDs
    // Workings of the battery indication: the leds gets cycled so only one is actually on at a time, the leds light up based on if the battery level is higher than the set level for the charge. If the charge is below 3.4 the first leds flickers
    if (currentMicros - lastSwitchTime >= SWITCH_INTERVAL) {
        lastSwitchTime = currentMicros;

        currentLedIndex++;
        if (currentLedIndex >= 4) {
            currentLedIndex = 0;
        }

        // 2. Clear ALL LEDs (Atomic & Fast)
        // Instead of looping to find which ones to turn off, just kill them all.
        // (Assuming LEDs are on PA4, PA5, PB0, PB1 - update if different!)
        PORTA.OUTCLR = PIN4_bm | PIN5_bm;
        PORTB.OUTCLR = PIN0_bm | PIN1_bm;

        // 3. Logic: Should the CURRENT LED be ON?
        bool shouldTurnOn = false;

        // SPECIAL CASE: Critical Low Battery (Blink LED 0)
        if (currentLedIndex == 0 && rawAvg < 696) {
            // ">> 8" divides by 256. "& 1" keeps the last bit (odd/even).
            // Result: A perfect ~256ms blink with NO division library.
            if ((millis() >> 8) & 1) shouldTurnOn = true;
        }
        // NORMAL CASE: Check Voltage Threshold
        else if (rawAvg >= BATT_THRESHOLDS[currentLedIndex]) {
            shouldTurnOn = true;
        }

        // 4. Execute: Turn ON the specific LED using VPORT
        if (shouldTurnOn) {
            switch (currentLedIndex) {
                case 0: VPORTA.OUT |= PIN4_bm; break; // PA4
                case 1: VPORTA.OUT |= PIN5_bm; break; // PA5
                case 2: VPORTB.OUT |= PIN0_bm; break; // PB0
                case 3: VPORTB.OUT |= PIN1_bm; break; // PB1
            }
        }
    }
}

uint16_t readBatteryADC() { // quick analog pin that can be used strictly for the one pin of my battery
    // 1. Select the Input Channel (AIN3 is PA3)
    ADC0.MUXPOS = ADC_MUXPOS_AIN3_gc; 

    // 2. Start Conversion
    ADC0.COMMAND = ADC_STCONV_bm; 

    // 3. Wait for result (Blocking, but very fast ~20us)
    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm)); 

    // 4. Return the result
    return ADC0.RES; 
}