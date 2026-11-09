#!/bin/bash
# Builds JoeysLab (VST3 + AU + standalone app) and installs the plugins for FL Studio.
set -e
cd "$(dirname "$0")"
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
OUT=build/JoeysLab_artefacts/Release
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/JoeysLab.vst3 ~/Library/Audio/Plug-Ins/Components/JoeysLab.component
ditto "$OUT/VST3/JoeysLab.vst3" ~/Library/Audio/Plug-Ins/VST3/JoeysLab.vst3
ditto "$OUT/AU/JoeysLab.component" ~/Library/Audio/Plug-Ins/Components/JoeysLab.component
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Installed. In FL Studio: Options > Manage plugins > Find installed plugins."
