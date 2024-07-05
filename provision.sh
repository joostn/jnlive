#!/bin/bash

set -e
set -x

# get dir containing this script:
SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

sudo pacman --noconfirm -S python3 git nano rsync cmake base-devel curl gnome-tweaks inetutils arch-install-scripts pipewire-alsa pipewire-jack pipewire-pulse pavucontrol bluez bluez-utils wireplumber helvum libtool autoconf ardour pro-audio lsp-plugins-lv2 ninja

if [[ ! -f /usr/bin/yay ]]; then
    cd /tmp
    git clone https://aur.archlinux.org/yay.git
    cd yay
    makepkg -si --noconfirm
    if [[ ! -f /usr/bin/yay ]]; then
        echo "yay not installed"
        exit 1
    fi
fi;

yay -S --noconfirm  visual-studio-code-bin

mkdir -p ~/.config/environment.d/
echo 'SSH_AUTH_SOCK=$XDG_RUNTIME_DIR/gcr/ssh' > ~/.config/environment.d/joost_sshagent.conf
systemctl --user enable --now gcr-ssh-agent.socket

sudo echo "deny = 30" > /etc/security/faillock.conf
sudo echo "Defaults timestamp_timeout=1440" > /etc/sudoers.d/timeout

if [[ ! -d '/usr/local/lib/lv2/Surge XT.lv2' ]]; then
    cd /tmp
    git clone https://github.com/surge-synthesizer/surge.git
    cd surge
    git submodule update --init --recursive
    cmake -Bbuild -DSURGE_BUILD_LV2=TRUE -DSURGE_SKIP_STANDALONE=ON -DSURGE_SKIP_ALSA=ON -DSURGE_BUILD_CLAP=OFF -DSURGE_SKIP_VST3=ON
    cmake --build build --parallel --config Release --target surge-staged-assets
    sudo cmake --install build
    if [[ ! -d '/usr/local/lib/lv2/Surge XT.lv2' ]]; then
        echo "Surge XT not installed"
        exit 1
    fi
fi

if [[ ! -d '/usr/lib/lv2/Fluida.lv2/']]; then
    cd tmp
    git clone https://github.com/brummer10/Fluida.lv2.git
    cd Fluida.lv2
    git submodule init
    git submodule update
    make -j8
    sudo make install
    if [[ ! -d '/usr/lib/lv2/Fluida.lv2/']]; then
        echo "Fluida.lv2 not installed"
        exit 1
    fi
fi

cd $SCRIPTDIR/app
cmake -Bbuild -GNinja
cmake --build build --parallel --config RelWithDebInfo

