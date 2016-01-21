# Esparducam
#### A low cost network camera for the ESP8266

This repository contains code and schematics to build a development board joining the ESP8266 with the [Arducam Mini](http://www.arducam.com/arducam-mini-released/) module. Build surveillance cameras, read your water meter, attach one to your kite.

The board design allows for small breakout boards to be attached. There currently is [a breakout board the the RFM69C](https://github.com/kanflo/esparducam/hardware/ism-boardlet), [an ESP12 board with pogo pins](https://github.com/kanflo/esparducam/hardware/esp-pinlet) and [one for swappable ESP12 modules](https://github.com/kanflo/esparducam/hardware/esp-boardlet).


###Usage

Clone the ESP Open RTOS repository. See the documentation for how to add your SSID name and password.

```
git clone --recursive https://github.com/Superhouse/esp-open-rtos.git
```

Add nedded extra modules to ESP Open RTOS ("EOR"):

```
cd esp-open-rtos/extras
git clone https://github.com/kanflo/eor-spi.git spi
git clone https://github.com/kanflo/eor-spi.spiflash spiflash
git clone https://github.com/kanflo/eor-past.git past
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

###Limitations

This is work in progress and there is currently very little error handling. Simultaneous clients will break the camera demo.

-
Licensed under the MIT license. Have fun!