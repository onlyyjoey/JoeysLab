# JoeysLab

A chord, melody and stem generator with a built-in sampler, made for building beats fast and dragging the results into FL Studio (or any DAW).

**Open the app:** https://onlyyjoey.github.io/JoeysLab/ (after GitHub Pages is switched on, see below). On a phone, use *Add to Home Screen*; on a computer, use the install icon in Chrome's address bar. It works offline after the first visit.

## What it does

- **Generate** full ideas in five styles: Rage (Yeat), Psych (Don Toliver), Soul (Kanye West), Space (Kid Cudi) and Cloud (Clams Casino). Pick the key, length (4 or 8 bars) and tempo, and lock the tempo if you want.
- **Stems:** each stem can be re-rolled on its own, by section, or note by note (new pitches, new rhythm, vary, brand new, counter melody). Synth presets, drum kits built from your own samples, 808s and basses, vocal chops and bells.
- **Sampler:** drop in a song, highlight a part and cut it down, then flip it (Straight, Clams flip, Chop & stutter, Rocky chop). Keep, reverse, pitch, blend, link or replace chops, add more parts of the song, and resample your loop to chop on top of it. Bass and 808 follow the sample's own bass notes.
- **Export:** pick any stems (and split drums into kick, snare, hats…) as MIDI, WAV or MP3. A single file downloads on its own; several come in one zip, all starting on bar 1.
- **Saves locally:** your session, stems and sample audio are kept on your device automatically and come back when you reopen the app. *Settings → Saved on this device → Clear* wipes it.

## Files

| Path | What it is |
| --- | --- |
| `index.html` | The whole app (HTML, CSS and JavaScript in one file) |
| `samples/` | Drum kit, 808s, basses, Mike Dean sine and vocal chop samples (WAV) |
| `sw.js` | Offline cache. Bump `VERSION` when you change files so installed copies update |
| `manifest.webmanifest`, `icon*` | Lets the app install to a home screen or desktop |

## Turning on the website (one time)

1. On GitHub, open this repo → **Settings** → **Pages**.
2. Under **Build and deployment**, set **Source** to *Deploy from a branch*, **Branch** to `main` and folder `/ (root)`, then **Save**.
3. After a minute the app is live at `https://onlyyjoey.github.io/JoeysLab/`.

## Running it on your own computer

Browsers block loading the samples from a plain file, so serve the folder:

```
python3 -m http.server 8000
```

then open http://localhost:8000.

## Notes

- Mic recording needs HTTPS (GitHub Pages) or localhost.
- MP3 export loads the free LAME encoder (lamejs, LGPL) from jsDelivr the first time it's used; if that fails you get WAV.
- Style names refer to artists only as a description of a sound. Samples in `samples/` are the owner's own kit and pack content.
