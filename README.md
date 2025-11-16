# visu
image viewer

Ubuntu/ Debian
```bash
sudo apt install libavif-dev
```
Installation safe rotationsL
```bash
git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git
cd libjpeg-turbo

```

```bash
mkdir build
cd build
cmake -G"Unix Makefiles" -DCMAKE_INSTALL_PREFIX=/usr/local ..
make -j$(nproc)
```

```bash
sudo make install
```