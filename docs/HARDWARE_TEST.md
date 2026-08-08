# 163 Music hardware test checklist

Run this checklist on at least one Old 3DS/2DS and one New 3DS with DSP
firmware available. Stereoscopic changes require a 3D-capable Old 3DS/3DS XL,
a New 3DS when available, and a 2DS-family fallback check. Debug logging is off
by default. Enable it in Settings and restart the app before collecting
non-sensitive memory, DSP and 3D diagnostics from
`/3ds/ClouDS-Music/hardware.log`; the app never writes login tokens, cookies,
full URLs, QR code keys or media authorization parameters there.

## Cold start and memory

Before borrowing an Old 3DS, run `make old3ds-stress` on a New 3DS or in
Azahar with New 3DS mode disabled.  The 28 MiB application and linear heaps
are deliberately conservative, but this remains a memory-allocation stress
test rather than a substitute for Old 3DS timing and hardware behavior.

1. Cold-start the `.3dsx` from Homebrew Launcher. Confirm both screens leave
   black promptly for the font-independent `ClouDS Music / LOADING UI` frame,
   then show the localized seven-stage progress screen. The displayed step
   must advance through settings, queue, network, audio, account and worker
   initialization before `7/7`; record any step that remains visible for more
   than two seconds and confirm the normal UI replaces `7/7` without a
   blank frame.
2. With a large queue and media cache, confirm cache statistics and offline
   playability checks continue after the normal UI appears without freezing
   input. While the background scan is active, start playback or open Search;
   the interactive task must preempt the scan. Enter Settings afterward and
   confirm cache totals eventually become accurate. Repeat with Wi-Fi disabled
   and confirm cached queue entries become selectable progressively.
3. Confirm both screens render smoothly at the 30 FPS presentation rate while
   held D-Pad input and playback progress continue updating responsively at the
   60 Hz main-loop rate, without missed repeats or audio underruns.
4. Confirm the D-Pad and Circle Pad both navigate lists and the 2x2 Discover
   home in the visible up/down/left/right directions.
5. Open Search from Discover, type `wangyiyun`, commit a Chinese candidate,
   cancel and reopen the IME three times. Confirm Chinese and multi-character
   keyboard labels use the fixed-menu Noto BCFNT, single ASCII keys use the 2x
   pixel font, candidates and committed input use the point renderer, up to
   nine candidates fit on one row, selection remains legible, and touching the
   first and ninth candidate commits the expected word.
6. Check `hardware.log`: `heap_free`, `linear_free` and `vram_free` must remain
   comfortably above zero and must not continually fall after reopening the
   IME. `app_free` is the kernel application region and is not a substitute
   for allocator headroom. Confirm `profile=old3ds-stress` for the constrained
   build and that `model` identifies 2DS-family systems correctly.
7. At native screen resolution, inspect every semantic text style. Confirm
   `NOW PLAYING` and other ASCII tabs remain crisp integer pixel text. Fixed
   labels, help actions, dialogs, page indicators and settings copy must use the
   anti-aliased menu BCFNT without changed baselines or clipped strokes. Song
   rows, lyrics, names, search input and IME candidates must retain native 12px
   point glyphs within the 18px semantic line height; dynamic 21/24px
   `TITLE`/`DISPLAY` roles use the native 15px large strike without texture
   scaling or sub-pixel positioning. Confirm common Korean uses the point
   renderer and an unsupported dynamic glyph becomes a stable square. Confirm
   cyan, gray, orange, red and white text match across both renderers. Record
   cold-start time plus `linear_free` before and after the menu BCFNT change.
   On both Japanese- and non-Japanese-region systems, also verify
   `スーパー / 働く / 辻 / 髙橋 / 﨑山 / 𠮷岡 / ｱｲﾄﾞﾙ` in song rows, the
   queue, search input and normal lyrics. No character may depend on the system
   font or turn into a replacement square.
8. Fill the four-row bottom playlist with long Chinese song and artist names.
   Confirm both 18px lines remain readable and truncate inside the panel. Also
   inspect every page's 18px BCFNT control actions, page heading, back hint and
   playlist heading; the potentially dynamic bottom status line remains point
   rendered.
9. In Settings, toggle Debug Logging on, restart and confirm a new `startup`
   entry is appended. Toggle it off, restart again and confirm the file size and
   last line no longer change. Existing log contents must not be deleted.
10. Confirm the non-selectable contact footer shows `cadl@duck.com` in both
   languages and Up/Down focus remains limited to the four actionable rows.
11. On a console using Luma3DS v10.3 or newer, temporarily rename the owner's
    `/3ds/dspfirm.cdc` while the app is closed, then cold-start the app. Confirm
    the bottom-screen modal reports `D880A7FA`, shows the default
    `L + Down + SELECT` shortcut and the exact `Miscellaneous options...` >
    `Dump DSP firmware` path, and blocks normal input until `A` or `B` closes
    it. Use Rosalina from HOME Menu to restore the owner's DSP firmware, fully
    close and relaunch the app, then confirm the modal no longer appears and
    `hardware.log` records `dsp=ready`. Never copy, publish or commit the dumped
    firmware.

## Network, TLS and cancellation

With Debug Logging enabled, request, media and playback failures append a
single-line entry containing `event`, `operation`, `category`, `unix_time` and
a sanitized `detail`. Non-fatal song detail, cover and lyric request failures
use `event=request_warning` so playback can continue. Repeated automatic HTTPS
probe failures are logged only once until connectivity succeeds again.

1. Open Recommendations from Discover and refresh it with `X`.
   Confirm the compact list shows up to eight text-only rows without generated
   cover placeholders, long Chinese titles remain inside the title column, and
   moving beyond the visible rows scrolls the selection without wrapping early.
   In both Chinese and English, confirm the page number and both paging arrows
   remain fully visible above the bottom edge. Repeat the footer check in My
   Playlists, playlist tracks, and Search results.
2. Start a song on a slow connection; confirm the UI remains responsive.
3. Confirm the timeline shows the flushed download range in muted gray during
   initial loading. Playback should start at 15% for ordinary files, subject to
   the 256 KiB minimum and 1 MiB maximum; `hardware.log` records `loaded`,
   `total`, and `target` on `stream_open_ok` for verification.
4. During playback, confirm red playback progress overlays the gray loaded
   range, and gray fills the timeline after the download completes. It should
   remain less prominent than the red playback position and orange buffering UI.
5. Confirm playback starts while `/3ds/ClouDS-Music/data/<song-id>/audio.mp3.part`
   or `audio.trial.mp3.part` is still growing and before the download completes.
6. Throttle or interrupt the connection long enough to exhaust the buffer.
   Confirm the play button shows the orange loading animation, the position
   remains stable, and playback resumes automatically after data returns.
7. During progressive playback, drag the progress bar and confirm the UI says
   seek is available after download completes. After completion, confirm seek
   works, does not restart the song from zero, and playback has no audible gap
   when the completed file and full seek index take over.
8. Press `B` during initial prebuffering and verify it returns to the previous
   playable state without leaving a `.part` file.
9. Save a valid login, disable Wi-Fi, and restart the app. Confirm the crossed
   Wi-Fi icon appears, Discover explains that its online features are
   unavailable, and `auth.bin` remains present. Restore Wi-Fi and confirm the
   icon clears and the saved account validates without scanning a new QR code.
10. With Wi-Fi disabled, use a queue containing cached and uncached songs.
    Confirm uncached rows are dimmed and cannot receive focus by D-pad or touch;
    previous, next, shuffle, repeat, and automatic advance must only choose
    cached rows. Confirm cached rows show the download icon both online and
    offline. A fully cached local song must start without an API request.
11. With Wi-Fi associated, replay a song whose permitted audio, cover, and
    lyrics are fully cached. Confirm playback starts without showing the audio
    resolve or full-index preparation as a prerequisite, then confirm the cover
    and lyrics appear while the seek index is still being prepared. Seeking
    must be temporarily unavailable and become accurate without an audible gap
    when the background index takes over. Switch to another song during index
    preparation and confirm the old scan cancels promptly.
12. While Wi-Fi remains associated, force DNS, connection, receive, and timeout
    failures separately. Confirm each failure preserves `auth.bin`, shows the
    offline state, and allows a cached queue item to play. Restore reachability
    without toggling Wi-Fi or pressing `A`; confirm the crossed Wi-Fi icon clears
    after an automatic HTTPS HEAD probe. With a controlled resolver or endpoint,
    confirm retries back off at approximately 2, 5, 10 and 30 seconds, remain at
    30 seconds, pause while the shell is closed, and never block a foreground
    request. Only an explicit invalid-session response may delete the credential.
13. Log in with an account entitled to a full VIP file and cache it, then verify
    it plays offline while the saved login remains. Explicitly log out and
    confirm that full VIP cache becomes unavailable; a trial cache remains
    selectable.
14. Repeat on a slow or fragmented SD card. Confirm brief stalls do not show the
    loading animation and longer stalls do not freeze input or rendering.
15. Using a controlled HTTPS test endpoint, return a cover larger than 2 MiB and
    audio larger than the selected cache limit, both with and without a
    `Content-Length` header. Confirm each transfer stops, removes its `.part`
    file and reports the size limit without affecting existing cache files.
16. On a disposable SD card image with less than 64 MiB free, start a cover and
    an audio download. Confirm both are rejected before writing, existing files
    remain readable and the UI reports that free space is reserved.
17. Force a certificate verification failure during a recommendation request,
    a QR login request and an audio download. Confirm `hardware.log` records the
    corresponding `request_failure` or `media_failure`, operation and
    `BADCERT_*` detail after exactly one automatic retry, and the UI advises
    checking the console date/time or updating the app. Confirm the log contains
    no full URL, `MUSIC_U`, Cookie,
    QR `codekey` or media authorization value. Leave the automatic probe
    retrying for at least two intervals and confirm the same outage produces
    only one `operation=network_probe` failure entry; restore connectivity and
    confirm a later outage can produce a new entry.

## Playback

1. Play three standard MP3 songs to completion and verify automatic next-song
   behavior.
2. Verify pause/resume with both `A` on the current Library item and the touch
   play button; also verify touch previous/next, progress dragging and the
   physical volume slider.
3. Test sequential, repeat-one and shuffle modes with both `SELECT` on Now
   Playing and the touch mode button.
4. Confirm album art orientation/colors and lyric highlighting over a full song.
   Confirm people, text, and other asymmetric details in the cover are upright,
   not vertically mirrored.
   Each adjacent lyric change should scroll and cross-fade smoothly without
   font-size popping, header overlap or text leaking outside the lyric panel.
   Use a track whose opening lyric timestamps are less than 150 ms apart and
   confirm rapid consecutive changes continue forward without flashing back to
   the previous line.
   Seek across several lines and switch tabs; the lyric view should reset to the
   correct line without animating through every skipped row.
   After the current cover/lyrics load and the next-song prefetch finishes,
   confirm the bottom status remains readable and never turns into replacement
   glyphs or random ASCII.
   Play a Korean track on a non-Korean-region system and confirm common Hangul
   lyrics use the embedded Korean font instead of question marks. Include one
   decomposed-Jamo fixture and confirm it renders as composed syllables, then
   include a deliberately unsupported rare syllable and confirm it becomes a
   stable square rather than corrupting adjacent UTF-8 text. Record
   `linear_free` before and after the first Korean lyric appears; it should not
   allocate a second font file or cause an audio underrun or visible input stall.
   Play a Japanese track containing full-width and half-width kana, `ー`, `〜`,
   `髙`, `﨑` and `𠮷`; confirm the same embedded point glyphs appear in normal
   and immersive lyrics. Record `app_free` and `linear_free` before playback
   and after entering immersive mode on an Old 3DS/2DS profile.
5. On a 3D-capable system, start with the 3D slider at zero, then raise it
   slowly. Confirm the active lyric is the nearest tier; the rows immediately
   above and below share the next tier, followed by another symmetric pair and
   the outer tier. Each step should be perceptible without breaking the lyric
   stack into unrelated layers. The separation must remain comfortable at the
   maximum slider position, with no double image, vertical disparity or visible
   left/right animation mismatch. Confirm the cover, artist and title form
   shallow 1/2/3-pixel total-disparity tiers behind the active lyric while the
   panel border and playback-status label remain on the screen plane. Their
   high-contrast edges must stay fused and crisp at every slider position.
   Repeat with a song that has no synchronized lyrics and confirm the playback
   card still has depth. Lowering the slider to zero and leaving Now Playing
   must restore ordinary 2D rendering without an audio underrun.
   With lyrics ready, confirm the lower help shows an enabled `Y` immersive
   action. Press `Y`: the top screen must become fully black behind native 24px
   bitmap lyrics. Press `Y` repeatedly and confirm the page cycles through exactly
   four styles without exiting: Lyric Wheel keeps the active lyric enlarged at
   center while surrounding rows roll along a shrinking, fading arc; Center Flip
   compresses and slides the previous line upward while the new line unfolds
   at screen center; Flash Fade keeps the screen empty before a line timestamp,
   shows only the current line at full opacity, holds it through most of the
   line, then quickly fades it before the next timestamp without moving or
   retaining the previous line;
   Opening Crawl comes last and keeps the current lyric on the nearest bottom
   row when it first appears, never reveals future lines, and moves the visible
   stack linearly toward the vanishing point without easing to a stop at lyric
   boundaries.
   Distant rows must be visibly narrower and
   vertically compressed. Confirm the
   `B`/`Y` hints and style name remain opaque for three seconds, fade smoothly over
   roughly one second, and reappear after switching styles. At maximum slider
   position the active text must remain crisp, with no soft filtering, doubled
   glyph edge, vertical disparity or excessive horizontal disparity. The crawl's
   nearer lower rows should have more parallax than the distant upper rows while
   staying comfortable. The bottom framebuffer must be pure black with no queue,
   battery or stale dialog pixels. Press `B`; it must restore the ordinary
   two-screen layout without also deleting, seeking, cancelling work or changing
   tabs. A song without lyrics must show the action
   disabled and ignore `Y`; changing tracks while immersed must return to the
   ordinary Now Playing page if matching lyrics are no longer available.
6. On Old/New 2DS, confirm Now Playing remains 2D and does not lose text,
   responsiveness or brightness. A failed optional right-eye allocation must
   record `stereo_target_failed`, continue in 2D and show `3D unavailable` when
   no higher-priority startup error replaces the status. The immersive page
   must still render centered 2D lyrics and restore the bottom UI on exit.
7. Without headphones, close the lid for at least 30 seconds. Reopen it and
    verify playback resumes from the suspended position and the current slider
    position still produces the expected depth. If a request lost connectivity
    during sleep, confirm reopening immediately schedules a HEAD probe and the
    app leaves offline mode once `https://music.163.com/` responds, without
    requiring an `A` retry.
8. Connect headphones and, while playing a fully cached song, close the lid for
   at least two minutes. Confirm audio remains audible throughout, its position
   advances by roughly two minutes, and reopening restores rendering without an
   underrun.
9. Repeat the headphone lid test while progressively downloading, buffering,
   and crossing an automatic next-song transition. Confirm download and
   playback continue without leaving a `.part` file after completion.
10. While headphone lid playback is active, unplug the headphones. Wait at
    least 30 seconds, reopen the lid, and confirm the console entered sleep and
    playback resumes normally rather than continuing through the speakers.
11. Pause playback with headphones connected, close and reopen the lid, and
    confirm the console sleeps and playback remains paused. Also confirm HOME
    still suspends audio and returning to the app resumes it.
12. On an original 2DS, repeat the playing and paused cases with the physical
    sleep switch and record any difference from clamshell models.

## Library queue and cache

1. Add at least six songs, reorder and delete items, then restart the app.
2. Confirm playlist order, play mode and volume are restored.
3. On Discover/Settings, confirm only the top-screen item has focus, then use both
   `SELECT` and the `LIBRARY` title to move focus exclusively to the bottom.
4. Press `B` and verify focus returns to the top-screen content; confirm `L/R`
   also resets focus when changing tabs.
5. With at least ten queued songs, test `UP`/`DN` one-row movement and
   `LEFT`/`RIGHT` four-row page movement, including clamping at both ends.
   Repeat offline with cached and uncached rows interleaved; page movement must
   never focus an uncached row. Then test `DEL` and the second-tap confirmation
   for `CLR`.
6. In Settings, switch to English and confirm the top and bottom screen labels,
   control hints, and status messages update immediately. Restart and verify
   English persists, then switch back to Chinese and verify it also persists.
7. Confirm the compact Settings layout keeps the language, cache limit, and
   clear-cache rows readable and fully inside the 400x240 top screen in both
   languages.
8. Select each 64/128/256/512 MB cache limit and the unlimited option, then
   restart to verify persistence. With unlimited selected, verify downloads
   still stop before consuming the 64 MiB SD-card reserve. Lower the limit
   below current usage, confirm that the first `A` only asks for confirmation,
   and verify pruning retains the current song and its cover and lyrics.
9. Select Clear Cache and verify the first `A` only asks for confirmation. Press
   `A` again, confirm the UI remains responsive, and verify the current song is
   retained while other MP3, cover, and lyric files are removed.
10. Play a song with synchronized lyrics and confirm
   `/3ds/ClouDS-Music/data/<song-id>/lyrics.lrc` is created without a leftover
   `.part`.
   Restart the app and play it again; confirm the lyrics appear and the cache
   file remains valid. Corrupt that file, retry, and confirm it is replaced by
   a valid network response without crashing.
11. With at least three queued songs, fully cache the current song's MP3, cover,
   and lyrics, then leave the next song uncached. Keep the current song playing
   and confirm the next song receives all three cache files without changing
   the visible playback status or causing an underrun.
12. Fully cache the item immediately after the current song and leave the
   following item uncached. Confirm background prefetch skips the complete
   item, follows playlist order across the end of the queue, and caches only
   the first incomplete item.
13. During background prefetch, start a search, change songs, and press `B` in
   separate runs. Confirm foreground actions remain immediate, cancellation
   leaves no `.part` file, and lowering or clearing the cache still retains
   the current song and respects the configured limit.
14. Bulk-add playlists until the local queue reaches 1000 songs. Confirm the
    next single-song add uses the existing oldest-song replacement prompt,
    while another bulk add stops without evicting entries. Restart and verify
    all 1000 songs reload; then switch offline and check that queue navigation,
    snapshot compaction, `heap_free`, and input responsiveness remain healthy
    on Old 3DS/2DS.

## Login

1. While logged out, play a known VIP song to the end of its preview. Confirm
   the status identifies a trial and only `audio.trial.mp3` is committed for
   that song; `audio.mp3` should not appear.
2. Using an account with an active subscription, select Account on the Discover
   home, generate a QR, scan with the official NetEase mobile app and confirm.
3. Replay the same song. Confirm it reaches the full duration, commits
   `audio.mp3`, and removes the previous `audio.trial.mp3`.
4. Confirm the nickname appears on the Account card and Recommendations becomes
   personalized.
5. Log out and replay the same song. Confirm the cached full file is not played,
   the status identifies a trial, and a successful preview download replaces
   `audio.mp3` with `audio.trial.mp3`.
6. While logged out, select My Playlists and verify a successful login continues
   directly to the on-demand playlist load.
7. Confirm created and collected playlists are labelled correctly, each page
   fits eight text-only rows without generated cover placeholders, page through
   both playlists and tracks, then play a track from a playlist.
8. With an affected account whose user-playlist response previously exhausted
   8192 JSON tokens, confirm each page still loads eight music playlists, video
   entries are omitted, next-page detection is correct, and an actual request
   failure is shown instead of the empty-playlist message.
9. Open a playlist whose decrypted detail JSON exceeds 2 MiB. Confirm the first
   page loads without a response-size error, subsequent pages reuse
   `playlist-tracks.bin`, and Old 3DS `app_free` remains stable while paging.
10. Cancel the first-page load of that large playlist. Confirm no
   `playlist-tracks.bin.part` remains and a previously committed index is not
   replaced by the cancelled response.
11. Press `B` once to return from tracks to playlists and again to return to the
   Discover home; confirm Settings replaced Search as the third top-screen tab.
12. Restart the app and verify the saved session is validated automatically.
13. Confirm the first `X` only asks for logout confirmation; press `X` again and
   verify `auth.bin` is removed and public recommendations
   still work.

Record the console model/firmware, SD card filesystem and the relevant
`hardware.log` lines with any failure report.
