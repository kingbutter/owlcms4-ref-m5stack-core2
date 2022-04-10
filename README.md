# owlcms4-ref-m5stack

This is a update to code from [https://github.com/jflamy/owlcms-esp32](https://github.com/jflamy/owlcms-esp32)  to work with the M5Stack-Core2


## Editing Logos
Set up logos according to the table: 
|Size|Name                         |
|-------------------------------|-----------------------------|
|20x20|logo_20.png|
|40x40|logo_40.png|
|120x120|logo_120.png|
|240x240|logo_240.png|

Convert each file in to c code at 
http://www.rinkydinkelectronics.com/t_imageconverter565.php

Edit logo_owlcms.h with the logo definitions created and build


## Initial Setup
On first boot an Access Point will be created, with its name displayed on the screen. Connect to the access point and you should be forwarded to the setup screen.

|Field|Description                         |
|-------------------------------|-----------------------------|
|SSID|SSID To Connect to|
|WiFi Password|SSID Password|
|MQTT Server|MQTT Server To Connect to|
|MQTT Port|MQTT Server Port To Connect to|
|MQTT Use TLS| use mqtts://|
|MQTT User|MQTT Username|
|MQTT Password|MQTT Password|
|Platform| Platform (FOP)|
|Referee| Which Referee will use this unit|
|Enable Sound| Enable sounds on notification|

## How to use

Press the Red Square or the left button for a No Lift
Press the White Square or  the right button for a Good Lift
Hold the Middle button for 5 seconds and release to trigger a config reset
