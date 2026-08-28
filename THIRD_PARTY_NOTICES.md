# Third-party notices

- The pinyin lookup design is adapted from the MIT-licensed
  [Fishason/DSSH](https://github.com/Fishason/DSSH), copyright 2026
  exdekotive. The upstream copyright and MIT terms are reproduced in
  `third_party/MIT-attributions.txt`.
- `romfs/pinyin_dict.bin` is generated from a pinned snapshot of
  [rime-ice](https://github.com/iDvel/rime-ice) and is distributed under
  LGPL-3.0. The complete license is stored in
  `third_party/rime-ice-LGPL-3.0.txt`; corresponding dictionary sources are
  reproducibly downloaded by `tools/fetch-ime-assets.sh`. Official source
  release archives include the exact six input dictionary files under
  `data/pinyin_dict_src/` together with the generator and license.
- `romfs/immersive-font.bin` is a 24px monochrome glyph subset rasterized from
  pinned [Noto Sans CJK](https://github.com/notofonts/noto-cjk) Sans 2.004
  Simplified Chinese Regular source under SIL Open Font License 1.1.
  `romfs/ui-menu-font.bcfnt` is the 502-glyph A4 subset used only for fixed
  menu copy, buttons, dialogs and control hints.
  `romfs/content-point-font.bin` is the corresponding 12px visible-glyph
  subset used by 18px semantic UI content, and
  `romfs/content-large-point-font.bin` is the 15px strike used by 21/24px
  semantic titles. All three contain the same Chinese, Japanese, Latin and
  3,500-syllable Hangul repertoire. Japanese coverage selects the glyphs
  available from the strict Shift_JIS JIS X 0208/0201 mapping, adds supported
  BMP kana and punctuation, and includes a curated set of common name variants
  found in music metadata. Han and Hangul bitmaps receive a bounded
  one-pixel visual-center correction during offline conversion; Latin and
  punctuation retain Noto's baseline placement. The complete license is
  stored in `third_party/Noto-Sans-CJK-OFL-1.1.txt`; pinned downloads,
  checksums, whitelist selection and deterministic conversion are reproduced
  by `tools/fetch-content-font.sh`, `tools/gen_ui_font_whitelist.py`,
  `tools/normalize_bcfnt.py`, `tools/gen_content_font_whitelist.py` and
  `tools/gen_immersive_font.py`.
- Common Traditional Chinese font coverage is derived from the Simplified to
  Traditional character and Taiwan/Hong Kong variant mappings in pinned
  [OpenCC](https://github.com/BYVoid/OpenCC/tree/81223ed87ae53283ef518e2deac34b7971f8a39e),
  distributed under Apache License 2.0. Only Traditional counterparts of the
  existing base repertoire are selected; OpenCC code is not linked into the
  application. The complete license is stored in
  `third_party/OpenCC-Apache-2.0.txt`.
- The Korean subset selection aggregates the Korean OpenSubtitles 2018 word
  frequencies published in
  [FrequencyWords](https://github.com/hermitdave/FrequencyWords/tree/525f9b560de45753a5ea01069454e72e9aa541c6/content/2018/ko),
  copyright Hermit Dave and contributors, under CC BY-SA 4.0. The application
  does not distribute the source word list: the generation script retains all
  2,350 KS X 1001 syllables, ranks observed non-KS syllables by aggregate
  frequency, and fills the remaining slots using initial/vowel/trailing Jamo
  component frequencies. The complete license is stored in
  `third_party/FrequencyWords-CC-BY-SA-4.0.txt`. The derived repertoire
  selection is offered under CC BY-SA 4.0; the Noto glyph data remains under
  SIL Open Font License 1.1.
- `external/minimp3` is CC0/public domain dedication.
- `external/qrcodegen` is Project Nayuki's QR Code generator, distributed
  under the MIT License included in its source files and reproduced in
  `third_party/MIT-attributions.txt`.
- NetEase endpoint and EAPI compatibility details reference the MIT-licensed
  [darknessomi/musicbox](https://github.com/darknessomi/musicbox), copyright
  2020 omi. Its MIT terms are reproduced in
  `third_party/MIT-attributions.txt`.
- The pinned devkitPro image links the application with libctru 2.7.0,
  citro2d 1.7.0 and citro3d 1.7.1 under the zlib License. Their upstream
  notices are reproduced in `third_party/devkitPro-zlib-LICENSES.txt`.
- HTTPS and downloads link against curl 8.4.0 under the curl License, reproduced
  in `third_party/curl-LICENSE.txt`, and Mbed TLS 2.28.8 under Apache-2.0. The
  complete Apache-2.0 text is stored in
  `third_party/OpenCC-Apache-2.0.txt`.
- Cover decoding links against libpng 1.6.53 from the pinned devkitPro image,
  distributed under the PNG Reference Library License included in
  `third_party/libpng-LICENSE.txt`, and libjpeg-turbo 3.1.3. As required by the
  IJG terms: this software is based in part on the work of the Independent
  JPEG Group. The libjpeg-turbo license summary and BSD terms are in
  `third_party/libjpeg-turbo-LICENSE.md`.
- Compression links against zlib 1.3.1. Its notice is reproduced in
  `third_party/devkitPro-zlib-LICENSES.txt`.
- `romfs/cacert.pem` is extracted from the Mozilla CA store and distributed by
  curl. The Mozilla source is under MPL-2.0, reproduced in
  `third_party/Mozilla-MPL-2.0.txt`.
- Project-owned `icon-v4.png`, `banner-v2.png` and the silent `banner.wav` are
  distributed under the project MIT License. The project name and logo policy
  is documented in `TRADEMARKS.md`; it does not add a non-commercial software
  restriction.
- CIA builds use a pinned snapshot of
  [Project_CTR makerom](https://github.com/3DSGuy/Project_CTR/tree/e8f5f529c54ff9b22a2491a480ffa69206bf7b19/makerom),
  distributed under the MIT License. The tool is downloaded into the ignored
  `.tools/` directory and is not included in application release packages.
- CIA builds use a pinned snapshot of
  [3ds-bannertool](https://github.com/carstene1ns/3ds-bannertool/tree/734d33be79fd3f8c29c6296158f06ac7c5ca9dcb),
  copyright Steveice10 and carstene1ns, distributed under the MIT License.
  The tool is downloaded into the ignored `.tools/` directory and is not
  included in application release packages.
