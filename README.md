# Esparducam
#### A low cost network camera for the ESP8266

This repository contains code and schematics to build a development board joining the ESP8266 with the [Arducam Mini](http://www.arducam.com/arducam-mini-released/) module. Build surveillance cameras, read your water meter, attach one to your kite.

<p align="center">
  <img src="https://raw.githubusercontent.com/kanflo/esparducam/master/esparducam.jpg" alt="Esparducam board"/>
</p>

The board design allows for small breakout boards to be attached. There currently is [a breakout board the the RFM69C](https://github.com/kanflo/esparducam/tree/master/hardware/ism-boardlet), [an ESP12 board with pogo pins](https://github.com/kanflo/esparducam/tree/master/hardware/esp-pinlet) and [one for swappable ESP12 modules](https://github.com/kanflo/esparducam/tree/master/hardware/esp-boardlet).

Oh, the accompanying blog posts are [here](http://johan.kanflo.com/building-a-low-cost-wifi-camera/) and [here](http://johan.kanflo.com/a-versatile-esp8266-development-board/).

###Usage

Clone the ESP Open RTOS repository. See the documentation for how to add your SSID name and password.

```
git clone --recursive https://github.com/Superhouse/esp-open-rtos.git
```

Add nedded extra modules to ESP Open RTOS ("EOR"):

```
cd esp-open-rtos/extras
git clone https://github.com/kanflo/eor-spi.git spi
git clone https://github.com/kanflo/eor-spiflash.git spiflash
git clone https://github.com/kanflo/eor-http-upload.git http-upload
```

Set the environment variable ```$EOR_ROOT```to point to your EOR repository:

```
export EOR_ROOT=/path/to/esp-open-rtos
```

Clone this repository, make and flash:

```
git clone https://github.com/kanflo/esparducam.git
cd esparducam
make -j8 && make flash
```

When flashing has completed, open a serial terminal to learn the IP number of your board. Open a web brower, point it to your IP and an image should be captured and displayed. While in the serial terminal, type ```help``` for a list of supported commands.

Start the python script ```server.py``` and type ```upload:<your IP>``` to capture, upload and display an image on your computer.

If you connect a [PIR module](http://www.ebay.com/sch/i.html?_trksid=PIR+module.TRS0&_nkw=PIR+module&_sacat=0) [eBay] to the JST connector you can use the command ```motion:on:<your ip>``` to have the board capture and upload an image when motion is detected.

###Limitations

This is work in progress and there is currently very little error handling. Simultaneous clients will break the camera demo.

-
Licensed under the MIT license. Have fun!