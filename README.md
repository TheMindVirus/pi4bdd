# pi4bdd
Second Monitor Support for Pi4 running Windows 10 for ARM Insider Build 21377 \
Build output available at: https://github.com/TheMindVirus/pi4bdd/tree/main/KMDOD/ARM64/Release/SampleDisplay

## WARNING!
Changing firmware and drivers may make your OS fail to boot. \
Please use with caution!

## Known Issues
 * May cause failure to boot for a variety of reasons (and may need multiple restarts)
 * Black Screen at LogonUI on Windows (which may eventually resolve itself over time)
 * Mailbox data access is temperamental and is best effort (a restart may help this)
 * A lot of the DXGK code is messy and unused (additional effort is required to clean this up)
 * The DXGK Display Only Driver source is not capable of rendering (a separate initialisation is required)
 * The driver currently only supports 1080p ARGB 32bpp on HDMI1 at variable refresh rate \
   (This is due to a lot of properties being hard coded in the KMDOD sample driver)
 * On some monitors you may experience visual glitches (due to formatting and timing errors)

## Monitor2
![Monitor2](https://github.com/TheMindVirus/pi4bdd/blob/pi4bdd-resurrected/Monitor2.png)
