# jn-live
Software for Linux for live playing LV2 instrument plugins. With a user interface on Native Instruments Komplete Kontrol S61 Mk2 and S88 Mk2 keyboards.

I use this for playing live, but it's not ready for public consumption yet.

To use the komplete kontrol mk2 as a control service, udev rules need to be added to give permission to the user to access the USB device:

Create a file in /etc/udev/rules.d/, for example: /etc/udev/rules.d/99-local.rules 
with the following contents:

SUBSYSTEM=="usb", ATTRS{idVendor}=="17cc", MODE="0660", GROUP="users"
KERNEL=="hiddev*", ATTRS{idVendor}=="17cc", MODE="0660", GROUP="users"

Assuming you are part of the 'users' group. 17cc is the vendor id for Native Instruments.
