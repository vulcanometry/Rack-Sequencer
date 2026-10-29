# Grid Sequence — VCV Rack 2 plugin

See [MANUAL.md](MANUAL.md) for the complete user manual.

**THE GRID** (46HP): an 8-track, scale-quantized piano-roll sequencer with 64 patterns of 16 steps, a song arranger and a built-in clock.

## Panel

| Area | What it does |
|---|---|
| Info bar | Left half: the song name, large: **click it to rename the song** (unnamed songs show `Untitled`). Right half: transport (`STOP`, `LOOP`, `SONG 03/12`, `ONCE 03/12`), the playing pattern (`P05`, green while the grid follows a song) and the bar and beat within it (`BAR 2.3`). Key, tempo and track have their own displays on the panel. |
| Grid | 16 steps across, 15 scale rows visible. The knobs, transport buttons and clock jacks are in the column on the left. Notes can be as long as the track (up to 64 steps; a drag can run past the edge of the view) and are edited with the mouse: **press an empty square and drag right** to draw a note of that length (a plain click makes a 1-step note); **drag a note's right end** (the grip) to stretch or shorten it; **drag anywhere else on a note** to move it to another step or pitch; **click a note** without dragging to delete it; **Shift+click a note** to tie it (its gate stays high to its end, so a note that follows straight on slides in without retriggering; a slur marks it). **Alt+wheel** transposes the edited track a scale degree; **Alt+Shift+wheel** rotates it a step earlier or later within its length. Every note, track, pattern and song edit, and every change to a track's setup (poly voices, MOD mode / range / glide) or the song name, can be undone with **Ctrl+Z**. The note's length in steps is shown while dragging. A track plays one note at a time (make it a **poly track** for chords, below), so a note trims or replaces whatever it overlaps on that track — but only where you release it; dragging across other notes and back leaves them intact. The edited track is drawn bright; the other tracks show as faint "ghost" notes in their colours. Arrows at the top/bottom edge mark notes that are scrolled out of view. |
| UP / DN, mouse wheel | Scroll the grid through 8 octaves |
| MOD (button between UP and DN) | Switches the grid to the **MOD view**. Each track has two MOD modes, switched with the NOTE/STEP badge at the top left of the view (or the right-click menu): **NOTE** gives each note a MOD value (0–100%), held while it plays; **STEP** gives every step of the track its own value, and MOD glides continuously from each step's value to the next (independent of notes, gates and gate length; a menu option makes it jump instead). In NOTE mode each note is a bar as high as its value; in STEP mode every step is a bar, with the notes marked along the bottom. Press to set a value, drag across to draw values, mouse wheel nudges one by 1%. New notes get the last note value set on that track. In the note view, a dark strip along the bottom of each note shows its value. |
| Step ruler (above the grid) | Shows step numbers for the steps in view and the edited track's end marker. **Click or drag** along it to set where the track ends (1–64 steps; each track in each pattern has its own length); **mouse wheel** over it nudges the length by one. Steps past the track's end are dimmed: notes there are kept but don't play. |
| Horizontal scroller (below the grid) | An overview of all 64 steps showing every track's notes at their pitch, the edited track's length and playhead, and a thumb marking the 16 steps in view. **Drag the thumb** to pan, **click** elsewhere to centre the view there, **mouse wheel** over it pans one step. Shift+wheel or a horizontal wheel over the grid pans a beat. While the sequencer plays, the view follows the edited track's playhead, turning to the next 16 steps as it gets there (turn off "Follow playhead" in the right-click menu to browse freely while playing). |
| Track lengths & pattern length | Each track loops over its own length, so tracks of different lengths drift against each other (polymeter); all of them restart when the pattern starts again. A pattern lasts as long as its longest track that has notes (a pattern with no notes, as long as its longest track), and the song moves on — or a SEQ change takes effect — when it ends. The ruler's end marker shows the edited track's length. |
| Song strip | The arrangement (up to 128 slots). Left-click a slot to move the cursor to it and show its pattern in the grid (SEQ turns to it); **drag a slot** to move it to another place in the song (hold it past either end of the strip to carry it onto the next or previous page); mouse wheel over a slot changes its pattern; right-click a slot for set / open / insert / duplicate / delete. Click or wheel over the `SONG` label to change page / move the cursor. The playing slot is green. |
| TEMPO | 30–300 BPM; each step is a 16th note. The display beside the knob shows the current tempo (amber when measured from CLK IN). |
| GATE | Value display with < > arrows: gate length, 5–100% in 5% steps (default 50%). Where each note releases within its last step; a long note's gate stays high through all its other steps. 100% = legato (back-to-back notes are tied). A tied note (Shift+click) always holds its gate to the very end. |
| ROOT / SCALE | Value displays with < > arrows (click, or mouse wheel), named on their left. SCALE shows a short name (e.g. `HarMin`); the full name is in its tooltip. Key for every track (14 scales). Notes are stored as scale degrees, so changing root/scale re-voices patterns live. |
| SEQ | Value display with < > arrows. Pattern 1–64 shown in the grid. In pattern mode it is also the pattern that plays (changes take effect at the next bar). |
| SONG buttons (below the song strip) | **MODE** cycles through three modes on each press: light off = loop the SEQ pattern; yellow = play the song and loop it; orange = play the song once, then stop and return to the start (RUN out sends its stop trigger, so a synced follower stops too). While a song is playing the grid follows the pattern being played (and edits go to it); stopping leaves the pattern that was playing in the grid (SEQ turns to it), and you can then browse and edit other patterns with SEQ. **INS** inserts the SEQ pattern before the cursor, **ADD** adds it after the cursor, **DUP** duplicates the cursor slot, **DEL** deletes it, **SET** sets it to the SEQ pattern, **EDIT** turns SEQ to the cursor slot's pattern, **CLR** empties the song. A new module's song starts empty; until slots are added, song mode plays the SEQ pattern. |
| Song files | Right-click menu, **Song** section: a **song name** text box (also editable by clicking the name in the info bar), **Load song...** / **Save song...** (the whole sequencer, as a Rack preset `.vcvm`, so songs also show in the Preset menu), and **Import MIDI file...**: each MIDI track/channel becomes a track (chords make it a poly track), notes snap to 16th steps, velocity becomes MOD, the scale is set to Chromatic, and the piece is cut into 4-bar patterns (repeats shared) strung into the song. Loading and importing can be undone. |
| PATTERN buttons | **COPY / PASTE / CLR** the pattern shown in the grid (copy in one pattern, step SEQ to another, paste) |
| TRACK buttons | **COPY / PASTE / CLR** the selected track of the pattern shown in the grid |
| RUN, RESET | Transport buttons; `RUN IN` toggles run, `RST IN` resets to step 1 / song slot 1. In pattern mode, starting always plays the SEQ pattern shown in the grid (from step 1 if a different pattern was playing before). |
| CLK IN | External clock: one step per rising edge (overrides the internal clock) |
| PAT IN | Next pattern. While unpatched, each pattern moves on when it ends. While patched, the current pattern keeps looping until a trigger arrives here; the next step then starts the next song slot (or, in pattern mode, the SEQ pattern) from step 1. |
| CLK | Step clock out (50% duty) |
| EOP | End of pattern: a trigger each time the sequencer moves on to the next pattern, in pattern and song mode (including the last pattern of a play-once song) |
| RUN, RST (outputs) | RUN sends a trigger on every start/stop (or, via the right-click menu, a gate while running); RST sends a trigger on every reset |
| Poly tracks | Right-click → **Poly voices** sets how many notes the selected track can play at once (1 = mono, up to 8). A poly track's notes can stack into chords: each note goes on the first voice that is free for its whole length; where all are busy, no note. Drag a note's grip up or down to move the whole chord (every note starting on that step) together. Its GATE, CV and MOD jacks then carry one channel per voice as a poly cable. It still takes up just its own track, so the other tracks stay free for mono or poly parts. To drive separate mono modules instead, turn on **Also send each voice to its own mono jacks**: voices 2, 3… also come out in mono on the next tracks' jacks (a bar beside the track numbers joins them to the poly track, and they don't play their own notes meanwhile). `x3` under the track number marks a poly track. |
| TRACK 1–8 | Select the track you're editing, mute it, and patch its GATE (10V), CV (1V/oct, C4 = 0V) and MOD outputs. The rings round the GATE and CV jacks light in the track colour while the gate is high. MOD sends each note's MOD value (velocity, filter, anything), held until the next note, or in STEP mode the value of each step as it plays; its range (0–10 V, 0–5 V or ±5 V) is set per track in the right-click menu. |

The module's right-click menu repeats the copy/paste/clear functions and adds transposing or rotating a track, the RUN gate/trigger options, and resetting the grid view.

### Syncing two sequencers

Patch the leader's **CLK → CLK IN**, **RUN → RUN IN** and **RST → RST IN** on the follower. The follower then steps, starts/stops and resets with the leader (a clock and reset arriving together, in either order within 1 ms, start on step 1). Both should be in the same run state when you patch them; alternatively set the leader's RUN output and the follower's RUN IN to gate mode in the right-click menu, which keeps them in step automatically.

For **full sync** of pattern changes, also patch the leader's **EOP → PAT IN** on the follower. The follower then changes pattern (moves to its next song slot) exactly when the leader does, whatever its own pattern lengths: a shorter follower pattern loops until the leader's pattern ends. A trigger arriving with the clock, or up to 1 ms after it, counts as the first step of the next pattern. Chain further sequencers the same way, follower EOP → next PAT IN. Leave PAT IN unpatched to keep a sequencer's patterns independent while it still shares the clock and transport.

## Building

Requires MSYS2 (MinGW64) and the Rack SDK. The Makefile expects the SDK at `../Rack-SDK`.

```sh
# from an MSYS2 MINGW64 shell
cd /d/Source/AIProjects/Sequence
make -j8                 # builds plugin.dll
make dist                # creates dist/GridSequence-2.0.0-win-x64.vcvplugin
make install RACK_USER_DIR="C:/Users/<username>/AppData/Local/Rack2"
```

Rack extracts and loads the `.vcvplugin` on its next start.
