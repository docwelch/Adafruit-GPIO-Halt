Adafruit-GPIO-Halt
==================

Press-to-halt program for headless Raspberry Pi. Similar functionality to the rpi_power_switch kernel module from the fbtft project, but easier to compile (no kernel headers needed).

Modifications
==================
This has been modified to work with the Odroid C2 running ArchLinuxARM
The Odroid has several pins that are already configured with pullups so we will use one of those (see [Odroid GPIO Defaults] (http://odroid.com/dokuwiki/doku.php?id=en:c2_gpio_default)). The current default is Pin 29 (GPIO 228). Connecting pin 29 to ground (pin 30) briefly will cause the Odroid to shut down.

To use this:

    git clone git://github.com/docwelch/Adafruit-GPIO-Halt
    cd Adafruit-GPIO-Halt
    make
    sudo make install

To create a service: 

    sudo nano /etc/systemd/system/halt
And place the following in the file:

    [Unit]
    Description=GPIO_Halt service for Odroid C2
    
    [Service]
    ExecStart=/usr/local/bin/gpio-halt
    Type=simple
    
    [Install]
    WantedBy=multi-user.target

To start the service:

    sudo systemctl start halt
    
To make the service run at boot:

    sudo systemctl enable halt
