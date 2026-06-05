# ELE3DEC Prototype Multimeter

Prototype multimeter project for **ELE3DEC Embedded Design Project 2026** using the **MSP430G2553**. The device measures voltage, resistance, and continuity, displays results on a 2-line LCD, and uses a speaker for continuity and battery-test feedback. The project brief requires a prototype multimeter with these three modes, PB1/PB2 mode switching, LCD output, speaker behavior, and submission materials including a circuit diagram, commented C code, flow diagram, photos, team contribution breakdown, and a demo video link. [file:1]

## Features

- Voltage measurement from 0V to 3.3V, shown in V or mV. [file:1]
- Battery Test mode with `BattGood` / `BattBad` status and distinct sound effects above or below 1.50V. [file:1]
- Resistance measurement displayed in ohms or kilo-ohms. [file:1]
- Continuity mode showing `Short` / `No Short` with an audible beep when shorted. [file:1]
- PB1 changes the main mode; PB2 changes sub-mode and does nothing in continuity mode. [file:1]

## Repository Structure

```text
src/                Main C source files
schematics/         Circuit diagram exports or source files
flowchart/          Software block diagram / flowchart
media/photos/       Build and LCD demonstration photos
video/              Demo video link or notes
docs/               Submission notes and contribution breakdown
```

## Hardware Mapping

Current code assumes the following mapping:

- P1.0 (A0): measurement input probe
- P1.1: PB1 main mode button
- P1.2: PB2 sub-mode button
- P1.5: speaker
- P1.6: LCD Enable (E)
- P1.7: LCD Register Select (RS)
- P2.0-P2.7: LCD data bus

Verify this against your final schematic before submission, because the project requires a correct circuit diagram and matching implementation. [file:1]

## Build Notes

This project is intended for the MSP430G2553 and can be built in Code Composer Studio or IAR using a standard MSP430 C project setup. The assignment requires commented C code and a clear logical structure for stronger rubric performance. [file:1]

## Submission Checklist

The project brief says the final submission PDF should include:

- Circuit diagram. [file:1]
- Commented C code. [file:1]
- Software flow diagram. [file:1]
- Photos of the completed device and LCD outputs. [file:14]
- Team contribution breakdown. [file:1]
- Video demonstration link. [file:1]

## Suggested GitHub Upload Steps

1. Put your final `.c` file into `src/`.
2. Put your circuit diagram into `schematics/`.
3. Put your flowchart into `flowchart/`.
4. Put photos into `media/photos/`.
5. Add your video URL into `video/demo-link.txt`.
6. Add the contribution breakdown into `docs/contributions.md`.
7. Push the folder as a GitHub repository.

## Files To Add Next

- `src/main.c`
- `schematics/circuit-diagram.png` or `.pdf`
- `flowchart/software-flowchart.png` or `.pdf`
- `media/photos/` images
- `video/demo-link.txt`
- `docs/contributions.md`

