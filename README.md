# ESP32 HomeKit Fan

A project to convert a conventional fan into a HomeKit-compatible smart fan
using an ESP32 and the 
[ESP HomeKit SDK](https://github.com/espressif/esp-homekit-sdk).

< INSERT FAN + HOMEKIT + ESP32 PICTURE HERE >

## Project Progression 

This project was born out of a desire to automate usage of a home fan along with
the discovery of the HomeKit SDK provided by Espressif. 

### 1. The First Mistake (Of Many)

Initially, the goal was to find a way to connect an ESP32 to the in-built 
microcontroller within the fan. Using a multimeter, I found the pin assignments
for the front-fascia buttons and LEDs and began thinking about how I could
actuate them using the ESP32. However, this plan was foiled by my not-so-steady
hands which caused a short-circuit which killed the microcontroller.

### 2. Pivot!

At this point, I realized I would need to bypass the now-dead microcontroller
entirely and pivot the project. I did some research into how I could control an
AC motor using the ESP32 and yielded some concerning results. The majority of
people warned against trying to accomplish such a thing, as mixing AC and DC
electronics could lead to some bad situations.

### 3. Modulating AC Power

At first, I settled on a small board which included a small IC, optocoupler, and
beefy triac. It stated that it was able to modulate AC power to a load using a
PWM signal, which was partially true. It required some semi-complex code to
trigger changes based on the signal crossing the zero point and blah blah blah.
I deemed this too complex for my needs.

### 4. If It Ain't Broke...

To come up with my next plan, I studied how the original microcontroller
accomplished the same goal. I found that it had four triacs which were connected
to four separate power wires that ran to the fan motor. After some research, it
seemed that this was a semi-common method for modulating the speed of an AC
motor. Based upon which wire is connected to power, the fan motor runs at a
different speed.

### 5. A Taste Of Success 

With this knowledge in my brain, I needed a way to programmatically connect
individual wires to power. After looking online, it seemed relays were the
defacto method for doing so. I purchased some relays, wrote some test code with
an Arduino Uno, and boom! I was now able to control the fan motor in a rather
simple fashion.

### 6. Hardware Ain't So Hard 

At this point, I spent a great deal of time building a makeshift board on which
I could mount all my components for later packaging. This part was rather
boring, as it involved laying out the components, burning myself with a
soldering iron, and stripping numerous wires. However, by the end I had a nifty
circuit board with which I could connect all my components together.

### 7. Software Ain't So... Soft?

With hardware concerns mostly out of the way, I turned my focus to designing my
software implementation for the project. It was clear that I would want to split
up the codebase into its logical components which were:
1. Main task entry
2. Generic event handling
3. HomeKit setup/callbacks 
4. IR remote control
5. Front-fascia buttons
6. Fan-speed relays
7. Front-fascia LEDs

### X. It's A Remote Michael, How Hard Could It Be?

## Getting Started

< INCLUDE SOME INFO ON BUILD PROCESS, INTRO TO HARDWARE? >

## Making Your Own Smart-Devices

< INCLUDE SOME INFO ON WHAT PARTS CAN BE REUSED >
