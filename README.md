# 4e

4 encoders clones the functionality of the monome arc, giving you 4 encoders with push buttons to control monome hardware such as norns, ansible, and the trilogy modules.

##usb_names

The companion file `usb_names.c` allows you to customize some variables for how the device shows up on other computers/devices. This is necessary for the device to be recognized by some other monome devices.

You should modify SERIAL_NUM numbers to something unique, but do not change the "m" in the first position of the array: `{'m','1','2','3','4','5','6','7'}`
