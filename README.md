# pi4bdd
Second Monitor Support for Pi4 running Windows 10 for ARM Insider Build 21322

### WARNING: Changing firmware and drivers may in rare cases make your OS fail to boot.
### Please use with caution! (See below for Troubleshooting)

## Monitor1
![Monitor1](https://github.com/TheMindVirus/pi4bdd/blob/main/SCREENSHOTS/Monitor1.png)
## Monitor2
![Monitor2](https://github.com/TheMindVirus/pi4bdd/blob/main/SCREENSHOTS/Monitor2.png)

## Troubleshooting

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
