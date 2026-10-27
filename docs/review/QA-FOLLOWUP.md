# Website QA follow-up — 27 October 2026

## Resolved in this PR

- Hero sound is off on every page load. An explicit, labelled toggle enables/mutes it; clicking an artist identity is also an explicit play/pause action. Keyboard focus alone never starts playback. The plugin gallery remains silent.
- The installation guide receives focus on opening. Close and Escape restore focus to its trigger.
- The image lightbox makes the background inert and restores focus on Close/Escape. The original PNG is loaded for enlargement, not the compressed thumbnail.
- The opening Hero fills the viewport, with a minimal circular arrow linking to the gallery and moving keyboard focus there. The former Explore the plugins text link was removed. The Hero includes extra bottom breathing room (80px desktop, 56px mobile) beyond the viewport; the scroll cue stays within the opening screen. Short screens can grow to preserve content.
- Hero and all 15 plugin thumbnails use responsive WebP variants with intrinsic dimensions. Reel mode requests a larger candidate. Existing full-size PNG assets remain available. See image-optimization.json for source and variant sizes: source images total 4,967,572 bytes; small variants total 416,680 bytes (about 92% smaller). This is an asset-size comparison, not a measured page-load-speed claim.
- Instrument and effect badges use Netflix red (#E50914) with white text. Small artist captions are 11px. Guide and Reel corners now match the rounded design.

## Verified

Manual interaction checks in the in-app Chromium browser and Chrome:

- Sound starts disabled; enable plays, mute pauses.
- Guide opening focuses Close; Escape returns to the guide trigger.
- Image enlargement works with pointer and Enter; Escape closes it, removes inert and restores the image trigger.
- The intro runs on every document load and crossfades into the Hero logo; reloads use a shorter transition with a subdued red halo. The loading logo matches the Hero logo width and moves from screen center to its destination while crossfading when that destination is visible. Skip and reduced-motion support remain.
- Grid/Reel and keyboard End navigation reach QUAZI as item 15; browsing remains silent (in-app browser).
- 390px mobile: no horizontal overflow; gallery shortcut focuses the gallery. Viewport override reset after checking.
- No JavaScript errors captured during the tested interactions.
- Both standalone scripts and all inline scripts parse; git diff --check passes.

Calculated contrast ratios for selected opaque colour pairs:

| Text | Ratio |
| --- | --- |
| White instrument badge text on Netflix red | 4.79:1 |
| White effect badge text on Netflix red | 4.79:1 |
| Netflix red large heading on card (#171717) | 3.74:1 |
| Card description | 8.74:1 |
| White preset text on Netflix red | 4.79:1 |
| Inactive view selector | 7.05:1 |

These checks cover the revised pairs, not every possible rendered state.

## Still requires external validation

- Firefox and Safari / iOS Safari are not available through this environment's browser controls.
- No live NVDA, VoiceOver or other screen-reader session was run. Keyboard and accessibility-tree inspection are not substitutes for that test.
- A complete accessibility audit, every hover/focus contrast combination, zoom/text resizing, real-device touch and throttled-network performance testing remain release checks.
- Installer execution is outside this website QA pass. No claim of full WCAG certification or all-browser compatibility is made.

Intro playback no longer uses a stored first-visit preference.
