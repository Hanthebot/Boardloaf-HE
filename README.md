# Boardloaf-HE

Hall-effect variant of [Yuburoll's Boardloaf](https://github.com/yuburoll/Boardloaf) split keyboard.
Wooting compatible, reversible, under 100mm pcb with qmk/vial support.

Strongly inspired by Boardloaf and [Truestrike42](https://github.com/byungyoonc/TrueStrike42)

## Preparation
- board
    - pcb x 2
    - sot23 SS49E x 36
    - soic16 74HC4051 x 6
    - rp2040 zero x 2
    - pj320a x 2
    - 0805 0.1uf capacitor x 42 (~1uf should be fine)
- case
  - 3d printed (from yuburoll's repo, pcb outline and holes are not modified, so should be fine)
  - cheap plate cut
    - m2 3mm heat insert x 16
    - m2 7mm standoff x 16 (can be shorter or longer a bit)
  - m2 screw x 16, m2x6 worked for me

## Features
- Supports both APC and rapid trigger

Documentation in progress.

## Improvements to be done
- [ ] switch to blackpill mcu for better ADC (not in this repo)
- [ ] switch to full duplex / TRRS communication
- [ ] make sot 23 pad larger, thus easier to solder

## Licenses
MIT license for all the code.
CC BY-SA 4.0 license for all hardware / design, from original Boardloaf.

## Disclaimer
Webapp and firmware were vibe coded. I checked it working, but need to double check.
