# REO First Person

A first-person camera mod for **Resident Evil Outbreak Recompiled**, the unofficial PC port of Resident Evil Outbreak
(File #1, USA). It puts the camera into your character's eyes: you see the game through her eyes while you walk, run
and aim, and you look around with the right stick.

REO First Person is an original mod, written from scratch on the program's own mod host services. It is a separate mod
from the Modern Camera and shares no code with it.

- Mod id `reo-first-person`, version **1.0.0**.
- 12 settings. The camera window and Configure show them, locked: you can see what the camera does, but not change it.

> Unofficial fan project, not affiliated with or endorsed by Capcom. See [Legal](#legal).

## Which disc it is for

**Resident Evil Outbreak (USA), SLUS-20765, disc version 2.00** - the release Resident Evil Outbreak Recompiled runs.
Every game address the camera uses was measured on this disc. Outbreak File #2 is not supported.

## Requirements

- **Resident Evil Outbreak Recompiled 1.5.1 or newer.**
- **Your own legal copy** of Resident Evil Outbreak (USA), SLUS-20765, disc version 2.00. This mod contains no disc
  image and no game files.

The camera never writes the game's code. To stop the game's own camera it switches one feature compiled into the
prepared game code, `camera.freeze`, and it runs at three marks there (the game frame, the pad mapper, and the room's
draw for the restroom mirrors). These code sites are in this repository (`patches.json`, every original instruction
given only as its hash), and the program compiles them in when it prepares your game, whether the mod is installed or
not, checking every site against your own copy. The mod needs no other mod's files.

## Install

1. Download `reo-first-person-1.0.0.zip` from this repository's **Releases** page (it is also in the
   [`release`](release) folder). Do not unpack it.
2. In Resident Evil Outbreak Recompiled, open **Mods**, choose **Install Mods** and pick the zip.
3. Make sure the mod is switched on in the list, then start the game.

The Mods tab checks the whole archive before it writes anything and installs the mod into its own folder,
`mods\reo-first-person\`. **Updating:** install the new zip the same way. The Mods tab asks before it replaces the
installed version and moves the old one into `mods\.trash\` (nothing is deleted).

## Features

- **In her eyes.** While you play, the camera sits in your character's eyes: a spot in the head, measured for each of
  the eight characters, just in front of the eyes. It follows the head. Movement stays the game's own controls.
  The camera is locked to the eyes of the picture that is drawn: the game draws each picture after it has moved
  her body once more, so the camera follows the head's own motion into that picture (her step, the run's bob, a turn)
  and stays in the eyes while she runs, walks, turns and stops.
- **Aiming.** While you aim, the camera stays in the eyes and is angled to line up with the gun's sights (eye, rear
  sight, front sight), with a small dot in the middle of the screen. Measured sights: the 45 Auto (Kevin's) and the
  Handgun for Mark; any other weapon is aimed along the character's aim. A shot's recoil and the weapon animations
  play as the game makes them; the camera stays in the eyes (following the head) and keeps its direction: no step
  aside.
- **Right stick.** The right stick looks around from her eyes (left / right up to 60 degrees, up 30, down 60) and the
  view eases back to where her eyes face when you let go. While the camera is in her eyes the game sees the right stick
  centred and a press of it (R3) as no ad-lib, so looking around or pressing the stick in says no ad-lib line, and the
  left stick moves her exactly as without the look.
- **Scripted camera shots** (cutscenes, events): the game's camera takes over; the camera returns into the eyes when
  you move on again (or raise your weapon). Movies, doors, the item screen and the map work as without the mod.
- Characters right in front of the camera stay drawn while it drives (the game normally hides characters near its
  camera); switched off, the game's rule is back.
- **Walls and pictures stay.** Each of the game's fixed cameras leaves out what stands between its lens and the room
  (the wall it looks through, the pictures on that wall, a light next to it), and the game still picks its fixed
  camera from where she stands. While the camera is in her eyes those walls and pictures stay drawn (J's Bar's
  restroom: the far wall and its framed poster; the Jack's Bar sign's glowing frame), also when the game changes its
  fixed camera. What every fixed camera that looks at it leaves out stays out (the restroom's vent stays open).
- **J's Bar's restroom mirrors.** The game's mirrors are a mirrored copy of the room's door end built behind the glass
  for its own fixed camera only: from her eyes they showed black glass and two floating posters. While the camera is in
  her eyes there, the mirrors show the room reflected, as the game's own mirror looks from its fixed camera, from every
  angle: what of the room the glass can show is drawn a second time, mirrored about the glass and clipped to it, with
  the game's own functions. Characters are not reflected (the game's mirror never reflected them).
- **The restroom's light bars.** Each bulb's glow card is drawn as the fixed camera shows it: seen from the side the
  fixed camera never sees, its glow no longer covers the bulbs behind it like a flat board.

## The camera window

- Opens with **F8** - in the menus and over the running game -, with the controller's **View** button while the mod is
  switched on, or with **Camera Window** next to Configure on the Mods tab. F8, View or Escape / Back closes it. It
  does not open over exclusive fullscreen (use Borderless or Windowed).
- It is **locked**: it shows the camera's settings with their live values - the header's On / Off, then the
  **Normal View** and **Aiming View** tabs with each view's position, look and field of view - but nothing in it can be
  changed, and it has no Reset to defaults. **Configure** on the Mods tab shows the same settings, locked the same way.

## On and off

- **Mods tab:** the mod's switch.
- **While playing:** **F7** switches it off and on again.

Off means the game's own camera, exactly as without the mod: the game's camera, its rule for hiding characters near
the camera, the walls and pictures of its fixed cameras and the restroom's mirrors are the game's again.

## Conflicts

It is a separate camera from the Modern Camera mod. Both drive the game's camera, so the Mods tab reports a conflict
when both are on: choose one (the one later in the load order stays active).

## Settings

The values the camera uses, as the camera window and Configure show them (locked). The offsets are in game units
(about 1 cm each); with every offset at 0 the camera is at the eye spot (8 units in front of the eyes, measured for
each character).

| Tab | Setting (key) | Value | Range / choices | What it does |
|---|---|---|---|---|
| Header | First Person (`enabled`) | On | On / Off | On: the camera is in your character's eyes. Off: the game's own camera, exactly as without the mod. |
| - | On / Off Key (`switch_view_button`) | F7 | F7 | F7 switches First Person off and on again while playing. |
| Normal View | Forward / Back (`normal_forward`) | 0 | -25 to 25 | While you walk and stand: forward (+) or back (-). |
| Normal View | Up / Down (`normal_up`) | 0 | -25 to 25 | While you walk and stand: up (+) or down (-). |
| Normal View | Left / Right (`normal_right`) | -4 | -25 to 25 | While you walk and stand: to her right (+) or left (-). |
| Normal View | Look Up / Down (`normal_look`) | 0 | -25 to 25 | While you walk and stand: the view turned up (+) or down (-), in degrees. 0 = level ahead. |
| Normal View | Field of View (`normal_fov`) | 63 | 40 to 90 | While you walk and stand: the vertical field of view, in degrees. |
| Aiming View | Forward / Back (`aim_forward`) | 2.5 | -25 to 25 | While you aim: forward (+) or back (-) from the same eye spot. |
| Aiming View | Up / Down (`aim_up`) | 3.5 | -25 to 25 | While you aim: up (+) or down (-). |
| Aiming View | Left / Right (`aim_right`) | -1 | -25 to 25 | While you aim: to her right (+) or left (-). |
| Aiming View | Look Up / Down (`aim_look`) | 0 | -25 to 25 | While you aim: the view turned up (+) or down (-), in degrees. 0 = lined up with the gun's sights. |
| Aiming View | Field of View (`aim_fov`) | 61 | 40 to 90 | While you aim: the vertical field of view, in degrees. |

## Building from source

The release zip holds the built mod; building it yourself is only needed to change it. You need clang and ld.lld with
the MIPS target (LLVM 18.1.8 is the last official Windows release that has it). From Git Bash, in this folder:

```bash
CLANG=<path to clang.exe> LLD=<path to ld.lld.exe> sh build.sh
python tools/package.py
```

- `build.sh` builds `build/mod.elf`: big-endian MIPS II, linked at 0x81000000, the format the program's mod runtime
  loads.
- `tools/package.py` makes `release/reo-first-person-<version>.zip` (mod.json, mod.elf, README.md,
  window_layout.json), as the Mods tab installs it.
- `patches.json` holds the code sites the mod uses (every original instruction as its `xxh64:` hash only); the
  program's build takes it from this repository into its game preparation.

## Credits

- **Capcom** - Resident Evil Outbreak, the original game.
- This mod was built by RaccoonRecomp with AI-assisted programming (Claude, Anthropic).

## Legal

- This is an **unofficial fan project**. It is **not affiliated with, endorsed or sponsored by Capcom**.
- *Resident Evil*, *Resident Evil Outbreak* and all related names, characters and content are trademarks and property
  of Capcom. **All copyright and credit for the game belong to Capcom.** The names are used only to say which game this
  mod works with.
- **You need your own legal copy of the game.** This mod contains **no disc image and no game files**. Its sources name
  the game addresses the camera uses; its code-site file gives the original instructions only as hashes.
