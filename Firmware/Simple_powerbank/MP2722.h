#ifndef MP2722_H
#define MP2722_H

#include <Arduino.h>
#include <Wire.h>

// --- Register Addresses ---

// Configuration bytes
#define REG_00h 0x00 // [7] REG_RST = 0 [6] EN_STAT_IB = 0 [5] EN_PG_NTC2 = 0 [4] LOCK_CHG = 0 [3] HOLDOFF_TMR = 1 [2,1] SW_FREQ = 01 [0] EN_VIN_TRK = 0
#define REG_00h_PERMANENT_SETTINGS 0x10 //  use at the beginning of the setup
#define REG_IIN_LIM     0x01 // [7,6,5] IIN_MODE = 000 [4,3,2,1,0] IIN_LIM = GETS UPDATED SET TO 11101 FOR 3A
#define REG_IIN_LIM_PERMANENT_SETTINGS 0x04
#define REG_CHG_CUR     0x02 // [7,6] VPRE (FAST CHARGE BATTERY FRESHOLD LEVEL) = 11 [5,4,3,2,1,0] ICC (BATTERY FAST CHARGE CURRENT) = 111111 FOR 5A
#define REG_CHG_CURR_PERMANENT_SETTINGS 0xFF
#define REG_PRE_CHG_TERM_CHG_CUR 0x03 // [7,6,5,4] IPRE = 1111 [3,2,1,0] ITERM = 1111
#define REG_PRE_CHG_TERM_CHG_CUR_PERMANENT_SETTINGS 0xFF
#define REG_VIN_LIM     0x04 // [7] VRECHG = 1 [6,5,4] ITRICKLE = 011 [3,2,1,0] VIN_LIM (THE VOLTAGE THAT VIN CAN SAG TO) = 1100 FOR 4.84V
#define REG_VIN_LIM_PERMANENT_SETTINGS 0xBC
#define REG_VBATT         0x05 // SETS THE BATT CHARGE VOLTAGE LIMIT, LEAVE AT DEFAULT FOR 4.2V
#define REG_VBATT_PERMANENT_SETTINGS 0x18
#define REG_VIN_OVP_SYS_MIN_TREG       0x06 // [7,6] VIN_OVP = 00 FOR 6.3V [5,4,3] SYS_MIN = 100 FOR 3.588V [2,1,0] TREG = 100 FOR 100 C THERMAL THRESHOLD OF THE MP2722, LEAVE AT DEFAULT FOR NOW 
#define REG_VIN_OVP_SYS_MIN_TREG_PERMANENT_SETTINGS 0x24
#define REG_WATCHDOG     0x07 // [7] IB_EN = 0 [6] WATCHDOG_RST = 0 [5,4] WATCHDOG = 00 TO DISABLE THE TIMER [3] EN_TERM = 1 [2] EN_TMR2X = 1 [1,0] CHG_TIMER = 11 SETS MAX CHARGE TIME FOR 15 HRS
#define REG_WATCHDOG_PERMANENT_SETTINGS 0x0F
#define REG_VBOOST 0x08 // 
#define REG_VBOOST_PERMANENT_SETTINGS 0x7E
#define REG_CC_CFG_AUTOOTG_EN_BOOST_EN_BUCK_EN_CHG 0x09 // [7] RESERVED [6,5,4] CC_CFG = 100 SETS FOR DRP WITH TRY.SNK [3] AUTOOTG = 1 [2] EN_BOOST = 0 [1] EM_BUCK = 1 [0] EN_CHG = 1
#define REG_CC_CFG_AUTOOTG_EN_BOOST_EN_BUCK_EN_CHG_PERMANENT_SETTINGS 0x4B
#define REG_CONFIG    0x0A // [7,6] RESERVED [5] AUTODPDM = 1 D+/D- DETECTION AUTOMATICALLY STARTS AFTER VIN_GD = 1 [4] FORCEDPDM = 0 [3,2] RP_CFG = 10 SETS CURRENT RATING ADVERTISEMENT 3A [1,0] FORCE_CC = 00
#define REG_CONFIG_PERMANENT_SETTINGS 0x28
#define REG_HV_ADAPTER      0x0B // [7,6,5] RESERVED [4] HVEN = 1 ENABLES HIGH-VOLTAGE ADAPTER [3] HVUP = 0 [2] HVDOWN = 0 HVREQ = 00
#define REG_HV_ADAPTER_PERMANENT_SETTINGS 0x10
#define REG_BATT_LOW     0x0C // [7] RESERVED [6] NTC1_ACTION = 1 [5] NTC2_ACTION = 0 [4] BATT_OVP_EN = 1 [3,2] BATT_LOW = 11 3.2V [1] BOOST_STP_EN = 1 [0] BOOST_OTP_EN = 1
#define REG_BATT_LOW_PERMANENT_SETTINGS 0x5F
//#define REG_WARM_JEITA 0x0D // leave default
//#define REG_VHOT_VWARM_VCOOL 0x0E // sets temperatures for working, leave default
#define REG_IMPEDANCE_TESTING 0x0F // tests for whether humidity or water got into the port, think of how to use it
#define REG_INT_MASKING 0x10 // [7,6] RESERVED [5] THERM_MASK = 1 [4] MASK_DPM = 1 [3] MASK_TOPOFF = 1 [2] MASK_CC_INT = 0 [1] MASK_BATT_LOW = 0 [0] MASK_DEBUG_AUDIO = 1   
#define REG_INT_MASKING_PERMANENT_SETTINGS 0x39
#define REG_DPDM_STAT 0x11 // register that tells what kind of charger/device has been plugged in based on D+/D- detection, READ ONLY
#define REG_SOURCE_GOOD 0x12 // Indicates whether input source type detection has finished, is he input source valid, READ ONLY
#define REG_CHG_BOOST_FAULT_OVP 0x13 // charging status, boost fault status, OVP status, READ ONLY
#define REG_NTC_TEMP_STATUS 0x14 // indicates NTC temperature status, READ ONLY
#define REG_CC_STATUS 0x15 // Indicates CC status, READ ONLY
#define REG_OTHER 0x16 // some other random stuff, READ ONLY

class MP2722 {
  public:
    // Constructor
    MP2722(uint8_t address = 0x3F); // Default I2C address

    // Initialization
    bool begin(); // Set up all the register values as they should be, those that stay permanently. Write at the beggining, or if you saw the MP2722 might have reset, by reading a certain register value
    
    // The "Main Loop" Worker
    // Call this if(interruptFlag) is true
    bool processInterrupt(); 

    // Manual Features
    bool checkMoisture();       // Returns true if wet
    void requestHighVoltage_9V();  // Try to get 9V from QC charger
    void requestHighVoltage_12V();  // Try to get 9V from QC charger
    bool wasReset();            // Check "Canary" to see if power was lost

    void gotosleep();
    bool checkFaultStatus();
    bool isChargingDischarging();

    // Setters (if needed manually)
    //void setInputCurrent(float amps);
    //void setChargeCurrent(float amps);

  private:
    uint8_t _addr;
    
    // Helpers
    void writeReg(uint8_t reg, uint8_t val);
    void writeBurst(uint8_t startReg, uint8_t length, uint8_t* data);
    uint8_t readReg(uint8_t reg);
    void readBurst(uint8_t startReg, uint8_t count, uint8_t* buffer);

    // Event Handlers
    //void handlePlugEvent(uint8_t dpdm_stat, uint8_t cc_stat);
    //void handleFaults(uint8_t fault_reg);
};

#endif