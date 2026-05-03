The antenna switch can be controlled via a browser interface or a serial interface. No special software is required for either method.  
However, it should be noted that a serial connection is not necessarily recommended if, for example, common-mode currents are expected at the switch due to an unsuitable antenna. Generally, a connection via the station's Wi-Fi network is recommended.  
There are some use cases where you might want to control a Wi-Fi-connected antenna switch using the Linux shell.  
This is now possible with Antswitch-CLI.  
  
antswitch-cli.config:  
Configuration of addresses and commands (admin users are only relevant for the web interface, see below)  
  
antswitch-cli.frq-map:  
Overview of frequencies, antennas, and tuners to be used.  
Syntax: [Start Frequency (Hz)]-[End Frequency (Hz)]=[Antenna Number]|[TUNER (EXTERNAL/INTERNAL/OFF)]  
  
antswitch-cli.port:  
Labels and Default Ports  
(The script can read this information from the hardware and write it to the file)  
  
install.sh:  
Please read this beforehand; it contains information on installing the additional web interface.  
  
antswitch.php and antswitch-admin.php:  
Web interface for antenna switching via Antswitch-Cli.  
  
Although the antenna switch has its own web interface, there are reasons for not making it externally accessible. Furthermore, the antswitch-cli web interface is easier to embed as an iframe.  
  
The frequency map file can be edited and labels from the antenna switch can be imported via antswitch-admin.php. User management is not explained further here. However, it is advisable to install a PAM module for Apache, which, for example, manages access to the "/var/www/html/login" subfolder.  
