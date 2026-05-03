# optional webserver for use with a wifi-enabled antenna-switch
# run this script with "sudo ./install.sh"
# -> edit .config, frq-map and .ports for your Antenna Switch, frequencies and portlabels
# -> edit antswitch.php in /var/www/html for the location of this files (for example in /usr/local/antswitch-cli/)
# -> edit antswitch-config.php in /var/www/html/login for admin-password (default:"antswitch")
sudo apt install curl jq php apache2
# if you want to use the apache php-webserver:
sudo mv ./antswitch.php /var/www/html/
sudo mkdir /var/www/html/login
# maybe you want to configure user authorization with libapache2-mod-authnz-pam for var/www/html/login/? (not included in this script)
sudo mv ./antswitch-config.php /var/www/html/login/
# important if you want to edit configuration of port with php-webserver:
sudo chown -R root:www-data ./
sudo chmod 755 antswitch-cli.sh
sudo chmod 664 antswitch-cli.ports
sudo chmod 664 antswitch-clo.frq-map

