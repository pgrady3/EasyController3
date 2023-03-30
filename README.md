# EasyController3

Note, the code and documentation surrounding this are still being updated. However, the board are hardware are final.

![Assembled Controller](/docs/side.jpg)

The EasyController3 is the next evolution of the successful [EasyController2](https://github.com/pgrady3/EasyController2). The new version is cheaper, more efficient, and uses well-stocked components.

## Improvements over the EasyController2

* Reduced BOM cost from $70 to $36
* Uses more efficient synchronous PWM. 5-10% greater total efficiency under partial throttle operation
* Improved from duty-cycle control to torque-control, improving controllability and reducing current spikes
* Lowered quiescent power from 960 mW to 280 mW in a typical installation
* Based on widely available Raspberry Pi Pico microcontroller
* Uses more widely available surface-mount components

## Getting Started

For a comprehensive guide on how to build your own EasyController, please see the [getting started guide](/docs/getting-started.md) (under construction).

## Background

The EasyController3 is a a simple brushless (BLDC) sensored motor controller (also known as a motor inverter or motor drive). It is intended to power vehicles such as electric bikes, skateboards, or Eco-Marathon vehicles in the 50-1000 Watt range.

The EasyController family was built to to fill a gap in publicly released motor controller designs. The [VESC](https://vesc-project.com/) is an extremely complex but capable design, but cannot be modified without extensive expertise. Alternatively, other DIY designs are not capable of the power levels necessary to drive vehicles. The EasyController3 is designed to be as simple yet still highly functional. It deliberately omits more complex features such as sensorless or field-oriented control, however these features can be added with modification. This project is meant as a learning tool and a foundation to potentially build more complex designs from.

## Specifications
* 8v-60v operation
* 20A continuous current, 40A burst (more with heatsinking)
* Socket for automotive fuse
* Automatic hall sensor identification
* Simple, well-documented code. <300 lines with comments
* Torque or duty-cycle control
* Raspberry Pi Pico microcontroller, compatible with Windows/Mac/Linux
* DGD2304 gate drivers
* SIR5802 MOSFETS
* BOM cost of $36 USD
* Open source under MIT License

## Schematic

![Schematic](/docs/schematic.png)

## Board

![Board](/docs/board.png) ![Board](/docs/top.jpg)

## Software Used

* PCB layout: [Eagle](https://www.autodesk.com/products/eagle/free-download)
* Compiler: [RP2040 C SDK](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) (see chapter 9)

## Contact Us

This controller was designed by [Patrick Grady](https://www.pgrady.net/), formerly with [Duke Electric Vehicles](https://www.duke-ev.org/). If you have a question that other users may also have, please open a GitHub issue. Alternatively, the best way to reach me is by email, at (first name).(last name)@outlook.com.