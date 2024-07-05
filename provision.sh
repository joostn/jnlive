#!/bin/bash

set -e
set -x

sudo pacman --noconfirm -S python3 git nano rsync cmake base-devel curl gnome-tweaks inetutils arch-install-scripts pipewire-alsa pipewire-jack pipewire-pulse pavucontrol bluez bluez-utils wireplumber helvum libtool autoconf ardour pro-audio lsp-plugins-lv2
