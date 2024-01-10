plugins:

https://aur.archlinux.org/packages/sfizz-lv2-git

git clone https://github.com/sfztools/sfizz-ui.git --recursive
cd sfizz-ui
mkdir build
cd build
cmake .. -DSFIZZ_JACK=OFF -DSFIZZ_SHARED=OFF -DSFIZZ_RENDER=OFF -DPLUGIN_LV2_UI=ON -DPLUGIN_VST3=OFF
cmake --build .
cmake --install .
