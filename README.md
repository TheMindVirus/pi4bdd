# pi4bdd
Second Monitor Support for Pi4 running Windows 10 for ARM Insider Build 21322

## WARNING!
Changing firmware and drivers may make your OS fail to boot. \
Please use with caution! (See below for Troubleshooting)

## Known Issues
 * May cause failure to boot for a variety of reasons (and may need multiple restarts)
 * Black Screen at LogonUI on Windows (which may eventually resolve itself over time)
 * Mailbox data access is temperamental and is best effort (a restart may help this)
 * A lot of the DXGK code is messy and unused, (additional effort is required to clean this up)
 * The driver currently only supports 1080p ARGB 32bpp at variable refresh rate \
   (This is due to a lot of properties being hard coded in the KMDOD sample driver)

## Monitor1
![Monitor1](https://github.com/TheMindVirus/pi4bdd/blob/main/SCREENSHOTS/Monitor1.png)
## Monitor2
![Monitor2](https://github.com/TheMindVirus/pi4bdd/blob/main/SCREENSHOTS/Monitor2.png)

## Troubleshooting

When copying the UEFI folder into an existing installation, \
Please use your existing EFI folder and take a backup of the BCD file for Windows. \
(This is located in `X:\EFI\Microsoft\Boot\BCD` and can be accessed with `bcdedit /store`)

Press ESC at boot after Rainbow Splash to Enter UEFI BIOS Settings. \
Please make sure your settings match the following:
```
[Device Manager]
--[Raspberry Pi Configuration]
----[CPU Configuration]
------[CPU Clock]
--------<Custom>
------[CPU Clock Rate (MHz)]
--------<2000>
----[Display Configuration]
------<Native resolution>
----[Advanced Configuration]
------[Limit RAM to 3 GB]
--------<Disabled>
------[System Table Selection]
--------<ACPI>
----[SD/MMC Configuration]
------[uSD/eMMC Routing]
--------<Arasan SDHCI> #IMPORTANT: Selecting <eMMC2 SDHCI> may make your OS fail to boot!
------[uSD Force Default Speed]
--------<Allow High Speed>
------[SD Default Speed (MHz)]
--------<50>
```
