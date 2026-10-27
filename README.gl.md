# StudioFros – Tape Delay (GuitarFros)

Delay de cinta en formato rack (VST3 + Standalone) feito con JUCE.

## Compilar en Ubuntu

```bash
sudo apt install build-essential cmake git libasound2-dev libjack-jackd2-dev \
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev libcurl4-openssl-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

CMake descarga JUCE só. Ao rematar, o VST3 cópiase **automaticamente a `~/.vst3/`**
e o standalone queda en `build/StudioFros_artefacts/Release/Standalone/`.

En REAPER: *Options → Preferences → Plug-ins → VST → Re-scan* e busca
**StudioFros - Tape Delay (GuitarFros)**.

Se non queres a copia automática: `-DSTUDIOFROS_INSTALL_AFTER_BUILD=OFF`.

Licenza: código GPL-3.0-or-later; a imaxe, ver `ASSETS_LICENSE.md`.
