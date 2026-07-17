# EERIE - Mix Analyzer

A free audio analysis tool for finding where a guitar (or any part) fits in a
mastered mix. Ships as a standalone app and a VST3 plugin, built with JUCE.

It only measures and exports - it never changes the sound. You read the meters
live and export the captured data to a file.

## Planned meters
- Loudness (EBU R128 / BS.1770)
- Spectrum (live FFT)
- Sound Field (stereo vectorscope / correlation)
- Plot Spectrum (averaged spectrum snapshot, exported as a frequency-vs-dB table)

Plus a record button that logs the selected data to a file and exports it.

## Building (Windows)

```
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release
```

The first configure downloads JUCE automatically (pinned to 8.0.14).

## Requirements
- CMake 3.22+
- Visual Studio 2022 (or Build Tools 2022) with the C++ workload
