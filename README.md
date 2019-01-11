# 4e

4e is simply four encoders. It speaks monome serial and clones the functionality of the monome arc. This provides an easy means to control monome hardware such as norns (and soon - ansible and the trilogy modules) or to interact with monome applications in Max or PureData. 

It's based on an inexpensive Teensy microcontroller, with encoders and a 1.2" OLED display to provide visual feedback. The display currently shows the arc led ring information as four 64-segment "bars" instead.

## expansion

The 4e pcb is designed to be expanded with the addition of a second 4e pcb. This allows you to create different layouts like 1x8 encoders, 2x4 encoders, or 4 encoders + 4 buttons


## usb_names

The companion file `usb_names.c` allows you to customize some variables for how the device shows up on other computers/devices. This is necessary for the device to be recognized by some computers, etc.

You should modify SERIAL_NUM numbers to something unique, but do not change the "m" in the first position of the array: `{'m','1','2','3','4','5','6','7'}`. Do not change the MANUFACTURER_NAME.


