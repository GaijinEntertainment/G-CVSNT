#! /bin/sh
sudo Resources/preinstall
sudo cp -rf ./Package_Root/Library/*   /Library
sudo cp -rf ./Package_Root/private/*   /private
sudo cp -rf ./Package_Root/usr/local/* /usr/local
sudo Resources/postinstall
