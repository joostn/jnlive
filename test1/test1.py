import usb1

# Replace these with your device's VID and PID
VID = 0x17cc
PID = 0x1620
INTERFACE = 3
ENDPOINT = 3


def hex_string_to_byte_array(hex_string):
    # Remove any extra spaces and split the string by spaces
    hex_values = hex_string.split()
    # Convert hex values to a byte array
    byte_array = bytearray(int(value, 16) for value in hex_values)
    return byte_array


data_to_send = hex_string_to_byte_array(
"""
84 00 00 60 00 00 00 00
a0 00 80 00 01 e0 00 ff 

02 00 00 00 00 00 00 10 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 

02 00 00 e0 00 00 00 10 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 

02 00 00 e0 00 00 00 10 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 

02 00 00 e0 00 00 00 10 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 
00 00 ff ff ff ff ff ff 
ff ff ff ff ff ff 00 00 

02 00 00 00 03 00 00 00 40 00 00 00

""")

#84 00 dd 60 00 00 00 00
#xx xx yy yy ss ss 00 19 
#02 00 oo oo 00 00 00 

#dd: 00 or 01 (display index)
#ss ss: 01 e0 (line stride)
#oo oo: start offeset (e.g. 480 is line 1)
# xx, yy: top left
def main():
    with usb1.USBContext() as context:
        handle = context.openByVendorIDAndProductID(
            VID, PID,
            skip_on_error=True,
        )

        if handle is None:
            print(f'Error: Device with VID {VID:#04x} and PID {PID:#04x} not found')
            return

        try:
            # Detach kernel driver if necessary
            if handle.kernelDriverActive(INTERFACE):
                handle.detachKernelDriver(INTERFACE)

            # Claim the interface
            handle.claimInterface(INTERFACE)

            # Send data
            handle.bulkWrite(ENDPOINT, bytes(data_to_send))

            handle.bulkWrite(ENDPOINT, bytes(data_to_send))

            print(f'Successfully sent data')

        except usb1.USBError as e:
            print(f'USB error: {e}')

        finally:
            # Release the interface
            handle.releaseInterface(INTERFACE)

            # Reattach the kernel driver if it was previously attached
            if handle.kernelDriverActive(INTERFACE):
                handle.attachKernelDriver(INTERFACE)

if __name__ == '__main__':
    main()

