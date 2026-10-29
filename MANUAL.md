# THE GRID — User Manual

**THE GRID** (plugin *Grid Sequence*) is an 8-track, scale-quantized piano-roll sequencer for VCV Rack 2. You draw notes on a grid, and each track sends a pitch CV and a gate. The module has 64 patterns, each track can be 1–64 steps long, and a song arranger chains patterns together. It also has a built-in clock, an external clock input and inputs and outputs for synchronizing several sequencers.

---

## Contents

1. [Quick start](#1-quick-start)
2. [Key concepts](#2-key-concepts)
3. [Panel overview](#3-panel-overview)
4. [The info bar](#4-the-info-bar)
5. [The note grid](#5-the-note-grid)
6. [The step ruler, scroller and track length](#6-the-step-ruler-scroller-and-track-length)
7. [Tracks: select, mute and outputs](#7-tracks-select-mute-and-outputs)
8. [Key: root and scale](#8-key-root-and-scale)
9. [Patterns](#9-patterns)
10. [Gate length](#10-gate-length)
11. [Clock and transport](#11-clock-and-transport)
12. [Song mode](#12-song-mode)
13. [Copy, paste and clear](#13-copy-paste-and-clear)
14. [Inputs and outputs reference](#14-inputs-and-outputs-reference)
15. [Synchronizing several sequencers](#15-synchronizing-several-sequencers)
16. [Right-click menu reference](#16-right-click-menu-reference)
17. [What is saved with the patch](#17-what-is-saved-with-the-patch)
18. [Workflow recipes](#18-workflow-recipes)
19. [Troubleshooting](#19-troubleshooting)
20. [Specifications](#20-specifications)

---

## 1. Quick start

1. Add **Grid Sequence → THE GRID** to your patch. It is 46HP wide.
2. Patch **Track 1 CV** to an oscillator's V/Oct input and **Track 1 GATE** to an envelope's gate input.
3. Press **RUN** to start it at 120 BPM (a new sequencer starts stopped). The **RUN** button's green light shows that it is running.
4. Click squares in the grid to add notes. Each column is one 16th-note step, and each row is a note of the current scale.
5. Turn **ROOT** and **SCALE** to change the key. The notes you drew follow the key.
6. Click a different **TRACK** button on the right, then draw notes for that track. Patch its CV and GATE to another voice.

---

## 2. Key concepts

| Term | Meaning |
|---|---|
| **Step** | One grid column. With the internal clock, a step is a 16th note: four steps per beat. |
| **Track** | One voice. There are 8 tracks, each with its own CV and gate output and its own colour. A track plays **one note at a time**. |
| **Note** | A coloured bar on the grid. It has a **pitch** (its row), a **start step** and a **length** in steps. |
| **Track length** | How many steps a track plays before it loops: 1–64. Each track in each pattern has its own length. The default is 16. |
| **Pattern** | All 8 tracks together, with their notes and track lengths. There are 64 patterns, numbered 1–64. |
| **Pattern length** | How long a pattern plays before it moves on: the length of its **longest track that has notes** (a pattern with no notes plays for its longest track: 16 steps for a new pattern). |
| **Song** | An ordered list of up to 128 **slots**. Each slot names a pattern to play. |
| **View** | The grid shows 16 steps at a time. Pan through longer tracks with the horizontal scroller under the grid. |
| **Scale degree** | Notes are stored as positions in the scale, not as fixed pitches. When you change the root or scale, every note moves to the matching degree of the new key. |

---

## 3. Panel overview

From left to right:

**Left control column**
- **Value displays:** SEQ, SCALE, ROOT and GATE (top to bottom), each named on its left, with `<` and `>` arrows.
- **Tempo:** the TEMPO knob, with a BPM display beside it.
- **Transport and inputs:** RUN and RESET buttons, plus the RUN IN, RST IN, CLK IN and PAT IN jacks.
- **Outputs** (dark pad): RUN, RST, CLK and EOP.

**Centre display**
- **Info bar**: the song name (click to rename) at the top.
- **Step ruler**: step numbers and the track's end marker.
- **Note grid**: 16 steps × 15 visible pitch rows. The **MOD** button in the strip to its right switches it to the MOD view ([§7](#mod-values)).
- **Horizontal scroller**: an overview of all 64 steps, for panning the grid.
- **Song strip**: the song arrangement.

**Function buttons** below the song strip, in three groups:
- **SONG**: MODE, INS, ADD, DUP, DEL, SET, EDIT, CLR.
- **PATTERN**: COPY, PASTE, CLR.
- **TRACK**: COPY, PASTE, CLR.

**Scroll buttons** sit to the right of the grid: UP and DN.

**Track section** on the right: one row per track, with the track number, TRACK (select) button, MUTE button, and the GATE, CV and MOD outputs.

Hover over any control or jack to see its tooltip. Double-click a knob to reset it to its default value. Right-click a knob or value display to type an exact value.

### Using the value displays (ROOT, SCALE, SEQ)

Each display shows its current value, for example `D#`, `Dorian`, `07` or `50%`. SCALE shows a short form of long scale names (`HarMin` for Harmonic Minor, `MajPen` for Major Pentatonic, and so on); hover over it for the full name.

| To… | Do this |
|---|---|
| Step forward or back one value | Click the **`>`** (right) or **`<`** (left) arrow. The arrow lights up when the mouse is over it. |
| Step quickly | Click repeatedly. Fast clicks keep stepping and never reset the value. |
| Step with the wheel | Mouse wheel over the display: up = next, down = previous. |
| Jump straight to a value | Right-click the display and type the value. This is also where you MIDI-map it or reset it to its default. |
| Undo a step | **Ctrl+Z** (Rack's normal undo). |

Values **wrap around** at the ends: B steps on to C, the last scale steps on to Major, and pattern 64 steps on to 01. GATE stops at 5% and 100% instead.

---

## 4. The info bar

The strip at the top of the display has the song name in its left half and where playback is in its right half:

| Item | Example | Meaning |
|---|---|---|
| Song name (left half) | `Night Drive` | The song's name. **Click it to rename the song**: a box opens with the name selected; type the new name and press Enter (or click elsewhere). A song that hasn't been named shows a grey `Untitled`. The name is saved with the patch and used as the file name when you save the song ([§12](#naming-saving-and-loading-songs)). |
| Transport | `STOP`, `LOOP`, `SONG 03/12`, `ONCE 03/12` | `STOP` = stopped (red). `LOOP` = running in pattern mode. `SONG` = running a looping song, at slot 3 of 12; `ONCE` = a play-once song. |
| Playing pattern | `P05` | The pattern that is playing (or will play when you press RUN). **Green** while a song is running: the grid then follows it, and it can differ from SEQ (see [§12](#12-song-mode)). |
| Bar and beat | `BAR 2.3` | Where playback is within the pattern: bar 2, beat 3 (4 steps per beat, 16 per bar). `BAR -.-` while stopped. |

The key, tempo and selected track have their own displays on the panel: the SCALE and ROOT displays, the BPM display beside the TEMPO knob (amber when following CLK IN), and the lit TRACK button with the ruler's end marker for its length.

---

## 5. The note grid

The grid shows **16 steps** and **15 pitch rows** at a time.

### Reading the grid

- **Row labels** on the left show the note name and octave of each row, for example `D#3`. C4 is 0 V. Rows that are the **root** of the scale are highlighted, so you can see each octave.
- **Columns** are shaded in groups of four steps, one group per beat.
- **The selected track's notes** are bright bars in the track's colour. A small **grip** (two short lines) at a note's right end is its stretch handle.
- **Other tracks' notes** appear as faint "ghost" bars in their own colours, so you can see the whole arrangement. Ghost notes of muted tracks are fainter still. You can only edit the selected track.
- **Arrows** at the top or bottom edge of a column mark a selected-track note that is above or below the visible rows.
- **The playhead** is a light column showing the selected track's current step. It is visible only when the grid shows the pattern that is playing.
- **Steps past the track's end** are darkened. Notes there are kept, but they don't play and can't be edited until you lengthen the track again ([§6](#6-the-step-ruler-scroller-and-track-length)).

### Editing notes

All editing applies to the **selected track** (see [§7](#7-tracks-select-mute-and-outputs)).

| To… | Do this |
|---|---|
| **Add a 1-step note** | Click an empty square. |
| **Draw a longer note** | Press on an empty square and drag to the right. The note grows as you drag, and its length in steps appears inside it. Release to finish. |
| **Lengthen or shorten a note** | Press on the **right end** of the note (the grip, the right part of its last step) and drag left or right. |
| **Move a note** | Press anywhere else on the note and drag. It follows the mouse to a different step or pitch and keeps its length. |
| **Delete a note** | Click the note without dragging. |
| **Tie a note** | **Shift+click** the note. A tied note holds its gate to the very end of its last step instead of releasing halfway, so a note that starts right after it plays without retriggering the envelope. A small slur over the note's end marks it. Shift+click again to untie it. |
| **Transpose the track** | **Alt + mouse wheel** over the grid moves every note of the selected track one scale degree up or down. If that would push a note off the top or bottom of the grid, nothing moves. |
| **Rotate the track** | **Alt + Shift + mouse wheel** over the grid shifts every note of the selected track one step later (wheel down) or earlier (wheel up), wrapping around at the track's end. |
| **Undo / redo** | **Ctrl+Z** / **Ctrl+Shift+Z** (Rack's undo). Undoable: every edit to notes, ties, chords, track lengths, MOD values, transpose and rotate; TRACK and PATTERN paste / clear; every song edit (insert, add, duplicate, delete, set, clear, moving slots); renaming the song; loading a song and importing MIDI; poly voices and sending voices to mono jacks; MOD mode, range and glide; and the panel's displays and knobs. Mute, track selection, the song cursor and the view / sync options are simple toggles and aren't recorded. |
| **Change a note's pitch** | Drag it up or down. You can also click a different row in the note's first column, which replaces it with a new 1-step note. Clicking a different row further along a long note shortens the long note so it ends just before the new one. |

**Rules while editing:**
- **One note at a time per track.** When you drop a note over others on the same track, the earlier note is shortened to end where the new one starts, and notes that start inside the new note are removed. (A **poly track** works differently: see [Poly tracks](#poly-tracks).)
- **Dragging is non-destructive until you release.** If you drag across other notes and back, they are restored. Only the final position counts.
- **Drags can run past the edge of the view.** While drawing, stretching or moving, you can drag beyond the right edge of the grid. The note keeps extending past the steps in view, up to the track's end. Pan with the scroller to see the rest of it.
- **Notes can't extend past the track's end.** Drawing and stretching stop at the last step of the track.
- A note **can be as long as the whole track**, up to 64 steps.
- **While rotating**, a note can hang past the track's end for a moment (it plays cut short there). Keep rotating and it wraps round to the start; rotating back puts it exactly where it was.

### Scrolling

| To… | Do this |
|---|---|
| Scroll **pitch** up or down | Mouse wheel over the grid, or the **UP** / **DN** buttons to the right of the grid. The grid covers 8 octaves. |
| **Pan** left or right through the steps | Use the **horizontal scroller** under the grid ([§6](#the-horizontal-scroller)). You can also use **Shift + mouse wheel** over the grid, or a **horizontal scroll** (tilt wheel or trackpad); each notch pans one beat (4 steps). |
| Return to the **middle octave** | Right-click the module → **Reset view to middle octave**. |
| Have the view **follow playback** | On by default: while playing, the view turns to the next 16 steps when the playhead gets there. Turn it off in the right-click menu → **Follow playhead**. |

---

## 6. The step ruler, scroller and track length

Above the grid is the **step ruler**, which sets track lengths. Below the grid is the **horizontal scroller**, which pans through the steps.

### Reading the ruler

- **Left corner**: labelled `STEP`.
- **Step numbers** at the start of each beat (1, 5, 9, 13, 17 …) for the steps in view.
- **Active steps** (inside the track's length) are lighter. **Inactive steps** (past the end) are dark.
- **End marker**: a bar and flag in the track's colour, just after the selected track's last step.
- **Playhead**: a short bar in the track's colour along the bottom edge under the step now playing.

### Setting a track's length (1–64 steps)

| To… | Do this |
|---|---|
| Set the length directly | **Click** a step in the ruler. The track now ends at that step. For example, clicking step 12 makes the track 12 steps long. |
| Sweep the length | **Press and drag** along the ruler. The end follows the mouse. |
| Nudge by one step | **Mouse wheel** over the step area: up = longer, down = shorter. |
| Set a length beyond the steps in view | Pan with the scroller until the step you want is in view, then click it in the ruler. For example, pan to steps 33–48 and click step 40. |
| Give every track in the pattern the same length | Set one track, then right-click → **Set every track in this pattern to N steps**. |

The ruler always edits the **selected track** in the **pattern shown in the grid**. The end marker shows the result.

**Shortening a track never deletes notes.** Notes past the new end are dimmed and silent, and they come back when you lengthen the track again.

### The horizontal scroller

The grid shows 16 steps at a time, but a track can be up to 64 steps long. The thin strip directly **under the grid** is a scroller and overview of all 64 steps.

**Reading it:**
- **Left corner**: the range of steps in view, for example `17-32`.
- **Lighter area**: the selected track's active length. Steps past its end are dark.
- **Thin dividers** every 16 steps.
- **Note ticks**: every note, drawn at its pitch, so you can see the shape of each line. The selected track is in its full colour; other tracks are faint.
- **White line**: the selected track's playhead while it plays.
- **Thumb** (the outlined, highlighted box): the 16 steps the grid is showing.

**Using it:**

| To… | Do this |
|---|---|
| Pan smoothly | **Drag the thumb** left or right. The grid follows one step at a time. |
| Jump to another part of the track | **Click** anywhere outside the thumb. The view centres there, and you can keep dragging from that point. |
| Pan one step | **Mouse wheel** over the scroller (or a horizontal scroll). |
| Pan a beat (4 steps) | **Shift + mouse wheel**, or a horizontal scroll, over the grid. |
| Follow playback | Automatic while the sequencer runs: when the selected track's playhead leaves the view, the view turns to the 16-step block that contains it (steps 1–16, 17–32, …), so you always see what's playing. It waits while you are dragging a note, the ruler or the scroller. To pan freely while playing, turn off **Follow playhead** in the right-click menu. |

The scroller always covers all 64 steps, so you can pan past the end of a short track and set a longer length from the ruler.

### How track lengths play together (polymeter)

Each track **loops over its own length**:
- A 16-step track and a 12-step track drift against each other and line up again every 48 steps.
- A 5-step track against a 16-step track gives a constantly shifting accent pattern.
- **Every track restarts at step 1 when the pattern starts again**, whether it loops or a new pattern begins. The pattern length (below) sets when that happens.

---

## 7. Tracks: select, mute and outputs

Each of the 8 track rows on the right has:

| Control | Use |
|---|---|
| **Number** | Track number, in the track's colour. |
| **TRACK** button | Selects the track for editing in the grid. The selected track's button lights brightly; the others glow faintly in their colours. |
| **MUTE** button | Toggles mute (red light). A muted track sends no gates, but its CV still follows its notes. Its notes are drawn faded in the grid. |
| **GATE** output | 10 V while a note sounds, 0 V otherwise. |
| **CV** output | Pitch at **1 V/octave**, with C4 = 0 V. The CV **holds the last note's pitch** between notes, so releases and glides sound right. |
| **MOD** output | The playing note's **MOD value**, scaled to the track's MOD range. Like CV, it holds the last note's value between notes. Use it for velocity, filter cutoff, accents or anything else that should change per note. |
| Jack rings | The rings round the GATE and CV jacks light up in the track's colour while the gate is high. |

### Poly tracks

Any track can be a **poly track** that plays several notes at once, for chords. It still takes up only its own row and jacks, so the other tracks stay free for mono or poly parts of their own.

**Setting one up:** select the track, then right-click → **Poly voices (notes at once)** → 2 to 8. `x3` (for 3 voices) appears under the track number. Choose **1 (mono)** to make it an ordinary track again.

**Editing:**
- **Notes can stack**: draw a note on a step that already has one and it becomes a chord. Each note goes on the first **voice** that is free for the note's whole length.
- **A note never trims or replaces another** on a poly track. Where every voice is already busy, the note can't be drawn. A note being stretched stops at the next note on its voice; a note being moved stays at the last place it fitted.
- Moving a note may move it onto a different voice, if its own voice is busy at the new place.
- **Moving a whole chord up or down:** press a note's grip (its right end) and drag up or down. Every note that starts on the same step moves with it, keeping its interval, until the highest or lowest note reaches the edge of the grid. Dragging sideways at the same time still changes the grabbed note's length. Ctrl+Z undoes it as one "move chord".
- Selecting, muting, track length, transpose, rotate, and TRACK copy / paste / clear work as on any track, on all its voices.

**Outputs:** the track's **GATE, CV and MOD jacks carry a polyphonic cable** with one channel per voice (channel 1 = voice 1). Patch them straight into polyphonic modules such as VCV's VCO, ADSR and VCA. The jack rings flash while any voice's gate is high.

**Feeding several mono modules:** turn on right-click → **Also send each voice to its own mono jacks**. Voice 2 then also comes out, in mono, on the next track's GATE / CV / MOD jacks, voice 3 on the track after that, and so on (the item shows which tracks). The poly track's own jacks keep sending the poly cable; a mono input reads its first channel, which is voice 1. So a 3-voice poly track on track 2 can play three separate mono synths from the jacks of tracks 2, 3 and 4.
- A bar in the poly track's colour beside the track numbers joins it to the tracks whose jacks it uses. Their jack rings light in the poly track's colour with that voice's gate. (While the bar is shown, the `x3` voice count under the poly track's number is left out.)
- **Their own notes don't play** while their jacks are used (they are kept, and play again when the option is turned off or the voice count is lowered). Their TRACK and MUTE buttons select and mute the poly track.
- Voices that would go past track 8 stay on the poly cable only. A track whose jacks are already in use can't send out its own voices.

**MOD:** in the MOD view in NOTE mode, each step's column is split into one narrow lane per voice: press in a voice's lane to set that voice's notes; a drag stays in the lane where it started. STEP mode works as on any track (all voices share the step values).

**Changing the voice count keeps notes:** lowering it hides the notes on the voices it drops (they don't play), and raising it again brings them back.

### MOD values

Each track's MOD output works in one of two **MOD modes**:

- **NOTE** (the default): every note has a **MOD value** from 0% to 100%. The MOD output sends it while the note plays and holds it until the next note. Good for velocity and accents.
- **STEP**: every **step** of the track has its own value, whether or not a note is on it. The MOD output **glides in a straight line from each step's value to the next** over the length of the step, so it flows continuously through the pattern, regardless of notes, gates or gate length. At the end of a pattern it glides into step 1 of the next one. Good for filter sweeps and other modulation. To have it jump to each step's value instead, turn off right-click → **Per-step MOD glides from step to step** (per track).

Switch the selected track's mode with the **NOTE / STEP badge** at the top of the MOD view's left column (click it), or right-click → **MOD mode**. Each mode keeps its own values, so switching back and forth loses nothing.

| To… | Do this |
|---|---|
| **See and edit MOD values** | Press the **MOD** button (in the strip between UP and DN, right of the grid). Its light turns on and the grid shows the MOD view. Press it again to go back to the notes. |
| **Set a value** | In the MOD view, press in the note's column (NOTE mode) or any step's column (STEP mode) at the height you want. Drag up or down to adjust. |
| **Draw values across several notes or steps** | Press and drag sideways, like drawing a curve. Every note or step you pass is set to the height of the mouse. |
| **Fine-tune one value** | Mouse wheel over its column: 1% per notch. The value and voltage are shown above the bar under the mouse. |
| **Switch NOTE / STEP mode** | Click the badge at the top of the left column, or right-click → **MOD mode**. |
| **Choose the output range** | Right-click the module → **MOD output range**: 0 to 10 V (default), 0 to 5 V, or -5 to +5 V. Each track has its own range. |
| **Set every note on the track to one value** | Set one note, then right-click → **Set every note's MOD value to the last one set**. |

In NOTE mode, each note of the selected track is a bar in the track's colour, as high as its value, with a line along the rest of the note. In STEP mode, every step up to the track's end is a bar, and the track's notes are marked as light strips along the bottom so you can line the values up with them. The left column shows the output voltage at 0%, 25%, 50%, 75% and 100% for the track's range. Shift+wheel pans as in the note view.

- **New notes** get the last note MOD value you set on that track (100% at first). Step values start at 0%.
- Step values loop with the track: a 12-step track uses the values of steps 1–12.
- Notes keep their MOD value when you move or stretch them, and it is copied, pasted, rotated and undone along with them.
- In the note view, a thin dark strip along the bottom of each note of the selected track shows its MOD value: the longer the strip, the higher the value (NOTE mode only).
- TRACK and PATTERN **CLR** also reset the step values to 0%.

---

## 8. Key: root and scale

Both are value displays: click `<` / `>` or use the mouse wheel to step through them ([§3](#using-the-value-displays-root-scale-seq)).

| Display | Range | Use |
|---|---|---|
| **ROOT** | C, C#, D … B | The root note of the scale. The grid's row labels update to match. |
| **SCALE** | 14 scales | Major, Minor, Harm. Minor, Mel. Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Major Pent., Minor Pent., Blues, Whole Tone, Chromatic. |

ROOT and SCALE apply to **all tracks and all patterns** at once.

**Changing the key re-voices everything live.** Notes are stored as scale degrees:
- **Changing ROOT** transposes everything.
- **Changing SCALE** changes the mode while the music keeps playing. For example, Major to Dorian keeps each note on the same degree number.
- **Scales with a different number of notes** (pentatonic has 5 per octave, chromatic has 12) remap degrees. A melody can then shift by more than a small interval. This is expected and can be used creatively.

The grid covers **8 octaves** of the chosen scale. Use **Chromatic** if you want every semitone available as a row.

---

## 9. Patterns

There are **64 patterns**. The **SEQ** display (01–64) chooses which pattern the grid shows and edits. Step through patterns with its `<` / `>` arrows or the mouse wheel, or right-click it and type a number to jump straight to a pattern.

- **In pattern mode** (MODE light off), SEQ also chooses the pattern that **plays**. When you change SEQ while running, the grid switches immediately. Playback switches when the current pattern ends, or when a PAT IN trigger arrives if PAT IN is patched. Until then the grid shows no playhead, because it isn't showing the pattern that is playing.
- **In song mode**, the song decides what plays. See [§12](#12-song-mode) for how SEQ and the grid behave there.

### Pattern length

A pattern plays for as many steps as its **longest track that contains notes**:
- Empty tracks are ignored, so their length doesn't matter.
- An entirely empty pattern counts as 16 steps.
- For example, tracks of 16, 12 and 64 steps where the 64-step track has notes give a pattern length of 64.
- To make a 12-step pattern, set the tracks you've written in to 12 steps.

When a pattern ends, the sequencer **moves on**: it loops the same pattern (pattern mode) or advances to the next song slot (song mode). It also sends a trigger from **EOP**. With PAT IN patched, the pattern changes only when a trigger arrives there ([§15](#15-synchronizing-several-sequencers)).

---

## 10. Gate length

The **GATE** display sets **where each note releases within its last step**, from 5% to 100% in 5% steps. The default is 50%. It works like the other value displays, but stops at 5% and 100% instead of wrapping round.

- **1-step notes**: the gate is high for that fraction of the step.
- **Longer notes**: the gate stays high through every step except the last, then releases at the GATE point within the last step. A 4-step note at 50% sounds for 3½ steps.
- **100% (legato)**: the gate stays high into the next note. Notes that follow each other directly are **tied** on every track: the gate doesn't drop between them, so envelopes don't retrigger.
- **Tied notes** (Shift+click a note): the gate stays high to the very end of the last step. When the next note starts straight after it, the gate never drops, so the envelope doesn't retrigger and the pitch just changes. Use ties for slides and legato lines. A tied note followed by a rest still ends at its last step.

Gate length follows the clock: with an external clock it uses the measured time between clock pulses.

---

## 11. Clock and transport

### Tempo and internal clock

**TEMPO** (30–300 BPM, default 120) sets the internal clock. The display beside the knob shows the current tempo; it turns amber when the tempo is measured from CLK IN. Tempo is in quarter notes and each step is a 16th note, so the sequencer plays 4 steps per beat.

### External clock (CLK IN)

When **CLK IN** is patched, the internal clock is ignored and the sequencer moves **one step per rising edge** (above about 2 V). The BPM display beside the TEMPO knob turns amber and shows the measured tempo. RUN still applies: clock pulses are ignored while stopped.

### RUN

- The **RUN** button (green light) starts and stops the sequencer. A newly added sequencer starts stopped; a saved patch reopens in the run state it was saved in.
- **RUN IN** does the same from a trigger: each trigger toggles run. You can switch it to **gate mode** in the right-click menu, where high = run and low = stop.
- **Stopping** silences any note that's still holding. Starting again continues from where it stopped.
- **In pattern mode, starting always plays the SEQ pattern** (the one shown in the grid). If a different pattern was playing when you stopped, for example an earlier SEQ pattern or a song, playback starts from step 1 of the SEQ pattern instead of carrying on with the old one.

### RESET

- The **RESET** button or a trigger at **RST IN** sends the sequencer back to the start: step 1 of the pattern, and slot 1 of the song in song mode.
- **With the internal clock running**, step 1 plays immediately.
- **With an external clock**, step 1 plays on the next clock pulse. A clock pulse that arrives up to 1 ms *before* the reset also counts as step 1. This keeps sequencers in step even when one module's reset arrives a sample late.
- RESET does not start or stop the sequencer.

---

## 12. Song mode

Song mode plays a list of patterns in order, so you can arrange a whole piece from your 64 patterns.

### MODE button

**MODE** (first button in the SONG group, below the song strip) cycles through three settings each time you press it:

| Light | Mode | Behaviour |
|---|---|---|
| Off | **Pattern** | Loops the SEQ pattern. |
| Yellow | **Song (loop)** | Plays the song from slot to slot and starts over at the end. |
| Orange | **Song (play once)** | Plays the song once, then **stops** and returns to the start. Press RUN to play it again. RUN OUT sends its stop trigger, so synced sequencers stop too. |

### The song strip

The strip under the grid shows the song's slots, 16 per page. Each slot shows the pattern number it plays. A new module's song is **empty**: the strip shows no slots until you add some (INS or ADD, or right-click an empty box → **Add edit pattern to the end of the song**). While the song is empty, song mode just plays the SEQ pattern, as pattern mode does.

- The **cursor** (yellow outline) marks the slot that the song buttons act on.
- The **playing slot** is green while in song mode.
- Dark empty boxes are beyond the end of the song.
- The `SONG` label on the left shows the strip's page, for example `1/2`.

| To… | Do this |
|---|---|
| **Move a slot** | Press on the slot and drag it left or right. It moves through the song as you drag, the other slots shift to make room, and the cursor goes with it. Hold it past the right or left end of the strip to keep moving it a slot at a time onto the next or previous page. Ctrl+Z undoes the move. If the song is playing, the playing slot keeps playing wherever it ends up. |
| Move the cursor and show the slot's pattern | Left-click a slot. The cursor moves there and SEQ turns to that slot's pattern, so the grid shows it straight away (while a song is running, the grid keeps following playback until you stop). |
| Change which pattern a slot plays | Mouse wheel over the slot (patterns 1–64). |
| Move the cursor one slot left or right | Mouse wheel over the `SONG` label. The strip pages automatically. |
| See the next 16 slots | Click the `SONG` label. It wraps back to the first page after the last. |
| Slot actions without the buttons | Right-click a slot: **Set to edit pattern**, **Open this pattern in editor**, **Insert edit pattern before**, **Add edit pattern after**, **Duplicate slot**, **Delete slot**. |

### SONG buttons

| Button | Action |
|---|---|
| **MODE** | Cycles Pattern → Song (loop) → Song (once) (see above). |
| **INS** | Inserts the **SEQ pattern** as a new slot **before** the cursor, and moves the cursor to it. |
| **ADD** | Adds the **SEQ pattern** as a new slot **after** the cursor, and moves the cursor to it. |
| **DUP** | Duplicates the slot at the cursor, inserting the copy after it. |
| **DEL** | Deletes the slot at the cursor. Deleting the last one leaves the song empty. |
| **SET** | Sets the slot at the cursor to the **SEQ pattern**. |
| **EDIT** | Sets **SEQ** to the pattern of the slot at the cursor, so you can edit it. |
| **CLR** | Empties the song (removes every slot). |

A song can have up to **128 slots**, and a pattern can appear in any number of them. All song edits can be undone with **Ctrl+Z**.

### What the grid shows in song mode

- **While a song is running**, the grid **follows playback**: it shows the pattern now playing and switches as the song moves on. The info bar shows the playing pattern (e.g. `P05`) in green. Edits you make go to the pattern that is playing, so you can change it live.
- **When you stop a song**, SEQ turns to the pattern that was playing, so the grid keeps showing it.
- **While stopped**, the grid shows the **SEQ pattern**, so you can browse and edit any pattern. Click a slot (or use **EDIT**) to jump SEQ to that slot's pattern.

### Building a song

1. Write your patterns using SEQ to choose each one.
2. If the song isn't empty yet, press **CLR** (in the SONG group) to start afresh.
3. Step SEQ to each pattern you want, in order, and press **ADD**. Each press adds a slot after the cursor. (Use **INS** to slip a pattern in before the cursor instead.)
4. Use **DUP** to repeat a slot. Use the mouse wheel over a slot to change its pattern, and **DEL** to remove one.
5. Press **MODE** until the light is yellow (loop) or orange (play once), then **RESET** and **RUN**.

### Naming, saving and loading songs

Right-click the module; the **Song** section of the menu has:

| Item | Use |
|---|---|
| **Song name** (text box) | Type a name for the song (you can also click the name in the info bar). It shows at the left of the info bar and is saved with the patch. An unnamed song shows as `Untitled`. |
| **Load song...** | Opens a saved song, replacing everything in this sequencer: all 64 patterns, the song, the track settings and the panel controls (tempo, scale, gate length, ...). Ctrl+Z undoes it. |
| **Save song...** | Saves everything in this sequencer to a file, named after the song (an unnamed song takes the file's name). |
| **Import MIDI file...** | Builds a song from a standard MIDI file (.mid); see below. |

Songs are saved as Rack module presets (`.vcvm`) in the module's user preset folder, so they also appear in the module's **Preset** menu, and any THE GRID preset can be loaded as a song.

### Importing MIDI files

**Import MIDI file...** replaces everything in the sequencer with the file's notes:

- **Parts:** each MIDI track (split by MIDI channel) becomes a sequencer track, in file order, up to 8. A part with overlapping notes (chords) becomes a [poly track](#poly-tracks) with as many voices as it needs, up to 8.
- **Timing:** notes snap to the nearest 16th-note step. The tempo is set from the file's first tempo (30–300 BPM).
- **Pitch:** the scale becomes **Chromatic** with root **C**, so every note fits exactly; MIDI note 60 (C4) is 0 V. Notes outside the grid's 8 octaves (MIDI 12–107) are left out.
- **Velocity** becomes each note's MOD value (127 = 100%).
- **Patterns and song:** the piece is cut into 4-bar (64-step) patterns, starting at pattern 1; identical sections share one pattern. The song plays them in order, MODE is set to song (loop), and the sequencer is left stopped. A note that runs on into the next pattern is split there and tied, so it keeps sounding.
- The song is named after the file.

If something doesn't fit — more than 8 parts, more than 8 notes at once in a part, notes out of range, or a piece needing more than 64 different patterns or 128 song slots — the import goes ahead and a message lists what was left out. **Ctrl+Z** undoes the whole import.

---

## 13. Copy, paste and clear

The **PATTERN** and **TRACK** button groups below the song strip work on the **pattern shown in the grid**. The same functions are in the right-click menu.

| Group | Button | Action |
|---|---|---|
| PATTERN | **COPY** | Copies all 8 tracks of the pattern: notes, note lengths, ties, MOD values and track lengths. |
| PATTERN | **PASTE** | Replaces the pattern with the copied one. |
| PATTERN | **CLR** | Removes every note in the pattern. Track lengths are kept. |
| TRACK | **COPY** | Copies the selected track: its notes, note lengths, ties, MOD values and track length. |
| TRACK | **PASTE** | Replaces the selected track with the copied track. |
| TRACK | **CLR** | Removes all notes from the selected track. Its length is kept. |

**Tips:**
- To **duplicate a pattern**: COPY, step SEQ to an empty pattern, then PASTE.
- To **copy a track to another track**: TRACK COPY, select the other track, then TRACK PASTE. This works across patterns too.
- **The clipboard is shared between all THE GRID modules** in the same Rack session. You can copy from one instance and paste into another.
- PASTE does nothing until something has been copied.
- PASTE and CLR can be undone with **Ctrl+Z**.

---

## 14. Inputs and outputs reference

### Inputs (left column)

| Jack | Signal | Function |
|---|---|---|
| **RUN IN** | Trigger (or gate) | Toggles run on each trigger. In gate mode (right-click menu), high = run, low = stop. |
| **RST IN** | Trigger | Reset to step 1 / song slot 1 ([§11](#11-clock-and-transport)). |
| **CLK IN** | Clock | External clock, one step per rising edge. Overrides TEMPO while patched. |
| **PAT IN** | Trigger | Next pattern. **Unpatched**: patterns move on when they end. **Patched**: the current pattern loops until a trigger arrives; the next step then starts the next pattern from step 1 ([§15](#15-synchronizing-several-sequencers)). |

Inputs respond to signals rising above about 1 V (CLK IN: about 2 V).

### Outputs (left column)

| Jack | Signal | Function |
|---|---|---|
| **RUN** | Trigger (or gate) | A 1 ms trigger every time the sequencer starts or stops. In gate mode (right-click menu), 10 V while running. |
| **RST** | Trigger | A 1 ms trigger every time the sequencer is reset, by button or RST IN. |
| **CLK** | Clock | One pulse per step (50% duty) while running. |
| **EOP** | Trigger | **End of pattern**: a 1 ms trigger each time the sequencer moves on to the next pattern. This happens when the pattern ends (longest track with notes) or when PAT IN tells it to. It fires in pattern mode and in song mode, including for the last pattern of a play-once song. |

### Outputs (track section)

| Jack | Signal |
|---|---|
| **GATE 1–8** | 10 V gate while a note sounds. |
| **CV 1–8** | Pitch, 1 V/octave, C4 = 0 V. Holds the last pitch between notes. |
| **MOD 1–8** | In NOTE mode, the playing note's MOD value, held between notes; in STEP mode, the value of the step being played. Scaled to the track's MOD range (0 to 10 V by default). |

---

## 15. Synchronizing several sequencers

You can run two or more THE GRID modules in lockstep, for example to get 16 or 24 tracks or to split drums and melody across modules. One module acts as the **leader** and the others follow it.

### Clock, transport and reset

Patch from the leader to each follower:

| Leader output | → | Follower input |
|---|---|---|
| **CLK** | → | **CLK IN** |
| **RUN** | → | **RUN IN** |
| **RST** | → | **RST IN** |

The follower now steps, starts, stops and resets together with the leader. Its patterns still change on their own, according to the follower's own pattern lengths and song.

**Matching run states:** RUN sends a toggle trigger, so both modules must be in the **same run state** (both running or both stopped) when you connect them. Otherwise they will keep running in opposite states. To avoid this entirely, open each module's right-click menu and turn on:
- on the leader: **RUN output: gate while running**
- on the follower: **RUN IN: gate, high = run**

The follower then simply runs whenever the leader runs.

### Full sync of pattern changes

Also patch:

| Leader output | → | Follower input |
|---|---|---|
| **EOP** | → | **PAT IN** |

The follower now changes pattern (moves to its next song slot) **exactly when the leader does**:
- **Shorter follower patterns** loop until the leader's pattern ends.
- **Longer follower patterns** are cut short when the leader moves on.
- A PAT IN trigger that arrives with the clock, or up to 1 ms after it, counts as the first step of the next pattern.
- In **pattern mode**, a patched PAT IN means the follower picks up a new SEQ value only when the leader's pattern ends.

### Chains of three or more

Patch each follower's **EOP → PAT IN** of the next module, and pass CLK, RUN and RST along the chain or from the leader. A follower's EOP fires when it moves on, which (with PAT IN patched) is exactly when the leader does. The whole chain therefore stays aligned.

### Keeping sequencers independent

Synchronization is entirely optional:
- **Leave PAT IN unpatched** and the follower plays its own patterns and song, while still sharing the clock and transport.
- **Leave everything unpatched** and each sequencer runs completely independently on its own clock.

---

## 16. Right-click menu reference

Right-click the module panel (not the grid's squares) to open the menu.

| Section | Item | Action |
|---|---|---|
| Pattern | Copy pattern / Paste pattern / Clear pattern | Same as the PATTERN buttons ([§13](#13-copy-paste-and-clear)). |
| Track N | Poly voices (notes at once) | 1 (mono) to 8. More than 1 makes it a poly track that can play chords; see [Poly tracks](#poly-tracks). |
| Track N | Also send each voice to its own mono jacks | Poly tracks only: voice 2 also comes out on the next track's jacks, voice 3 on the one after, and so on, for separate mono modules. Those tracks' own notes don't play meanwhile. |
| Track N | Copy track / Paste track / Clear track | Same as the TRACK buttons. On a poly track they act on every voice. |
| Track N | Set every track in this pattern to N steps | Copies the selected track's length to all 8 tracks of the pattern. |
| Track N | Transpose track up / down one degree | Moves every note of the selected track one scale step up or down (same as Alt+wheel over the grid). Does nothing if a note would leave the grid. |
| Track N | Rotate track one step later / earlier | Shifts every note of the selected track one step, wrapping at the track's end (same as Alt+Shift+wheel over the grid). |
| Track N | MOD mode | **Per note** (NOTE) or **Per step** (STEP); see [MOD values](#mod-values). |
| Track N | Per-step MOD glides from step to step | On (default): in STEP mode, MOD slides smoothly from one step's value to the next. Off: it jumps to each step's value as the step starts. |
| Track N | MOD output range | The voltage range of the track's MOD output: 0 to 10 V, 0 to 5 V or -5 to +5 V. |
| Track N | Set every note's MOD value to the last one set | Gives every note on the selected track the MOD value you set most recently on it (shown on the right of the item). |
| Song | Song name | Text box: the song's name, shown in the info bar. |
| Song | Load song... / Save song... | Load or save everything in the sequencer as a song file; see [Naming, saving and loading songs](#naming-saving-and-loading-songs). |
| Song | Import MIDI file... | Builds a song from a .mid file; see [Importing MIDI files](#importing-midi-files). |
| Song | Clear song | Same as the SONG **CLR** button. |
| Sync | RUN output: gate while running | RUN outputs a gate instead of start/stop triggers. |
| Sync | RUN IN: gate, high = run | RUN IN reads a gate instead of toggling on triggers. |
| View | Follow playhead (show the steps that are playing) | While playing, when the selected track's playhead leaves the view, the grid turns to the 16-step block containing it. On by default; turn it off to pan freely while playing. |
| View | Reset view to middle octave | Scrolls the grid back to the octave around C4 (0 V). |

Rack's standard items are also available. **Initialize** clears all notes, patterns, track lengths and the song, and resets the knobs. **Randomize** randomizes the knobs only, not your notes.

The song strip has its own right-click menu for individual slots ([§12](#12-song-mode)).

---

## 17. What is saved with the patch

Everything is stored in the patch file and in Rack's autosave:
- All 64 patterns: notes, note lengths, ties, MOD values and track lengths.
- Each track's voice count (poly tracks) and whether it sends its voices to their own mono jacks.
- Each track's MOD mode, MOD output range and the MOD value new notes get, and every pattern's step MOD values.
- The song, its name and its cursor.
- Track mutes and the selected track.
- Run state.
- Grid scroll position (pitch and steps).
- Follow-playhead and RUN gate/trigger options.
- All knob and button settings.

Patches made with earlier versions of the module load correctly: notes become 1 step long and tracks 16 steps long where that information didn't exist yet.

---

## 18. Workflow recipes

**A bass line with long notes**
Select a track, press on step 1 at the root row and drag to step 4. That draws a note lasting one beat. Add more notes the same way.

**A tied, sliding lead**
Draw notes that touch each other end to end, then Shift+click each one except the last to tie it. The gate stays high across all of them, so the envelope plays once while the pitch moves. Add a slew limiter on the CV for real portamento.

**Shifting a groove**
Write a drum or bass track, then hold Alt+Shift and roll the wheel over the grid to slide it a step at a time against the other tracks. Ctrl+Z puts it back.

**Polyrhythms**
Track 1: a 16-step kick pattern. Track 2: set its length to 12 in the ruler. Track 3: length 5. All three loop independently and line up again at the start of each pattern.

**A 64-step melody over a 16-step groove**
Set the melody track to 64 steps, panning across it with the scroller under the grid. Leave the drum tracks at 16 steps; they repeat four times under the melody. With the melody track selected, the grid turns page by page as it plays through.

**Odd-length pattern (e.g. 7/8)**
Set every track you use to 14 steps: set one in the ruler, then use **Set every track in this pattern to 14 steps**.

**Variations of a pattern**
Write pattern 1. PATTERN **COPY**, step SEQ to 2, **PASTE**, then change a few notes. Arrange 1-1-1-2 in the song.

**Live key changes**
While the song plays, step ROOT for a key change or SCALE for a change of mode. Everything follows instantly.

**Doubling up voices across two modules**
Sync a second sequencer with all four cables ([§15](#15-synchronizing-several-sequencers)). Give it the same song layout and use its tracks for harmony or drums.

---

## 19. Troubleshooting

| Symptom | Check |
|---|---|
| No sound from a track | Is the sequencer running (green RUN light)? Is the track muted (red light)? Are the notes inside the track's length (not dimmed)? Are CV and GATE patched? |
| Can't add notes in some columns | Those steps are past the track's end (darkened). Lengthen the track in the ruler. |
| Notes disappeared after changing the track length | They're kept. Lengthen the track again and they return. |
| Changing SEQ doesn't change what plays | In pattern mode the change happens at the end of the current pattern, or when PAT IN fires if it is patched. In song mode the song decides what plays. |
| Edits go to a different pattern than SEQ shows | In song mode while running, the grid follows the playing pattern (green `Pnn` in the info bar). Stop the transport to edit the SEQ pattern. |
| Pattern is longer than expected | The pattern lasts as long as its longest track that has notes. Check each track's length (`TRK n:len`). |
| Follower sequencer runs when the leader stops | Their run states got out of step. Stop or start one by hand, or switch both to RUN gate mode in the right-click menu. |
| Follower never changes pattern | PAT IN is patched but receives no triggers. Patch the leader's EOP to it, or unpatch PAT IN. |
| Melody jumped strangely after changing scale | Scales with a different number of notes per octave remap scale degrees. Switch back, or use scales with the same number of notes. |
| Grid shows the wrong octave | Scroll with the mouse wheel or UP/DN, or use **Reset view to middle octave**. |
| `EXT` tempo looks wrong | The display assumes one clock pulse per 16th note. A clock at another division plays correctly but shows a different BPM. |

---

## 20. Specifications

| Item | Value |
|---|---|
| Width | 46HP |
| Tracks | 8, each mono or poly (up to 8 notes at once, as a poly cable) |
| Patterns | 64 |
| Track length | 1–64 steps, per track per pattern (default 16) |
| Note length | 1 step up to the track's length |
| Song length | 1–128 slots |
| Pitch range | 8 octaves of the chosen scale; C4 = 0 V |
| Scales | 14, with 12 root notes |
| Tempo | 30–300 BPM (16th-note steps), or external clock |
| Gate length | 5–100% of the note's last step in 5% steps (default 50%; 100% = legato), or all of it for a tied note |
| Gate / trigger level | 10 V; triggers are 1 ms |
| CV | 1 V/octave, clamped to ±10 V |
| MOD | 0–100% per note or per step (1% steps); output range 0 to 10 V, 0 to 5 V or -5 to +5 V per track |
