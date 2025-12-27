# visu
image viewer

## Prerequisites

Ubuntu/Debian:
```bash
sudo apt install qt6-base-dev libopencv-dev libavif-dev libturbojpeg0-dev
```
Installation safe rotationsL
```bash
git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git
cd libjpeg-turbo

```

## Installation

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

## Local installation (user)

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local
make -j$(nproc)
make install
```

If the icon doesn't appear in the application menu, refresh the icon cache:
```bash
gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor
```