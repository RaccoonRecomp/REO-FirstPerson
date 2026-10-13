# Changelog

## 1.0.0

First release on GitHub: the mod as a downloadable zip for **Mods → Install Mods** (`release/reo-first-person-1.0.0.zip`).

- **In her eyes**: the camera sits in your character's eyes (a spot measured for each of the eight characters) and
  follows the head, locked to the eyes of the picture that is drawn, also while she runs, walks, turns and stops.
- **Aiming**: the camera stays in the eyes and lines up with the gun's sights (the 45 Auto, Mark's handgun; other
  weapons along her aim), with a dot in the middle of the screen; recoil plays in the picture and the view holds still.
- **Right stick**: looks around from her eyes and eases back when you let go; the game sees the right stick centred
  and R3 as no ad-lib, and the left stick moves her exactly as without the look.
- **Walls and pictures stay** that a fixed camera of the room leaves out; **J's Bar's restroom mirrors** show the room
  reflected; the restroom's light bars look as the fixed camera shows them.
- **Its own code sites** (`patches.json`: `camera.freeze` with its own title, the marks of the game frame, the pad
  mapper and the room's draw; every original instruction as its hash only), compiled in by the program at the first
  preparation. It needs no other mod's files.
- **The camera window and Configure are locked**: they show the camera's settings with their values, nothing can be
  changed there. The values: normal view Forward / Back 0, Up / Down 0, Left / Right -4, Look 0, Field of View 63;
  aiming view Forward / Back 2.5, Up / Down 3.5, Left / Right -1, Look 0, Field of View 61.
- **F7** switches it off and on while playing; off is the game's own camera, exactly as without the mod.
