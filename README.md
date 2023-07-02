# ESP32 HomeKit Fan

A project to convert a conventional fan into a HomeKit-compatible smart fan
using an ESP32 and the 
[ESP HomeKit SDK](https://github.com/espressif/esp-homekit-sdk).

< INSERT FAN + HOMEKIT + ESP32 PICTURE HERE >

## Getting Started

To get started using this ESP-IDF project, first make sure that you have ESP-IDF
correctly set up and ready to use on your system. Also ensure you know the port
used by your ESP32 when you plug it in. Then:

```bash
# Move into the ESP-IDF project folder
$ cd esp32_homekit_fan

# Set the correct ESP32 target, ESP32-C3 in my case
$ idf.py set-target esp32c3

# Open up the TUI config menu for ESP-IDF
$ idf.py menuconfig

# Go to "Compiler options"->"Assertion level"->"Disabled" to avoid RMT problems

# Build, flash, and then monitor output for the project!
$ idf.py -p <YOUR_PORT> build flash monitor

# Pay attention to the output, it will have QR codes for configuring Wi-Fi and
# the HomeKit accessory.
```

If you're wondering why you had to disable asserts to build, it comes down to
quirks of the RMT component. I found that if a button is held too long, it seems
to force signal truncation which upsets the RMT component.

Despite this, I only pay attention to the first part of the signal that arrives,
so I am able to safely ignore this assertion.

## Making Your Own Smart-Devices

Something nice about this project is that it can apply to a few different
use-cases.

First, within `main.c (app_main)` you'll need to update some of the initialization
using new components that fit your hardware. I would recommend initializing
output hardware before HomeKit, then giving some time before initializing input
hardware (as I have done).

Next, you'll want to create new event types for your specific project as I did
in `main.h`. These should line up with every command you expect your device to
receive.

Next, in `homekit.c (HomeKit_init)` you'll need to make changes in order to align
the setup with the specific smart-device you are implementing. This includes
setting the correct service UUID for `hap_serv_create()` and then creating
associated characteristics with `hap_serv_add_char()`.

Also in `homekit.c`, you'll need to update `HomeKit_write_callback()` to reflect
the events associated with your project.

Best of luck!
