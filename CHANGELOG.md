# Changelog

## 2.6.8

- **Sticks**: the right stick turns the view and her with it, so she always faces where you look (nothing eases back);
  the left stick walks her where you look - forward, back, left and right of the view - and never turns the view.
  Switched off, the controls are the game's own, as before.
- **Aiming**: the view looks where she aims, so a shot hits the dot in the middle (1.0.0 angled the view at the sights
  from the eyes, about 8 degrees beside the shots); with the 45 Auto and Mark's handgun the camera sits on the gun's
  sight line, so the sights line up on the dot.
- **Only the right stick turns the view**: opening a door, picking something up, the d-pad, the quick turn, a hit or
  a grab no longer turn or swing the view (the game turned her and the view turned with her); lowering the weapon,
  the view keeps looking where you aimed, up or down as well.
- **The camera stays at her head**: wherever she faces - walking sideways or back, turned round by the game, in a hit
  or a grab - it is where her eyes are when she turns her head to where you look, looking out of it (it was where her
  eyes are when she faces the view, turned about her feet: walking sideways or round a corner up to about 60 units off
  her head, out of the room into the dark; in a hit or a grab up to about 100 units, through a wall).
- **No jumps of the camera**: her own turns - to where the left stick walks her, back to where you look when she
  stands - take at most 30 degrees a game frame (a turn round about 0.2 s) instead of happening at once, and the
  camera knows each of them before the picture is drawn, so it glides with her head round her feet. Turned at once,
  her head was thrown 30 - 50 units round her feet: stopping with her back to the view the camera jumped 22 - 30
  units forward by itself and stayed 40 - 47 units off her head, and running back and forth showed the back and then
  the inside of her head. While she turns toward the stick the game walks her the way she faces. A turn the game
  makes at once (a hit, a grab) is followed smoothly; through the game's quick turn the camera keeps its spot (it no
  longer went round to her back with her head and jumped about 78 units forward) until she faces where you look again
  or walks on; her step is carried on into the next picture only as far as she goes.

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
