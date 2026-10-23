# Rooster

A privacy-focused Chromium browser with an Arc-style feel. Built on
[Helium](https://github.com/imputnet/helium)'s patches to Chromium, with its
own UI work, native ad blocking ([adblock-rust](https://github.com/brave/adblock-rust))
and video downloading on the roadmap. macOS first; Windows and Linux follow.

## How it works

Rooster doesn't fork Chromium or Helium. It pins them and applies changes on top:

```
Chromium 156 source (tarball)
  └─ Helium's patch series            pinned in config/pins.ini
  └─ helium-macos's patch series
       minus config/skip.txt          Helium parts Rooster leaves out
  └─ Rooster's patches                patches/series
  └─ Helium's substitutions           de-Googled domains, Chromium → Helium
  └─ scripts/rebrand.py               Helium → Rooster (config/branding.ini)
  └─ resources/                       Rooster's icons
```

Updating to a new Helium release means bumping `config/pins.ini` and fixing
whatever no longer applies.

### What's kept from Helium
Everything that makes it private: ungoogled-chromium's de-Googling,
fingerprinting noise (canvas, audio, hardware), Global Privacy Control, HTTPS
by default, Manifest V2 support, DNS CNAME uncloaking for extensions, and
Helium's UI and settings work.

### What's removed
Helium's hosted services, so Rooster never contacts imput's servers: the
extension download proxy, !bangs, filter lists, spellcheck dictionaries,
component and browser updates, crash uploads, the onboarding page, and the
bundled uBlock Origin (replaced by adblock-rust in milestone 2). See
`config/skip.txt` and `patches/rooster/` for the exact changes and reasons.

## Building (macOS, Apple Silicon)

Requirements: Xcode 26, Homebrew, Python 3 with Pillow, and **about 150 GB of
free disk**. On a 256 GB Mac, put the build on an external SSD.

```sh
brew install python@3.13 wget zstd coreutils sccache
pip3 install pillow requests
xcodebuild -downloadComponent MetalToolchain

export ROOSTER_BUILD_DIR=/Volumes/YourSSD/rooster-build   # optional
scripts/rooster doctor     # checks tools, disk, memory
scripts/rooster setup      # download (~4 GB), prepare (~5 min), configure
scripts/rooster build      # first build: overnight on a 16 GB M4; then minutes
scripts/rooster run        # runs with its own profile in $ROOSTER_BUILD_DIR
```

Before the first run, set `bundle_id` in `config/branding.ini` to a reverse-DNS
name for a domain you own. It names the profile folder, so changing it later
orphans existing profiles.

Dev builds are configured for 16 GB of RAM (`config/flags.dev.gn`: component
build, no LTO, no symbols, 6 parallel jobs via `ROOSTER_JOBS`). Release builds
(`scripts/rooster configure release`) use Helium's official LTO + PGO setup and
need a bigger machine or CI runner.

After changing anything under `patches/`, `config/skip.txt` or the branding,
start over from clean source: `scripts/rooster reset && scripts/rooster download && scripts/rooster prepare`.
Downloads are cached, so that only re-extracts.

## Layout

| Path | What it is |
| --- | --- |
| `config/pins.ini` | Chromium, Helium and helium-macos versions (by commit) |
| `config/branding.ini` | Product name, internal scheme (`rooster://`), bundle ID |
| `config/skip.txt` | Upstream patches Rooster leaves out, each with a reason |
| `config/flags*.gn` | GN build arguments, over Helium's |
| `patches/` | Rooster's own patches to the Helium-patched tree |
| `scripts/rooster` | The build tool |
| `scripts/merge_patches.py` | Combines the three patch series minus skips |
| `scripts/rebrand.py` | Helium → Rooster in branding, source and strings |
| `resources/make_icons.py` | Generates every logo asset from one definition |

## Status

**Milestone 1 (this): a buildable, rebranded, de-serviced Helium.**
All patches apply to Chromium 156.0.8078.47 and `gn gen` resolves the whole
build graph, but it hasn't been compiled yet; expect a round of compile fixes
on the first Mac build.

Known gaps, by design for now:
- **No extension installs from the Chrome Web Store.** Helium routes them
  through its proxy, which is off. Next decision: direct to Google, or a
  Rooster proxy.
- **No ad blocking yet.** The bundled uBlock Origin is gone and adblock-rust
  arrives in milestone 2.
- **No component updates**, so no Widevine (Netflix etc.) and no certificate
  revocation list updates. Same decision as the Web Store.
- No auto-update (Sparkle is skipped) and no spellcheck dictionary downloads.
- The Helium services settings page is still there, with switches that do
  nothing.

Roadmap: adblock-rust (2), Arc-style sidebar in Views (3), video downloader
with yt-dlp (4), polish (5), signed releases (6), Windows and Linux (7).

## License

GPL-3.0, like Helium, whose patches Rooster ships. Every release must link its
source. Imported code keeps its own license (ungoogled-chromium: BSD-3-Clause;
Chromium: BSD-3-Clause and others).

Helium doesn't accept AI-written contributions, so don't send changes made in
this repo back to Helium's repositories.
