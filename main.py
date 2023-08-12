#!/usr/bin/env python3

import evdev
import math
from evdev import ecodes, InputDevice, UInput, AbsInfo

def main():
    device = None
    for input in (InputDevice(fn) for fn in evdev.list_devices()):
        if input.name == "Usb KeyBoard Usb KeyBoard":
            device = input
            break
    device.grab()
    for event in device.read_loop():
        if event.type == 1:
            isUp = event.value == 0
            isDown = event.value == 1
            if isUp or isDown:
                state = "down" if isDown else "up"
                print(f"button {event.code} is {state}")

if __name__ == '__main__':
    main()
