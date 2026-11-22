# PocketMod — Master Development Specification & Multi-Agent Protocol

Project Status: Phase 0 (Architecture & GUI Prototype Complete) | Target Platform: Android | Last Updated: November 22, 2026

> Imported from the Google Doc "PocketMod - Master Dev Spec & Agent Protocol" on 2026-11-22.
> Content is unchanged; only the formatting was converted to Markdown.

## 1. Project Vision & Core Objective

PocketMod is a mobile-first Android groovebox engineered specifically for rapid, intuitive music sketching when away from primary instruments or desktop environments. Designed for mobile ergonomics, it strikes a deliberate balance between tactile immediacy and sound design depth.

### Core Capabilities

- **MIDI-Based Songwriting:** A flexible pattern and scene sequencing framework optimized for fast composition via touchscreen.
- **WAV Drum Sampler:** A low-latency, 8-to-16-pad drum instrument backed by RAM-cached uncompressed PCM samples.
- **Block-Based Modular Synth:** A hybrid subtractive and semi-modular synth architecture offering powerful routing without virtual cable clutter on small screens.
- **Integrated FX Engine:** Per-track insert effects complemented by global send buses (delay/reverb) and master bus safety limiting.
- **DAW Handoff & Export:** Frictionless offline exporting to Standard MIDI Files (.mid) and audio stems (.wav master and multi-track stems) for seamlessly finalizing projects in any desktop Digital Audio Workstation.

## 2. Multi-Agent Workspace & Communication Protocol

All multi-agent and human collaborations occur within the shared **"PocketMod - Multi-Agent Dev Hub"** directory. The structured environment ensures seamless handoffs between architectural design, audio engine C++ implementation, and UI development.

### Directory Structure

| Directory / File | Description & Contents |
| :-- | :-- |
| PocketMod - Multi-Agent Dev Hub | Root folder. Contains this **Master Dev Spec** and the Agent Communication Dashboard spreadsheet. |
| 01_Audio_Assets_WAV | Curated drum one-shots (.wav), kit manifests (includes uploaded Default_Kit_Manifest_v1.0.json), and reference audio test loops (Format: 48 kHz, 16/24-bit uncompressed PCM). |
| 02_UI_Prototypes | Interactive HTML/Compose prototypes (including uploaded pocketmod_groovebox.html), UI wireframes, design systems, and visual assets. |
| 03_Code_Specs_&_Builds | Module API contracts (JNI/ring-buffer specs), JSON preset schemas (including uploaded ProjectSerializer_Schema_v1.0.json), C++/Kotlin interface definitions, and APK test builds. |

### Agent Operating Rules

1. **Check In First:** Before starting any task, read the Agent Communication Dashboard (Task Board, Agent Comms Log, and Decision Log) to confirm system status and check for open handoffs.
2. **Single-Owner Task Locking:** Set a task's Status to **In Progress** and assign your agent ID before modifying any shared artifacts. Two agents must never work on the same task simultaneously.
3. **Structured Handoffs:** When completing a deliverable or identifying a blocker, log a entry in the Agent Comms Log containing: *From Agent*, *To Agent*, *Priority*, *Artifact Reference*, and *Status = "Open"*.
4. **Architecture Immutability:** Never alter or reverse entries in the Decision Log without logging an official RFC proposal in the Agent Comms Log addressed to both the **Human Lead** and **Architect Agent**.
5. **Verification Before Done:** A task cannot be marked **Done** until all associated Acceptance Criteria are met and validated via tests or visual verification.

## 3. Technical Architecture & Core Stack

The system utilizes a strict two-tier architecture designed to maintain real-time audio stability on low-latency Android devices while delivering an interactive UI.

```text
+-------------------------------------------------------------------+
|                        UI & Control Layer                         |
|                 (Kotlin + Jetpack Compose UI)                     |
+-------------------------------------------------------------------+
                                  |
              Lock-Free SPSC Ring Buffers (JNI Bridge)
       [UI -> DSP: Parameter Changes]  [DSP -> UI: Metering & Pos]
                                  v
+-------------------------------------------------------------------+
|                     Real-Time Audio Engine                        |
|             (Native C++20 + Google Oboe Architecture)             |
|                                                                   |
|   +-------------------+  +-------------------+  +---------------+ |
|   |  Sample-Accurate  |  |   Multi-Engine    |  |  Insert/Send  | |
|   |  Sequencer Clock  |->|  DSP (Sampler /   |->|   FX Rack &   | |
|   |  (Frame Counter)  |  |    Modular Synth) |  | Master Limiter| |
|   +-------------------+  +-------------------+  +---------------+ |
+-------------------------------------------------------------------+
```

### UI vs. Native Audio Engine Separation

- **UI Layer:** Developed using **Kotlin** and **Jetpack Compose**. Manages application state, touchscreen input, visual rendering, and file IO.
- **Audio Engine:** Written in high-performance **C++20** leveraging **Google Oboe** (wrapping AAudio where available, falling back to OpenSL ES). The audio callback thread runs at high priority to eliminate JVM Garbage Collection pauses and avoid audio stutter (glitching).

### Threading & Lock-Free JNI Communication

- Communication across the native boundary utilizes **Single-Producer Single-Consumer (SPSC) Lock-Free Ring Buffers**.
- **UI-to-DSP Buffer:** Transmits real-time parameter changes, MIDI triggers, and transport commands without blocking the callback.
- **DSP-to-UI Buffer:** Streams playhead positions, peak metering data, and active voice statuses back to Jetpack Compose for rendering.
- **Callback Constraints:** Zero heap allocations (malloc/new), zero system calls, zero mutex locks, and zero blocking IO are permitted within the C++ audio render callback.

### Sample-Accurate Sequencer Clock

- System timing is driven by counting rendered sample frames inside the high-priority C++ audio callback rather than system wall-clock timers.
- *Example:* At 48 kHz sample rate and 120 BPM, one 16th note corresponds to exactly 6,000 audio frames ((48,000 × 60) / (120 × 4)). Events trigger precisely on the exact frame sample boundary.

### MIDI Data Representation & File Handling

- Internal representation uses lightweight, aligned C++ struct arrays representing MIDI events (frame timestamp offset, MIDI event type, note number, velocity, duration).
- Bidirectional conversion engines handle importing external .mid files into internal memory structures, as well as exporting internal tracks into standard Multi-Track Type 1 .mid files.

## 4. Sound Engines & DSP Specifications

### Drum Sampler (.wav Engine)

- **Sample Loading:** Decodes full audio files into 32-bit floating-point PCM arrays in RAM during project loading via dr_wav. Disk streaming during playback is strictly prohibited.
- **Configuration:** Supports 8 to 16 drum pads mapped to standard MIDI note numbers (C1–D#2).
- **Per-Pad Parameters:** Gain/Volume, Stereo Pan, Pitch/Tune (±24 semitones), Sample Decay time, and Choke Group IDs (e.g., Closed Hi-Hat in Group 1 automatically cuts off an active Open Hi-Hat assigned to Group 1).

### Block-Based Semi-Modular Synthesizer

- **Signal Flow:** Fixed ergonomic chain: Dual Oscillators -> Resonant SVF Filter -> ADSR Envelope Amplifier. This setup eliminates complex virtual patch cables on mobile screens.
- **Oscillator Section:** 8-voice polyphonic engine featuring Sine, Sawtooth, Square (with Pulse Width Modulation), Triangle, and Noise generators. Includes coarse/fine detuning and basic Frequency Modulation (FM) depth controls.
- **Filter Section:** State-Variable Filter (SVF) offering smooth continuous Low-Pass (LP), High-Pass (HP), and Band-Pass (BP) modes with cutoff frequency and resonance control.
- **Tap-to-Modulate Matrix:** Includes 2 assignable LFOs / Modulation Envelopes. Selecting "Assign Mod" and tapping a destination control (Cutoff, Oscillator Pitch, Amp Level) links the source and opens an inline depth control.

### Effects Rack & Master Bus

- **Per-Track Inserts:** 2 to 3 swappable processing slots per track featuring Overdrive/Bitcrusher, Modulation (Chorus/Phaser), and 3-Band Parametric EQ / DJ-Style Tilt Filter.
- **Global Send Buses:** Shared tempo-synced Delay and algorithmic Space Reverb (Freeverb / Feedback Delay Network architecture) for efficient CPU utilization across tracks.
- **Master Output Protection:** A mandatory peak-limiting / soft-clipping stage at the final output node prevents digital clipping and speaker damage caused by high synth resonance spikes.

## 5. Mobile-First UX & Interactive Prototype Reference

The layout is organized into 4 primary functional views, as implemented in the pocketmod_groovebox.html functional reference prototype:

```text
+-------------------------------------------------------------------+
|  [Drums]      [Synth]      [FX]      [Song]    (Top Navigation)   |
+-------------------------------------------------------------------+
|                                                                   |
|                         ACTIVE TAB VIEW                           |
|                                                                   |
|   * Drums : 8-Pad Matrix & 16-Step Grid Sequencer                 |
|   * Synth : Dual OSC, SVF Filter, Mod Matrix & Scale Keybed       |
|   * FX    : Track Inserts, Global Sends & Stereo Output Meters    |
|   * Song  : Scene Launching Grid (A-D) & Mix/Arrangement Controls |
|                                                                   |
+-------------------------------------------------------------------+
|  [Play/Stop]  [BPM: 120]  [Rec]  [Pattern: A1]  (Transport Bar)    |
+-------------------------------------------------------------------+
```

### Interface Views

- **Drums Tab:** Displays an 8-pad velocity-sensitive grid, sample load selectors, per-pad sound parameters (Pitch, Decay, Vol, Choke), and a switchable 16-step grid sequencer view.
- **Synth Tab:** Exposes oscillator controls, SVF filter parameters, ADSR shape sliders, Tap-to-Modulate mapping controls, and a scale-locked pad surface (with customizable Chord Modes) alongside a mini piano-roll view.
- **FX Tab:** Provides access to insert slots for active tracks, return levels for global Delay/Reverb sends, visual parameter dials, and real-time master output meters featuring soft-limiter activity indicators.
- **Song Tab:** Focuses on macro arrangement via 4 Scene Launchers (A: Intro, B: Verse, C: Chorus, D: Bridge), an editable timeline arrangement chain, a multi-channel track mixer, and export options.

## 6. Four-Phase MVP Development Roadmap

| Phase / Milestone | Core Deliverables | Target Artifacts / Output | Verification Criteria |
| :-- | :-- | :-- | :-- |
| **Milestone 1** Audio Core & Drum Grid | Oboe C++ engine implementation, SPSC lock-free JNI ring buffers, sample-accurate clock, dr_wav memory preloading, and 16-step drum UI grid. | AudioEngine_v1.0.so, DrumGridCompose_v1.0 | Measured audio latency < 10 ms on target test devices; zero audio drops or GC glitches during continuous pad triggering. |
| **Milestone 2** Subtractive Synth & MIDI | 8-voice polyphonic dual-oscillator synth engine, scale-locked touch pads, chord engine, mini piano roll UI, and Standard MIDI File (.mid) importer/exporter. | SynthEngine_v1.0.cpp, MidiParser_v1.0.kt | Validated roundtrip .mid file export/import parity against standard desktop DAWs (Ableton/FL Studio). |
| **Milestone 3** Modular Layer & FX | Tap-to-Modulate routing engine, per-track insert FX modules (EQ, Overdrive), global Delay/Reverb send buses, and master bus limiter. | EffectsRack_v1.0.cpp, ModMatrixSpec_v1.0 | Stable CPU consumption under < 35% load on mid-tier test hardware running all effects slots concurrently. |
| **Milestone 4** Arrangement & Polish | Scene sequence chaining, JSON project save/load subsystem, offline master & stem WAV background renderer, and device performance tuning. | ProjectSerializer_v1.0, PocketMod_v1.0_Release.apk | Successful offline export of song stems matching live playback audio bit-for-bit without buffer underflows. |

## 7. Recommended File & Data Conventions

### Project & Preset File Format (JSON Schema)

Projects and instrument presets are saved using structured JSON. Relative paths are used for external sample dependencies to ensure cross-device portability.

```json
{
  "pocketmod_version": "1.0.0",
  "project_name": "Midnight_Drive",
  "tempo": 124.0,
  "scale": "Minor",
  "root_note": "A",
  "tracks": [
    {
      "track_id": 0,
      "track_name": "Drums",
      "type": "SAMPLER",
      "pads": [
        {
          "pad_id": 0,
          "sample_path": "samples/kicks/Tight_909_Kick.wav",
          "pitch_semitones": 0,
          "decay_ms": 350,
          "choke_group": 0
        }
      ]
    },
    {
      "track_id": 1,
      "track_name": "Bass_Synth",
      "type": "SYNTH",
      "polyphony": 1,
      "synth_params": {
        "osc1_wave": "SAW",
        "osc2_wave": "SQUARE",
        "osc_mix": 0.5,
        "cutoff_hz": 1200.0,
        "resonance": 0.4
      },
      "mod_matrix": [
        {
          "source": "LFO_1",
          "destination": "FILTER_CUTOFF",
          "depth": 0.65
        }
      ]
    }
  ]
}
```

### Artifact Naming Convention

All multi-agent deliverables, spec documents, API definitions, and build outputs must adhere to the following naming structure:

```text
[Module]_[ArtifactName]_v[Major].[Minor].[Extension]
```

Examples:

- AudioEngine_RingBufferSpec_v1.0.md
- SynthEngine_VoiceAllocator_v0.8.cpp
- ProjectSerializer_Schema_v1.2.json
- PocketMod_PrototypeBuild_v0.4.apk
