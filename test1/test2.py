import hid

# Vendor ID and Product ID of the HID device
VENDOR_ID = 0x17cc
PRODUCT_ID = 0x1620

def main():
    # Open the HID device
    try:
        device = hid.device()
        device.open(VENDOR_ID, PRODUCT_ID)
        print(f"Connected to HID device {VENDOR_ID:04x}:{PRODUCT_ID:04x}")

        # Send data to the HID device
        device.write([0xa0, 0x00, 0x0])
        device.write([0xa0, 0x00, 0x10])

        # Continuously read and display incoming HID packets
        while True:
            data = device.read(64)
            if data:
                print(f"Received data: {data}")

    except Exception as e:
        print(f"Failed to connect to HID device: {e}")

    finally:
        # Close the HID device
        device.close()

if __name__ == "__main__":
    main()
