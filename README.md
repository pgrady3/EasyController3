# EasyController3

*Update May 12, 2023 - During high power testing above 15 amps, it was discovered that the gate drivers are unreliable. A fix has been developed, and version 3.1 will be released soon. We were in a bit of a rush to release and didn't test as thoroughly as we should have. We do NOT RECOMMEND building this version at this time, and please check back for the update.*



![Assembled Controller](/docs/side.jpg)

The EasyController3 is the next evolution of the successful [EasyController2](https://github.com/pgrady3/EasyController2). The new version is cheaper, more efficient, and uses well-stocked components.

## Improvements over the EasyController2

* Reduced BOM cost from $70 to $36
* Uses more efficient synchronous PWM. 5-10% greater efficiency under partial throttle operation
* Improved from duty-cycle control to torque-control, improving controllability and reducing current spikes
* Added regenerative braking
* Lowered quiescent power from 960 mW to 280 mW in a typical installation
* Based on Raspberry Pi Pico microcontroller
* Uses more widely available surface-mount components

## Getting Started

For a comprehensive guide on how to build your own EasyController, please see the [getting started guide](/docs/getting-started.md).

## Background

The EasyController3 is a a simple brushless (BLDC) sensored motor controller (also known as an ESC, motor inverter, or motor drive). It is intended to power vehicles such as electric bikes, skateboards, or [Eco-Marathon vehicles](https://en.wikipedia.org/wiki/Shell_Eco-marathon) in the 50-1000 watt range. It is designed for hobbyists to assemble at home.

The EasyController family was built to to fill a gap in publicly released motor controller designs. It is designed to be simple yet still highly functional. It deliberately omits more complex features such as sensorless or field-oriented control, however these features can be added with modification. This project is meant as a learning tool and a foundation to potentially build more complex designs from.

## Specifications
* 8v-60v operation
* 20A continuous current, 40A burst (more with heatsinking)
* Large surface mount components designed to be hand-soldered by hobbyists
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

## Other Related Controller Designs

There are several other open-source motor controller designs worth mentioning:
* The [VESC](https://vesc-project.com/) is a highly capable design, and is well suited for vehicle applications. However, due to the complexity and poorly written code, it is difficult to modify.
* The [Simple FOC](https://simplefoc.com/) project is an open-source library for FOC commutation. However, due to the hardware-agnostic code, it is low-performance and suited for gimbal motors, not vehicle applications.
* The [ODrive](https://odriverobotics.com/) is a modular controller built for robotics applications. However, the hardware is closed-source.

The EasyController3 is most suitable for people who want to design, build, and understand their own motor controller. If you'd just like to purchase something off-the-shelf, we recommend the VESC, which is around $100 USD.

## Contact Us

This controller was designed by [Patrick Grady](https://www.pgrady.net/), formerly with [Duke Electric Vehicles](https://www.duke-ev.org/). If you have a question that other users may also have, please open a GitHub issue. Alternatively, the best way to reach me is by email, at (first name).(last name)@outlook.com.