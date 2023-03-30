# Building an EasyController3

This document is meant to provide a guide on how to assemble the EasyController3. While some electronics experience is required, hopefully this lowers the barrier to entry.

## What you need

* $100-200 USD to buy parts and boards
* A computer to install the Raspberry Pi Pico SDK on
* A soldering iron and associated tools
* A benchtop power supply capable of ~5 amps for testing
* A BLDC motor 
* A throttle
* High-current wire and connectors for power and motor phase connections
* Low-current wire and connectors for hall sensors and throttle connections

## PCB Manufacturing

This respository includes [Gerber files](/board/gerbers.zip) which can be directly submitted to PCB manufacturers. We recommend Chinese manufacturers such as [JLCPCB](https://jlcpcb.com/), which typically ship worldwide and deliver in one week for $30 USD.

## Buying Components
The repository includes a [Bill of Materials (BOM)](/board/BOM.xlsx) with Digikey part numbers to order from. Digikey ships quickly and internationally. The parts may also be available from other vendors.

Many parts may be substituted, such as resistors, capacitors, and transistors. However, most of the ICs cannot be substituted without changing the design.

## Assembling the Hardware

TODO

## Installing the SDK and Configuring the Firmware

TODO