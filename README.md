# Coil (working name)

A spring reverb plugin (AU, VST3, Standalone) built with JUCE. Stereo, four spring voicings, low CPU
(roughly 3-5% of one core per instance in testing).

## Controls
- **Model** - Vintage (classic long tank), Keys (short, bright), Hi-Gain (long, dark, drippy, hot amp),
  Studio (bright studio-style tank).
- **Decay** - reverb time in seconds (reads close to the true T60).
- **Tension** - spring tension. Centre is the model's default; up = tighter, shorter and higher pitched,
  down = looser and longer.
- **Bright** - tone of the spring, darker to brighter.
- **Boingy** - amount of the "boing" chirp on transients.
- **Drive** - pushes the amp stage that drives the spring into saturation; the reverb gets harmonically richer.
- **Wet EQ** - two bands (gain / freq / width). At the lowest Width a band becomes a shelf
  (low shelf for the low band, high shelf for the high band).
- **Mix matrix** - Dry level, Wet level, Wet Width (0-200%) and Wet Pan.

## Get a Mac build without installing anything (GitHub Actions)
1. Create a free GitHub account and a new empty repository.
2. Upload the contents of this folder (keep the hidden `.github` folder; the build recipe finds
   `CMakeLists.txt` whether it is at the top or in a subfolder).
   Tip: if the hidden folder doesn't upload, use Add file > Create new file, name it
   `.github/workflows/build.yml`, and paste in the contents of that file.
3. Open the **Actions** tab, choose **Build Coil (macOS)**, click **Run workflow**.
4. When it finishes (about 10-15 minutes), open the run and download the **Coil-macOS** artifact.
   It is a zip inside a zip: unzip twice.

## Install on macOS
- AU:   copy `Coil.component` to `~/Library/Audio/Plug-Ins/Components/`
- VST3: copy `Coil.vst3` to `~/Library/Audio/Plug-Ins/VST3/`
- The build is unsigned. Clear the quarantine flag, then rescan in Logic's Plug-in Manager:
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Coil.component ~/Library/Audio/Plug-Ins/VST3/Coil.vst3`

## How it works
Each spring is a waveguide loop: a delay line plus a long chain of first-order allpass filters (this
gives the frequency-dependent delay that makes the falling "chirp"), damping and a low cut, fed back
on itself. Two springs per side with slightly different lengths give density and the stereo spread.
A transient detector feeds an extra strongly-dispersive branch (the Boingy control). The Drive stage
is a tube-style saturator ahead of the spring with static level compensation, so Drive changes
colour more than level.

## Files
- `Source/SpringDsp.h` - DSP core (plain C++, no JUCE)
- `Source/Plugin*.{h,cpp}` - JUCE processor and editor
- `tests/spring_test.cpp` - decay / chirp / tension / boingy / stability / CPU checks

## Notes
All four voicings are original and untuned against any commercial plugin. They are starting points
meant to be adjusted by ear. Pro Tools (AAX) is not included.
