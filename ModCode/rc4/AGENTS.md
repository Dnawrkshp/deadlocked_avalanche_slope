# AGENTS.md

## Project Overview
This repository contains a **mod for a PlayStation 2 game** written in **C99**. The mod makes extensive use of the **`libdl`** library to call functions that are built into the original game binary, such as collision detection, math utilities, game state, moby handling, networking, UI, and other engine services.

> **Important:** Only use functions, structs, enums, constants, and macros that actually exist in `libdl`. Do **not** invent or hallucinate APIs that are not provided by the library.

### libdl Include Path
```
M:/PS2/vscode-include/libdl/
```

## Coding Guidelines
* Target language: **C99**.
* All game-engine interactions should be performed through the **`libdl`** API.
* The third axis (**Z**) is the **vertical** axis in-game.
* When using vector, matrix, quaternion, math, collision, moby, player, UI, or other engine helpers, search the relevant `libdl` header file for the exact declaration before calling it.
* Prefer including the narrow subsystem header that owns the API you need. Use `dl.h` only when you need the broader libdl entry points.
* Agents cannot build or test this project locally. The user must manually build the mod and may pass compiler/build errors back to the agent for follow-up fixes.

## libdl Header Outline

| Header | Purpose / When to use it |
|--------|---------------------------|
| `area.h` | Area metadata and lookup helpers. Use for querying map area definitions/counts. |
| `camera.h` | Game camera structures and accessors. Use when reading camera state, FOV, camera transforms, camera widgets, or camera blend data. |
| `cheats.h` | Built-in cheat toggles and visual/gameplay cheat settings. Use for applying no-packs/no-V2s, mirror world, colorblind mode, weather, and similar cheat state. |
| `collision.h` | Collision flags, surface IDs, line/sphere casts, and hit result accessors. Use for ray tests, moving sphere tests, moby collision checks, and surface walkability/lethality checks. |
| `color.h` | Basic color utility helpers. Use for lerping, scaling, max channel lookup, or channel assignment on packed colors. |
| `common.h` | Shared libdl macros and inline annotations. Usually included indirectly; use when you need common libdl definitions. |
| `compression.h` | WAD decompression helper. Use when decompressing packed WAD data. |
| `dialog.h` | Dialog IDs and dialog sound playback. Use for triggering built-in dialog/audio cues. |
| `dl.h` | Top-level libdl update hooks. Use for `dlPreUpdate`/`dlPostUpdate` style integration points. |
| `dme.h` | DME packet type IDs. Use when working with DME/network packet identifiers. |
| `game.h` | Core game state, scoreboard, winner/end-game controls, time, host/client IDs, world IDs, and voice channel helpers. Use for match flow and global game data. |
| `gameplay.h` | Gameplay file/map placement structures. Use for reading gameplay moby, pvar, cuboid, and related map definition data. |
| `gamesettings.h` | Game rules, options, client state/team/skin/name helpers, and mode names. Use for match settings and per-client setup. |
| `gid.h` | GID-related definitions. Use when a struct or API references game object identifiers and you need the canonical libdl type definitions. |
| `graphics.h` | 2D/3D drawing helpers, text rendering, quads, sprites, texture upload, font windows, GIF paging, and scissor/state helpers. Use for custom HUD/debug rendering and low-level graphics work. |
| `guber.h` | Network-synced object/event system types and helpers. Use for guber moby lookup, object UID handling, event allocation/read/write, spawned moby creation, and master object control. |
| `help.h` | Help string IDs and lookup. Use for retrieving built-in help text. |
| `hud.h` | HUD canvas/object structures and helpers. Use for player HUD flags, team colors, HUD canvas/object lookup, popup control, and HUD element position/scale changes. |
| `map.h` | Map IDs, map masks, and map name/mask conversion helpers. Use for checking or converting map identity. |
| `math.h` | Scalar math wrappers/utilities. Use for trig, powers, roots, clamping, interpolation, angle wrapping, and scalar numeric helpers. |
| `math3d.h` | Vector, matrix, and quaternion helpers. Use for 3D transforms, vector math, projections, rotations, matrix construction/inversion/multiplication, and quaternion conversions. Search this header for exact vector/matrix function names and signatures. |
| `mc.h` | Memory card file primitives. Use for MC open/close/seek/read/write/result polling. |
| `moby.h` | Moby structures, IDs, state, movement, animation, damage, targeting, flags, joints, effects, and spawn/destroy helpers. Use for nearly all object/entity interaction. |
| `music.h` | Music track IDs and playback controls. Use for starting, stopping, pausing, transitioning, and querying music tracks. |
| `net.h` | Custom message handlers, Medius/custom app messages, network aggregation, network error checks, DME/lobby connections, and low-level inet helpers. Use for networking and message transport. |
| `pad.h` | Controller input structures and helpers. Use for raw/mapped button state, button down/up edges, input reset/disable/enable, and pad lookup. |
| `patch.h` | Patch support helpers. Use for SIF RPC patching. |
| `player.h` | Player, hero, camera, gadget, pad, health, weapon, team, spawn, and state helpers. Use for player lookup/control, inventory, damage/health, respawn, position/rotation, local input, and connection checks. |
| `radar.h` | Radar blip structures and helpers. Use for radar blip lookup, indexing, and color queries. |
| `random.h` | Random scalar/vector generation and seeding. Use for random ranges, radians, vectors, and deterministic seed setup. |
| `sha1.h` | SHA-1 hashing helper. Use when a SHA-1 digest is needed. |
| `sound.h` | Sound definitions, instances, handles, playback, stopping, and global sound data. Use for non-dialog sound effects. |
| `spawnpoint.h` | Spawn point and hill point structures/helpers. Use for spawn/hill counts, index management, spawn point lookup/mutation, and point-inside-spawn checks. |
| `spline.h` | Spline structures and lookup. Use for retrieving 3D spline definitions from map/gameplay data. |
| `stdio.h` | Standard formatted I/O wrappers. Use for `sprintf`, `snprintf`, `printf`, and `sscanf` where libdl-provided declarations are required. |
| `stdlib.h` | Basic allocation wrappers. Use for `malloc` and `free`. |
| `string.h` | Memory/string helpers. Use for `memset`, `memcpy`, `memmove`, comparisons, copies, concatenation, substring search, and `safe_strcpy`. |
| `team.h` | Team ID definitions. Use when code needs canonical team constants. |
| `time.h` | System timer helper. Use for retrieving system time. |
| `ui.h` | Menu IDs, UI elements, dialogs, popups, timers, UI sounds, menu pointers, callbacks, and UI number formatting. Use for interacting with built-in menus and screen UI prompts. |
| `utils.h` | Address range helpers. Use to check whether pointers are in EE, scratchpad, or IOP memory. |
| `vehicle.h` | Vehicle base/interface structures and rider helpers. Use for vehicle state, dead-reckoning data, adding/removing players, and vehicle-specific behavior. |
| `weapon.h` | Weapon/gadget IDs, slots, mod IDs, definitions, damage helpers, holoshield controls, and weapon moby visual updates. Use for weapon lookup/configuration and gadget behavior. |

---

*End of file.*
