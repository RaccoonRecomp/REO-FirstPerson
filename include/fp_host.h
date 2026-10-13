#ifndef FP_HOST_H
#define FP_HOST_H

// REO First Person: what this mod imports from the program's mod runtime (docs/MODDING.md, "Mod code" and "Camera
// mods"). The section names follow the recomp mod convention the runtime reads: an import is a weak empty function in
// a ".recomp_import" section, an event callback a function in a ".recomp_callback" section named after the event.
// Every argument and result is one 32-bit word ($a0-$a3, $v0).

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed short s16;
typedef signed int s32;
typedef float f32;

#define FP_IMPORT(func)                                                                                             \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")                        \
        _Pragma("GCC diagnostic ignored \"-Wreturn-type\"")                                                        \
            __attribute__((noinline, weak, used, section(".recomp_import.*"))) func {}                            \
    _Pragma("GCC diagnostic pop")
#define FP_EVENT(event) __attribute__((section(".recomp_callback.*:" #event)))
// Events: recomp_on_play_main() at every game frame (the game's frame mark); recomp_on_room_draw(room, group) at the
// camera.roomDraw mark of this mod's own code sites (patches.json, compiled into the game build: the room's draw loop,
// once per object group of the room, before its objects: room = the room's draw data, group = the group);
// recomp_on_pad_map(held, record) at its camera.padMap mark (the game's pad mapper, once per game frame for the
// character the pad drives, before the mapper copies the mapped pad record into her record: held = the address of the
// mapped record's held word, record = her character record).

// The mod's own settings (mod.json), live: a switch reads 0 / 1; a number setting its value.
FP_IMPORT(u32 recomp_get_config_u32(const char* key))
FP_IMPORT(double recomp_get_config_double(const char* key))
// A feature of the game build's feature profile by name (camera.freeze, of this mod's own code sites in patches.json,
// holds the game's own camera writes off).
FP_IMPORT(u32 reo_feature_set(const char* name, u32 on))
FP_IMPORT(u32 reo_feature_state(const char* name))
// 1 while the code of that image of the feature profile is in memory ("game.bin": a level is loaded).
FP_IMPORT(u32 reo_image_loaded(const char* image))
// The camera input service: 12 words (see FpCamInput); 0 when the host has none.
FP_IMPORT(u32 reo_cam_input(u32* out))
// The sticks of the game's controller held at the centre where the game reads the pads (FP_PAD_CENTRE_RIGHT: the right
// stick), until the next call; 0 gives them back.
FP_IMPORT(void reo_pad_override(u32 flags))
// The crosshair the program draws over the game picture: 9 words (see Fp_Crosshair).
FP_IMPORT(void reo_cam_crosshair(const u32* words))
// Inside recomp_on_room_draw only, and only for the camera mod that drives the camera: a function of the game's code
// called at once with `count` (at most 8) argument words in $a0-$a3, $t0-$t3; its $v0 (0 when refused). Every register
// is the game's again afterwards.
FP_IMPORT(u32 reo_guest_call(u32 function, const u32* args, u32 count))
// Inside recomp_on_room_draw only: `count` (1 to 64) words into a scratch block of guest memory for the calls of this
// event (on the game's stack below the mark's; put back when the event ends); its guest address (0 when refused).
FP_IMPORT(u32 reo_guest_scratch(const u32* words, u32 count))
// The status the Mods tab shows, and one line for the log.
FP_IMPORT(void reo_cam_status(u32 status, u32 detail))
FP_IMPORT(void reo_log(const char* text))

// reo_feature_set / reo_feature_state
#define FP_FEATURE_OFF 0
#define FP_FEATURE_ACTIVE 1
#define FP_FEATURE_WAITING 2
#define FP_FEATURE_UNAVAILABLE 3
#define FP_FEATURE_UNKNOWN 4

// reo_cam_status
#define FP_STATUS_OFF 0
#define FP_STATUS_WAITING 1
#define FP_STATUS_DRIVING 2
#define FP_STATUS_YIELDED 3
#define FP_STATUS_UNAVAILABLE 4

// reo_cam_input: word 2 = actions held, bit n = the action whose bind setting is the n-th of the host's list. The
// mod's one bind is its On / Off key in the setting "switch_view_button" (bit 9).
#define FP_INPUT_HELD 2
#define FP_INPUT_FOCUSED 10
#define FP_INPUT_UI_OPEN 11
#define FP_ACTION_TOGGLE 0x200u
// Words 6 and 7: the right stick (f32 x, y: -1 .. 1, +x right, +y up; 0 inside the program's dead zone, 7000 / 32767).
#define FP_INPUT_RX 6
#define FP_INPUT_RY 7

// reo_pad_override
#define FP_PAD_CENTRE_RIGHT 0x2u

#endif
