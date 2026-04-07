#include "MP2722.h"
#include <avr/sleep.h>
MP2722::MP2722(uint8_t address) {
    _addr = address; // Save the address so writeReg can use it later
}

const uint8_t STARTUP_CONFIG[13] = {
    0x10,
    0x04,
    0xFF,
    0xFF,
    0xBC,
    0x18,
    0x24,
    0x0F,
    0x7E,
    0x4B,
    0x28,
    0x10,
    0x5F
};

bool MP2722::begin() {
    // 1. "Ping" the chip
    // Try to read the Part ID (or any static register)
    Wire.beginTransmission(_addr);
    if (Wire.endTransmission() != 0) {
        return false; // NO REPLY! Wiring is wrong or chip is dead.
    }

    // 2. Chip is alive! Now configure it.
    writeBurst(0x00, 13, (uint8_t*)STARTUP_CONFIG);
    writeReg(0x10, 0x39);

    return true; // Success
}

void MP2722::gotosleep() {
    sleep_enable();
    sleep_cpu();
    sleep_disable();
}

bool MP2722::checkFaultStatus() {
    if (readReg(0x13) & 0x1F) { // a fault has occured with boost or charging
        return 1;
    }
    return 0;
}

bool MP2722::isChargingDischarging() {
    uint8_t status_registers[2]; 
    status_registers[0] = readReg(0x13); // charge status, boost fault, charge fault status register
    status_registers[1] = readReg(0x09); // charge status, boost fault, charge fault status register
    if (status_registers[0] & 0xE0 || status_registers[1] & 0x4) { // checks whether charging or discharging is happening
        return 1;
    }
    return 0;
}

bool MP2722::processInterrupt() { // Set what happens when the pulse was detected on interrupt pin
    uint8_t status_registers[2]; 
    
    status_registers[0] = readReg(0x13); // charge status, boost fault, charge fault status register
    status_registers[1] = readReg(0x16); // battery low status register bit 4

    // --- Decode the Status ---

    if (status_registers[1] & 0x10) {
        gotosleep(); // battery low interrupt was sent, boost was turned off, put device to sleep
        return 0;
    }

    if (status_registers[0] & 0x1F) { // a fault has occured with boost or charging
        return 1;
    }
    return 0;
}

// Check if the chip might have reset by checking if one of the registers permanent settings have changed
bool MP2722::wasReset() {
    uint8_t check_register = readReg(REG_VBOOST);
    if (check_register == REG_VBOOST_PERMANENT_SETTINGS)
    {
        return 0;
    }
    return 1;
}

// void MP2722::handlePlugEvent(uint8_t dpdm, uint8_t cc) {
//     // 1. Check for High Voltage Adapter (Samsung/QC)
//     // DPDM_STAT 1001 = High Voltage
//     if (((dpdm >> 4) & 0x0F) == 0x09) {
//         // Found a Fast Charger! Request 9V.
//         requestHighVoltage();
//     }
    
//     // 2. Check for USB-C 3.0A
//     // CC_STAT 11 = vRd-3.0
//     else if ((cc & 0x03) == 0x03 || (cc & 0xC0) == 0xC0) {
//         // Pure USB-C 3A. Set limit to 3.0A
//         setInputCurrent(3.0);
//     }
    
//     // 3. Fallback (Standard USB)
//     else {
//         setInputCurrent(2.0); // Or safe default
//     }
// }

// void MP2722::requestHighVoltage() {
//     // Write 01 to HVREQ (Bits 1:0 of Reg 0x0B)
//     // This asks for 9V.
//     uint8_t val = readReg(REG_HV_CTRL);
//     writeReg(REG_HV_CTRL, (val & 0xFC) | 0x01);
// }

// bool MP2722::checkMoisture() {
//     // 1. Enable Current Source (Bit 6)
//     // You'll need the register address for VIN_SRC_EN (Reg 0x0F usually)
//     uint8_t regVal = readReg(0x0F);
//     writeReg(0x0F, regVal | 0x40);
    
//     delay(10); // Wait for stabilization

//     // 2. Read ADC (Pseudo-code, check datasheet for ADC reg)
//     // If voltage is > 0.5V but < 4.0V, it's likely water.
//     // float voltage = readADC(ADC_VIN); 

//     // 3. Disable Current Source
//     writeReg(0x0F, regVal & ~0x40);

//     // return (voltage > 0.5 && voltage < 4.0);
//     return false; // Placeholder
// }

// ---------------------------------------------------------
// Private Helper: Write a single byte to a specific register
// ---------------------------------------------------------
void MP2722::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr); // Start talking to MP2722
    Wire.write(reg);               // Say: "I want to write to this Register"
    Wire.write(val);               // Say: "Here is the value"
    Wire.endTransmission();        // Stop talking
}

// Private Helper: Write multiple bytes to the registers
void MP2722::writeBurst(uint8_t startReg, uint8_t length, uint8_t* data) {
    Wire.beginTransmission(_addr);
    Wire.write(startReg); // Tell chip where to start writing (e.g., Reg 0x01)
    
    // Loop through the array and send bytes one by one
    // The MP2722 auto-increments the address internally after each byte
    for (uint8_t i = 0; i < length; i++) {
        Wire.write(data[i]);
    }
    
    Wire.endTransmission();
}

// ---------------------------------------------------------
// Private Helper: Read a single byte from a register
// ---------------------------------------------------------
uint8_t MP2722::readReg(uint8_t reg) {
    // Step 1: Tell the chip WHICH register we want to read
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.endTransmission(false); // "false" means send a Restart, not a Stop (keeps connection alive)

    // Step 2: Request 1 byte of data
    Wire.requestFrom(_addr, (uint8_t)1);
    
    // Step 3: Grab the byte
    if (Wire.available()) {
        return Wire.read();
    }
    return 0; // Return 0 if communication failed
}

// ---------------------------------------------------------
// Private Helper: Read multiple bytes at once (Burst Read)
// ---------------------------------------------------------
void MP2722::readBurst(uint8_t startReg, uint8_t count, uint8_t* buffer) {
    // Step 1: Tell the chip where to start reading
    Wire.beginTransmission(_addr);
    Wire.write(startReg);
    Wire.endTransmission(false); // Restart

    // Step 2: Request 'count' bytes
    Wire.requestFrom(_addr, count);

    // Step 3: Read them all into the array
    for (uint8_t i = 0; i < count; i++) {
        if (Wire.available()) {
            buffer[i] = Wire.read();
        } else {
            buffer[i] = 0; // Safety for failed reads
        }
    }
}