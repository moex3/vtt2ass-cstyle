# vtt2ass-cstyle
A .vtt subtitle converter to .ass and .srt.

## Building
Clone the repo with
```sh
git clone --recurse-submodules https://github.com/moex3/vtt2ass-cstyle
```
Make sure the required dependencies are present: `harfbuzz` and `freetype2`.

Build with
```sh
make
```
By default it will build a debug build.

To generate both .srt and .ass subs in one run:
```sh
./v2a srt --output out.srt ass --output out.ass --width 1920 --height 1080 --font ~/.local/share/fonts/ipaexg.ttf  input.vtt
```
This will generate `out.srt` and `out.ass` with the specified video dimensions, using the `ipaexg` font from the the `input.vtt` file.

## Features
- Handle ruby tags for srt by enclosing them in parenthesis, and by positioning the text correctly for ass.
NO vertical ruby for now.
![srt ruby](https://ra.thesungod.xyz/MNDU3ZUE.jpeg)
![ass ruby](https://ra.thesungod.xyz/VK_f23Tc.jpeg)
- Basic vertical text support for ass subtitles.
- Bold, italic and underline support.

## Missing features
- No overlap detection.
- No region support.
- The positioning can be buggy (I can never get that right).
- Not tested with real word subtitles. If you want to use it, and it fails on that subtitle, make an issue.
- I wrote the vtt tokenizer myself, not according to the spec.
- Codebase quality. Enough said.
