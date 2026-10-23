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
  └─ Rooster's patches                patches/series (small hooks into Chromium)
  └─ src/                             Rooster's own code (//rooster) and the
                                      vendored crates for adblock-rust
  └─ Helium's substitutions           de-Googled domains (minus
                                      config/domsub-keep.txt), Chromium → Helium
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
bundled uBlock Origin (replaced by adblock-rust, below). See
`config/skip.txt` and `patches/rooster/` for the exact changes and reasons.

### Ad blocking
Built in, using Brave's [adblock-rust](https://github.com/brave/adblock-rust)
engine (0.13.3) instead of an extension:

- `src/rooster/components/adblock/core`: a small cxx bridge to the crate and a
  thread-safe C++ `Engine`. Third-party detection uses Chromium's public
  suffix list.
- `src/rooster/components/adblock/browser`: `AdblockService` downloads the
  filter lists daily straight from their publishers (EasyList, EasyPrivacy,
  uBlock Origin's filters/privacy/unbreak lists, Peter Lowe's list), caches
  them and a compiled engine in `<profile dir>/Adblock`, and swaps in new
  engines without blocking the UI. `AdblockURLLoaderFactory` sits in front
  of every page's requests (and subframe navigations) and cancels matched
  ones with `ERR_BLOCKED_BY_CLIENT`, or strips tracking parameters
  (`removeparam`). Top-level pages, extensions and browser pages are left alone.
- `patches/rooster/adblock-hooks.patch`: the only places Chromium calls in.
- `patches/rooster/adblock-rust-crates.patch` and `src/third_party/rust`: the
  crate and its dependencies, vendored with Chromium's `gnrt` tool.

Not done yet: cosmetic filtering (element hiding and scriptlets, which most
cookie-banner and YouTube rules need), `redirect` rules (matched requests are
cancelled instead), per-site toggles and a settings UI.

Two Chromium crate policies are overridden for this, both noted in
`gnrt_config.toml`: `regex` (limited to tests in Chromium) ships, because
adblock-rust needs it, and `aho-corasick` may use `unsafe` code.

## Building (macOS, Apple Silicon)

Requirements: Xcode 26 or newer, Homebrew, uv with Python 3.13, and **at least
60 GB of free disk** for a dev build (about 45 GB used: ~16 GB of source,
~3.5 GB of cached downloads, 20–30 GB of build output with no debug symbols).
Keep `symbol_level = 0` in `config/flags.dev.gn` on a small disk; symbols add
tens of GB. `rooster build` caps sccache at 5 GB (`SCCACHE_CACHE_SIZE`).

Chromium 156 targets the macOS 26 SDK. Newer macOS/Xcode versions usually work,
but can surface new deprecation warnings (not errors here) or SDK breakage that
needs a patch.

```sh
brew install uv wget zstd coreutils sccache
uv python install 3.13     # rooster finds it with uv and uses it as python3
xcodebuild -downloadComponent MetalToolchain

export ROOSTER_BUILD_DIR=/Volumes/YourSSD/rooster-build   # optional
scripts/rooster doctor     # checks tools, disk, memory
scripts/rooster setup      # download (~4 GB), prepare (~5 min), configure
scripts/rooster build      # first build: overnight on a 16 GB M4; then minutes
scripts/rooster run        # runs with its own profile in $ROOSTER_BUILD_DIR
```

The bundle ID is `dev.dunkbing.rooster` (`config/branding.ini`). It names the
profile folder, so changing it later orphans existing profiles.

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
| `config/domsub-keep.txt` | Files exempt from domain substitution (Google endpoints kept) |
| `patches/` | Rooster's own patches to the Helium-patched tree |
| `src/` | Files added to the tree as-is: `//rooster` and vendored crates |
| `scripts/rooster` | The build tool |
| `scripts/merge_patches.py` | Combines the three patch series minus skips |
| `scripts/install_overlay.py` | Copies `src/` in, refusing to overwrite Chromium files |
| `scripts/rebrand.py` | Helium → Rooster in branding, source and strings |
| `resources/make_icons.py` | Generates every logo asset from one definition (`uv run resources/make_icons.py`) |

Unit tests for Rooster's code: `scripts/rooster build rooster_unittests`, then
run `out/Default/rooster_unittests` in the build's `src` directory.

## Status

**Milestone 1 (this): a buildable, rebranded, de-serviced Helium.**
All patches apply to Chromium 156.0.8078.47 and `gn gen` resolves the whole
build graph, but it hasn't been compiled yet; expect a round of compile fixes
on the first Mac build.

Downloads that Helium proxies go **directly to Google**, as in Chrome:
Chrome Web Store installs and updates (`clients2.google.com`,
`chromewebstore.googleapis.com`), component updates such as Widevine and
certificate revocation lists (`update.googleapis.com`), and spellcheck
dictionaries (`redirector.gvt1.com`). Google sees those requests; extension
usage pings stay off. See `patches/rooster/direct-google-downloads.patch` and
`config/domsub-keep.txt`.

Known gaps, by design for now:
- Ad blocking covers network requests only so far (see "Ad blocking").
- No auto-update (Sparkle is skipped).
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
