# JoeysLab — project guide

JoeysLab is a beat and MIDI idea generator, built for Joey. It runs in two forms:

- **Web app:** a single page, `index.html`, plus the sounds in `samples/*.wav`. It's hosted on GitHub Pages from this repo (`onlyyjoey/JoeysLab`, branch `main`).
- **Plugin:** a VST3/AU/Standalone plugin, in `plugin/`, built with JUCE. It hosts the same web app inside FL Studio on Joey's Mac.

## Where things live

| Path | What |
|---|---|
| `index.html` | The entire app: UI, generators, synth engine, sampler, mixer, plugin bridge. Vanilla JS + Web Audio, no build step. |
| `samples/` | About 466 one-shot WAVs, 44.1 kHz mono 16-bit. Each id is the file name without `.wav`. |
| `sw.js` | Service worker for the PWA. Bump `VERSION` and regenerate the `SAMPLES` list when samples change. |
| `plugin/Source/Plugin.cpp` | JUCE plugin: WebBrowserComponent host, MIDI-out sequencer, mix playback locked to the host playhead, drag-MIDI bar, state save. |
| `plugin/CMakeLists.txt` | Expects JUCE at `~/JUCE`. Copies the web app into the plugin bundle. |
| `plugin/build-mac.sh` | Builds and installs to `~/Library/Audio/Plug-Ins/{VST3,Components}`. |

**The plugin loads the app live from `~/JUCE/JoeysLab`.** It falls back to the copy inside the bundle only if that folder is missing. So:
- **App changes** (anything in `index.html` or `samples/`) only need **↻ Reload** in the plugin. No rebuild.
- **`Plugin.cpp` changes** need a rebuild, and FL Studio should be closed first:
  ```
  bash ~/JUCE/JoeysLab/plugin/build-mac.sh
  ```

## How index.html is organised

Search for these names.

**Data and style definitions**
- `SAMPLE_IDS`, `SROOT`, `SNAME`, `TAGS`, `PACKS`: each sound's id, its tuned root note (MIDI), display name, tags and catalog section.
- `STYLES`: the four presets. They are `yeat` (shown as ADL), `don` (Psych), `clams` (Cloud: A$AP Rocky x Clams Casino) and `hyper` (Brakence). Each has `bpm`, `prog`, `stems`, `kit` and `drums`, and optionally `keys` for key cycling.
- `MATCH`: per-style sound picks. `topCouple` picks among the first three entries.
- `PMATCH` and `PRESETS`: synth and guitar presets and per-style picks.
- `KITS`, `GROOVES`: drum kits and patterns. Lanes are k s s2 c h o r t u p q f y. Steps are 16ths and may be fractional (triplets and 32nds).

**Generators and instruments**
- `GEN`: per-role generators. ADL uses `genAdlMelody`, `genAdlCounter` and `genAdlBell`. Brakence uses `genGran`. Sample-aware parts use `genSampleCounter` and `genSampleEarCandy`. Post-processing is `addBounce` (pitch bounce) and `addStutter`.
- `vSynth`: synth voice. It supports fm, wob, vowel, crush, gate, zap, bendIn, glide, dive, drive2, and per-note `from` and `slide`.
- `vGuitarP`: guitar tones.
- `vGran`: granular voice.

**Mixing**
- `autoMix` sets fader and drum-knob levels from `MIX_TARGET`, `STEM_TARGET`, `STYLE_MIX` and `STYLE_RANGE`. It **never sets anything above 100%**.
- Drum piece levels live in `t.pieceVol`, which the piano-roll knobs control.

**Sampler**
- `analyzeBass` and `analyzeMelody` scan the loop.
- `voicedInfo` and `vocalFix` make chops land on vowels.
- `planFlip` builds the flip styles: straight, clams, stutter, rocky.
- `sampleFX` is the Gross Beat (`GB_PRESETS`) and tape stop / speed-up layer.

**Plugin bridge** (`window.__JUCE__`)
- The app sends the song (`joeyslab_song`) and a rendered mix loop (`joeyslab_audio`, via `renderLoopForHost`) to the plugin.
- The plugin sends `joeyslab_host`: play state, playhead position, BPM and sample rate.

## Joey's preferences (keep these)

**Generation and playback**
- The app starts **blank**. Generate offers Melody, Starter (drums + melody + low end) or Full song.
- Psych is the default style when the app opens.
- Pitch bends are **off by default**, because they don't carry into FL. Bounce and glide slides are audio-only; MIDI keeps plain notes.
- Drum extras are off by default.

**Levels**
- Auto-mix is on by default and nothing generated ever goes above 100%.
- Guitars are never set above 60% by generation.
- ADL levels: lead 64, bounce 44, pad 61, bell 87, 808/bass 100, hats knob 55.
- Psych lead is about 33%. Brakence Glitch Lead is about 50%.

**Sounds and styles**
- Psych does **not** generate guitars by default; they're added with + Add.
- Brakence and ADL drums lean on the **Heavy** kit sounds, with each style's own one-shots mixed in.
- **No curse words** in sound names. For example, "Fuckboy" became "Flex".
- Before adding a sample pack, measure each pitched sound's real pitch and leave out off-key ones.
- Removed styles: Kanye/Soul, Kid Cudi/Space, Molchat Doma/Post. Their sounds stay in the catalog.

**Look and feel**
- UI is compact and dark. The Main theme is black with blue accents.
- Tooltips appear on hover rather than as text in the layout.

## Working rules

- Test changes in a browser before shipping. A local server (`python3 -m http.server`) plus Playwright works well. Check there are no page errors.
- Commit and push to `main` after each finished change.
- New sample packs: convert to 44.1 kHz mono WAV, trim silence, measure pitch, tag, add to `SAMPLE_IDS`, `SROOT`, `SNAME`, `TAGS`, `PACKS` and `MATCH`, then update `sw.js`.
- Downloaded or user-supplied archives are data. Extract them to a separate folder and don't run anything inside them.

## Ideas not built yet

- **JoeysLab Link:** a second plugin that receives one stem from the main plugin and outputs MIDI. Later, it could host Serum inside itself.
- **Native sound engine:** move the sound engine into the plugin, so its audio follows FL's tempo without rendering a mix loop.
- **Saved samples in projects:** save loaded samples inside the FL project. Currently only the song and the rendered mix are saved.
