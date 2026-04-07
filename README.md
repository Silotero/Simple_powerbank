# Simple_powerbank
This is a portable battery bank capable of being charged and discharged at 15W.

The main IC is the MP2722. It does all the heavy lifting. It advertises that it can be sink or source, but mainly trying to be the source to make sure that phones never become sources for it (Retry source function). It takes care of device plugged in detection at the USB-C port and the unplugging of the device to stop the charge. It also is a buck-boost converter, that allows bucks the charge to be suitable for the batteries, as well as boosting the charge for the USB-C output. It can also detect many faults and advertise them to the Attiny404 microcontroller via I2C, after sending an interrupt signal on its interrupt pin.
There is and AP9101CK6 IC that takes care of battery overcharge, overdischarge and short.

The Attiny404 acts as an additional brain for the MP2722 unlocking more of its functions as well as creating a basic user interface. It operates 5 leds. 4 of them indicate the battery charge and the 5th one lights up to indicate if a problem has been detected by the MP2722.
Since the MP2722 has many registers that change how the device behaves, the Attiny404 must set them according to how the user wants the device to operate. At startup the microcontroller sets up all of the registers and then checks them periodically or after an interrupt to see if the device has for some reason reset. If it detects that it writes to the registers again.
