// REO First Person 2.6.8 - the first-person camera for Resident Evil Outbreak (USA, SLUS-20765 v2.00) on the
// program's mod runtime. An original mod:
// written from scratch on the program's own host services.
//
// At every game frame (the runtime's recomp_on_play_main) it puts the game's camera into the local player's eyes:
//   - normal view: the head bone's world matrix applied to a fixed point per character, 8 units in front of the eyes
//     (measured), looking where the right stick points the view;
//   - aiming view: it looks along her aim, where her shots go (her facing, tilted with the gun's elevation), so a shot
//     hits the middle of the picture; with a gun whose sights were measured the camera spot is moved onto the gun's
//     sight line (the line through the rear sight along her aim: the right hand's weapon node and the weapon's
//     measured sight spot), so the sights line up in the middle too; while a shot's recoil or a gesture moves the gun
//     off the line, the camera keeps the spot it had (no step aside): the gun kicks in the picture as the game animates
//     it. With any other weapon the camera stays at the eye spot;
//   - live tuning: each view's Forward / Back, Up / Down, Left / Right (game units, in the head's levelled axes), Look
//     Up / Down (degrees) and Field of View are the mod's settings, read at every frame (the Mods tab's Configure, or
//     the camera window: F8). With every offset at 0 the camera is at the eye spot above; the defaults are the owner's
//     numbers (kTuneDefaults);
//   - locked to the eyes of the frame that is drawn: the game draws a frame with the camera the mod wrote at the frame
//     mark before, after it has moved her body for that frame (measured: the mark comes after the draw). So the head
//     is carried on by its own motion of the last frame (the body's step, the run's bob, a turn) and the camera is put
//     into the eyes where they will be drawn; without this the camera trailed the eyes by her step (10.5 units when
//     running: inside the face);
//   - always at her head: the eye spot is laid out from her head joint in the view's direction (where her eyes are
//     when she turns her head to where you look), so walking sideways or back, turned round by the game or in a hit
//     or a grab it stays at her head, looking out of it where you look (never a spot turned about her feet);
//     no jump of its own: the pad mapper turns her at most FP_TURN_STEP a frame, as the mark planned it, so her head
//     is carried on into the drawn frame by her turn about her feet as well as her feet's step (never further than she
//     goes); a turn nobody foresaw (the game's: a hit, a grab) is taken out and eased (FP_EYE_EASE), and it keeps its
//     spot through the game's quick turn (her own turn);
//   - the right stick (the camera input service's look stick) is the only control of the view: left / right turns it
//     freely and she turns with it (her heading, as the game keeps it, is the view's while she stands), so she always
//     faces where you look; up / down looks up and down. Nothing eases back. The game sees the right stick centred
//     while the camera is in her eyes, and a press of its click (R3) is taken out of the game's new presses
//     (recomp_on_pad_map): both are Outbreak's ad-lib control;
//   - the left stick only walks, relative to the view: the game walks her by the left stick's direction from its
//     camera's (the camera block's look-at minus its eye), and the camera block looks where you look, so up walks
//     forward, left / right that way and down back (her own step back while she walks: she keeps facing the view).
//     At the pad mapper (recomp_on_pad_map) her heading is turned toward the direction the game walks her in (at most
//     FP_TURN_STEP a frame), and the game is given her heading as the stick's direction, so she goes the way she faces;
//     walking never turns the view. Wherever she faces, the camera is at her head, looking where you look;
//   - nothing else turns the view: when the game turns her itself (a door, an item, the d-pad, its quick turn, a hit,
//     a grab) the view stays where the right stick points it, and she faces it again when she stands. A new room
//     starts with the view where she faces;
//   - while she aims, the view is the aiming view (below) and the right stick turns her aim with it; raising the
//     weapon, she faces where you look; lowering it (or a hit that ends her aim), the view looks on where she aimed.
// It holds the game's own camera writes off with the camera.freeze feature of the game build, keeps characters right
// in front of the camera drawn while it drives, keeps the room's walls and pictures drawn that a fixed camera leaves
// out, lets J's Bar's restroom mirrors show the room reflected (recomp_on_room_draw), shows a crosshair dot while
// aiming and hands everything back when switched off (the Mods tab or its key) or when the game takes the camera for a
// scripted shot.

#include "fp_host.h"

// ---- Game data, Resident Evil Outbreak (USA) SLUS-20765 v2.00 ------------------------------------------------------
#define CAM_EYE 0x00324138      // f32 x, y, z: the camera's eye
#define CAM_AT 0x00324144       // f32 x, y, z: the point it looks at
#define CAM_FOV 0x00324180      // f32: vertical field of view, degrees
#define VBLANK_COUNT 0x003243F4 // u32: the game's vblank counter
#define ROOM_NOW 0x003243DC     // u32: the room she is in (changes during a door's loading screen)
#define CHAR_BASE 0x004A5C30    // character records, 0x10E0 bytes each; record 0 = the local player offline
#define REC_ALIVE 0x000         // u8 != 0: the record is in use
#define REC_STANCE 0x008        // u8: 1 normal, 2 weapon raised, 3 hit (weapon raised or down)
#define REC_AIMSTATE 0x009      // u8 while the weapon is raised: 0 raising, 1 ready, 2 firing
#define REC_ACTION 0x00A        // u8: her current action (0x0B aiming, 0x15 firing)
#define ACTION_AIM 0x0B
#define ACTION_STAND 0x0B       // (with her weapon down: she stands)
#define KIND_BACK_FIRST 0x09    // her action's kind (+0x009, her weapon down): a step back
#define KIND_BACK_LAST 0x0B
#define REC_AUTOAIM_LEFT 0x00C  // u16 while she raises her weapon: the frames the game's auto-aim still turns her
#define REC_AUTOAIM_STEP 0xC4E  // s16: its turn in each of them (65536 a turn, + = to her left)
#define REC_ID 0x030            // low byte: 0 Kevin, 1 Mark, 2 Jim, 3 George, 4 David, 5 Alyssa, 6 Yoko, 7 Cindy
#define REC_MODEL 0x034         // -> model; *(model) -> the skeleton's node pointer table
#define REC_POS 0x038           // f32 x, y, z: her feet
#define REC_FWD_X 0x070         // her model matrix's forward row, x
#define REC_FWD_Z 0x078         //                                 z
#define REC_HEADING 0x092       // s16: her heading, 65536 per turn (the facing row's atan2(x, z); + = to her left)
#define REC_AIM 0xBC8           // s16: the gun's elevation, 65536 per turn, + = up
#define REC_WEAPON 0xC7C        // u8: the item id of the equipped weapon (0 = none)
// Her walking input (game.bin): with the left stick pushed (magnitude 16 or more) the game takes the camera's yaw - the
// camera block's look-at minus its eye, atan2(x, z) in 65536ths of a turn, truncated - and the stick's direction, each
// in steps of 16, adds them and turns / walks her toward that (measured: turning on the spot first when it lies well to
// her side; with it behind her she steps back, keeping her facing, where there is room, else she turns round; with the
// dash held she turns round and runs). In some of her actions (REC_PAD_ACTION 0x1A, 0x27, 0x31, 0x80, 0xB0; 0x41 with
// REC_PAD_MODE 1) the stick is read as the d-pad's four directions instead, without the camera.
#define REC_PAD_ACTION 0x54F    // u8: her action as the walking input reads it
#define REC_PAD_MODE 0xB98      // u8
// The mapped pad record (the pad mapper's $a2 is the address of its held word): the left stick's magnitude and direction
// (s16 each), which the mapper copies into her record.
#define PAD_LSTICK_MAG 0x18
#define PAD_LSTICK_DIR 0x1A
// The mapped record's held word at the mark has the buttons as the pad gives them (Square 0x100, Cross 0x10, Circle
// 0x20); the mapper then maps the three onto the player's control type's actions through a table (a row of three action
// words per control type, Square / Cross / Circle), one of them the dash (run, 0x10).
#define CONTROL_TYPE 0x003243EC // u8: the player's control type
#define CONTROL_TABLE 0x002343C0
#define ACTION_DASH 0x10u
// A push of the left stick within this of the view's forward (65536ths of a turn: 101.25 degrees), or any push with
// the dash, sets her off that way (turning her at most FP_TURN_STEP a frame); further back without the dash she keeps
// facing the view while she stands or steps back (the game steps her back, or turns her round itself where there is
// no room).
#define FP_PAD_FRONT 0x4800
// The game's system pad (the pad of port 1, worked out at every frame before the pad mapper): the buttons newly pressed
// in this frame. Outbreak's ad-lib (game.bin) starts a line when this word has the right stick's click (R3: a line of
// kind 8) or one of the right stick's four directions (bits 0x00F00000).
#define SYS_PAD_PRESSED 0x00324210
#define PAD_R3 0x2000u
#define PAD_STICK_MOVE 16       // the magnitude from which the walking input reads the stick
#define GAME_YAW_STEP 16.0      // the camera's yaw and the stick's direction are taken in steps of 16
#define GAME_PI 3.1415927f      // the game's own constants of the yaw (degrees from radians, 65536ths from degrees):
#define GAME_UNITS 182.04445f   // the floats 0x40490FDB and 0x43360B61
#define HIDE_RADII 0x0070A6D0   // f32 x4: models nearer the camera than these are not drawn; [1] = characters
#define CUT_HEADER 0x003DDDB0   // the room's camera cuts: +1 count, +3 current; cuts of 0x198 bytes from +0x34
#define CUT_MASK 0x154          // a cut's objects: 4 words, bit i = room object i drawn while the cut is current
#define CUT_MASK_WORDS 4
#define CUT_MAX 64
#define OBJ_COUNT 0x0049F762    // u16: the room's objects (walls, pictures, furniture)
#define OBJ_FLAGS 0x0049F790    // u8 per object: bit 0 = drawn (set from the cut's mask), bit 7 = hidden by an event

// The skeleton (the same layout for all eight characters). A node starts with its world matrix: rows X, Y, Z (each
// carries the model's scale) at +0x00 / +0x10 / +0x20, the translation at +0x30, +0x3C = 1.0; +0xC8 = its parent.
#define NODE_HEAD 21  // the head bone
#define NODE_EYE 23   // an eye point (a child of the head's child)
#define NODE_WRIST 19 // the right wrist
#define NODE_HAND 28  // the right hand's weapon node (a child of the right wrist): the weapon is drawn on it

// ---- The eye spot: where the camera sits in each character's head (measured, head-bone space, model units) --------
// up = the eyes' height above the head joint; forward = 16: 8 units in front of the eyes (at 12 the face came in front
// of the camera while walking; at the eyes the picture is the inside of the head). restCos / restSin = the cosine and
// sine of the head bone's measured pitch when she stands still: the view is turned by it, so standing still looks
// level and every head movement moves the view as much as it moves the head.
typedef struct {
    f32 up;
    f32 forward;
    f32 restCos;
    f32 restSin;
} FpEyes;
static const FpEyes kEyes[8] = {
    {7.5f, 16.0f, 0.98481f, -0.17365f},  // Kevin   (rest pitch -10.0 degrees)
    {7.5f, 16.0f, 0.99337f, -0.11494f},  // Mark    (-6.6)
    {7.5f, 16.0f, 0.99488f, -0.10106f},  // Jim     (-5.8)
    {7.5f, 16.0f, 0.97815f, -0.20791f},  // George  (-12.0)
    {6.7f, 16.0f, 0.99604f, -0.08889f},  // David   (-5.1)
    {7.0f, 16.0f, 0.97815f, -0.20791f},  // Alyssa  (-12.0)
    {6.5f, 16.0f, 0.99317f, -0.11667f},  // Yoko    (-6.7)
    {7.0f, 16.0f, 0.99098f, -0.13399f},  // Cindy   (-7.7)
};

// ---- The sights: the spot just behind each weapon's rear sight (measured, the weapon node's axes, game units) ------
// x along the weapon node's X axis (the barrel points along -X), y along Y, z along Z (up on the gun). The aiming view
// looks along her aim from the sight line: the line through this spot along her aim (the barrel points along her aim
// within about 1.5 degrees, measured with Kevin's 45 Auto; her shots go along her aim). A weapon without an entry is
// aimed along her aim from the eye spot.
typedef struct {
    u32 item;
    f32 x, y, z;
} FpSight;
static const FpSight kSights[] = {
    {0x51, 0.0f, -1.7f, 9.0f},  // 45 Auto (Kevin's)
    {0x52, 2.0f, -1.7f, 9.0f},  // Handgun for Mark (9mm)
};
#define FP_SIGHTS ((u32)(sizeof(kSights) / sizeof(kSights[0])))

// ---- The live tuning settings (mod.json): per view Forward / Back, Up / Down, Left / Right, Look Up / Down, FOV ---
typedef struct {
    f32 fwd, up, right, look, fov;
} FpTune;
static const char* const kTuneKeys[2][5] = {
    {"normal_forward", "normal_up", "normal_right", "normal_look", "normal_fov"},
    {"aim_forward", "aim_up", "aim_right", "aim_look", "aim_fov"},
};
// The defaults (mod.json has the same): the owner's numbers from the 1.0.1-tuning test (2026-10-10); the normal view's
// Forward / Back 0 (the eye spot itself, the owner had 2.5): with the eyes locked to the drawn frame running stays clear
// of the face down to -1 (measured: at -2 the run's first long stride shows the inside of the head for one picture).
#define FP_NORMAL_FORWARD 0.0f
static const FpTune kTuneDefaults[2] = {
    {FP_NORMAL_FORWARD, 0.0f, -4.0f, 0.0f, 63.0f},  // normal view
    {2.5f, 3.5f, -1.0f, 0.0f, 61.0f},               // aiming view
};
#define FP_OFFSET_MAX 25.0f       // the offsets' range, game units (mod.json: -25 .. 25)
#define FP_LOOK_MAX 25.0f         // Look Up / Down, degrees (mod.json: -25 .. 25)
#define FP_FOV_MIN 40.0f          // Field of View, degrees (mod.json: 40 .. 90)
#define FP_FOV_MAX 90.0f

#define FP_LOOK 500.0f            // distance of the look-at point
#define FP_STEADY_COS 0.9063078f  // the barrel within 25 degrees of her aim: the sights are usable
#define FP_SETTLED_COS 0.9993908f // and turned less than 2 degrees since the last frame: held still
#define FP_BLEND_VBLANKS 12.0f    // normal <-> aiming view and her aim <-> the sight line take 0.2 s
#define FP_TAKEN_DIST2 (25.0f * 25.0f)
// Carrying the head on into the drawn frame: a change that grew by more than this since the frame before (a one-off
// jump of the pose: the run's first long stride, a shot's recoil) is carried on only at the size of the one before;
// a step longer than FP_PREDICT_JUMP (a door, a new room) is not carried on at all.
#define FP_PREDICT_GROW 6.0f      // game units per frame (her position; running is 10.5 a frame)
#define FP_PREDICT_GROW_AXIS 0.1f // the head's axes (unit length about 1: 0.1 is about 6 degrees a frame)
#define FP_PREDICT_JUMP 40.0f
#define FP_CARRY_MAX 11.0f        // her feet's step is carried on at most at her run's (10.5; changing her way while
                                  // running, the game steps her 14 - 18 units once)
// The normal view's spot stays at her head and never jumps on its own. Her head is ahead of her feet (5 - 25 units
// walking, standing, running, setting off; 36 bent forward), so every turn of her body about her feet swings it round
// them: the camera follows that swing as her head's own motion where the turn is known before the frame is drawn - the
// pad mapper's turn of her, which the mark plans (below), and the game's own steady turn (no more than FP_GAME_TURN a
// frame, carried on). A turn nobody foresaw (the game's: a hit, a grab, its own turns; the pad mapper's few degrees
// toward a new push of the stick) is taken out (the camera moves as her feet that frame) and the camera eases onto her
// head again (FP_EYE_EASE of the distance a frame, FP_EYE_EASE_MIN .. FP_EYE_EASE_MAX units), never more than
// FP_EASE_REACH off her head (a longer swing - a grab spinning her round, crouched - is followed at once beyond that)
// and never with her head coming in front of the lens (FP_HEAD_CLEAR: the camera is never nearer her head joint than
// this along the view; the eye spot is about 16 units in front of it, her face about 12).
#define FP_EYE_EASE 0.5f
#define FP_EYE_EASE_MIN 1.5f
#define FP_EYE_EASE_MAX 6.0f
#define FP_HEAD_CLEAR 14.0f
#define FP_EASE_REACH 20.0f
#define FP_GAME_TURN 2731        // 15 degrees a frame (the game's turn while she walks is 8)
#define FP_TURN_UNSEEN 364       // 2 degrees: a turn this much off the one foreseen is taken out
// The pad mapper's turns of her (to the view as she stands, to the way the left stick walks her) are at most
// FP_TURN_STEP a frame (a turn round 0.2 s), so her head never jumps round her feet (running, 30 degrees swing it 8
// units; at once, a turn round threw it 30 - 50 units, into the camera's place). The mark plans each frame's turn from
// the stick as it was in the frame before (the pad mapper then turns her so), so the camera is at her head as it is
// drawn (bent far forward, her head ahead of her feet by more than about 19 units, at most FP_TURN_SWING of her head's
// swing a frame); a new push of the stick turns her at most FP_TURN_ASIDE off that plan in its first frame (her head
// swings at most about 4 units then). While it turns her toward the stick, the game's walking input is given her
// heading as the stick's direction, so it walks her the way she faces (as it did when she was turned at once).
#define FP_TURN_STEP 5461        // 30 degrees (65536 a turn)
#define FP_TURN_ASIDE 1456       // 8 degrees
#define FP_TURN_SWING 10.0f      // and never a turn that swings her head more than this a frame (bent far forward)
#define FP_YAW_PER_RADIAN 10430.378f
#define FP_KIND_QUICK_TURN 0x15 // her action's kind (+0x009, her weapon down) in the game's quick turn
#define FP_QUICK_BACK_COS 0.9396926f // after it, she faces the view again within 20 degrees
#define FP_RESUME_DIST2 (30.0f * 30.0f)
// A door into another room (ROOM_NOW changes during its loading screen): the view starts from her heading where the
// game places her there (her head jumps there, FP_PREDICT_JUMP), once; it is the right stick's again from then on.
#define FP_ROOM_LOADING 1       // the room changed, she is not placed yet
#define FP_ROOM_PLACED 2        // placed (the view started from her heading), not yet yours
// The right stick's look (the program's dead zone is applied by the camera input service), at full push: left / right
// turns the view and her with it, up / down tilts the view within the limits (looking further up showed her brow at the
// picture's edge, measured with Yoko and Kevin; looking down shows her own body). Nothing eases back.
#define FP_STICK_TURN 140.0f        // degrees a second, left / right
#define FP_STICK_TILT 90.0f         // degrees a second, up / down
#define FP_STICK_UP_MAX 30.0f       // degrees up
#define FP_STICK_DOWN_MAX 60.0f     // degrees down
#define FP_VBLANK_SECONDS (1.0f / 59.94f)
#define FP_YAW_UNITS 182.04445f     // the game's yaw units (65536 a turn) per degree
// While she aims, a turn of hers the game makes itself is carried on into the drawn frame: its auto-aim as she raises
// the weapon exactly (its turn per frame while it counts down: REC_AUTOAIM_*), any other when it is no faster than
// this (6 degrees a frame, in the game's units: its aim controls turn her 4); a faster one is followed a frame late, so
// the view never swings past where she stops.
#define FP_AIM_CARRY 1092
#define FP_AUTOAIM_MAX 8192     // (an auto-aim step larger than 45 degrees a frame is not one)

// ---- EE RAM: aligned words only (the runtime's guest access) --------------------------------------------------------
static inline u32 rd32(u32 a) {
    return *(volatile u32*)(a & ~3u);
}
static inline void wr32(u32 a, u32 v) {
    *(volatile u32*)(a & ~3u) = v;
}
static inline u32 rd8(u32 a) {
    return (rd32(a) >> ((a & 3u) * 8)) & 0xFFu;
}
static inline u32 rd16(u32 a) {
    return (rd32(a) >> ((a & 2u) * 8)) & 0xFFFFu;
}
static inline void wr8(u32 a, u32 v) {
    u32 sh = (a & 3u) * 8;
    wr32(a, (rd32(a) & ~(0xFFu << sh)) | ((v & 0xFFu) << sh));
}
static inline void wr16(u32 a, u32 v) {
    u32 sh = (a & 2u) * 8;
    wr32(a, (rd32(a) & ~(0xFFFFu << sh)) | ((v & 0xFFFFu) << sh));
}
typedef union {
    f32 f;
    u32 u;
} FpWord;
static inline f32 rdf(u32 a) {
    FpWord w;
    w.u = rd32(a);
    return w.f;
}
static inline void wrf(u32 a, f32 v) {
    FpWord w;
    w.f = v;
    wr32(a, w.u);
}
static inline u32 bitsf(f32 v) {
    FpWord w;
    w.f = v;
    return w.u;
}
static inline u32 sane(f32 v) {
    FpWord w;
    w.f = v;
    return (w.u & 0x7F800000u) != 0x7F800000u && v > -1e6f && v < 1e6f;
}

// ---- Small maths ------------------------------------------------------------------------------------------------------
typedef struct {
    f32 x, y, z;
} V3;
static f32 fp_sqrt(f32 x) {
    return x > 0.0f ? __builtin_sqrtf(x) : 0.0f;
}
static V3 v3(f32 x, f32 y, f32 z) {
    V3 r;
    r.x = x;
    r.y = y;
    r.z = z;
    return r;
}
static V3 rdv(u32 a) {
    return v3(rdf(a), rdf(a + 4), rdf(a + 8));
}
static V3 add(V3 a, V3 b) {
    return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}
static V3 sub(V3 a, V3 b) {
    return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}
static V3 mul(V3 a, f32 s) {
    return v3(a.x * s, a.y * s, a.z * s);
}
static f32 dot(V3 a, V3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static V3 cross(V3 a, V3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static V3 lerp(V3 a, V3 b, f32 t) {
    return add(a, mul(sub(b, a), t));
}
static V3 unit(V3 a, V3 fallback) {
    f32 n = fp_sqrt(dot(a, a));
    return n > 1e-5f && sane(n) ? mul(a, 1.0f / n) : fallback;
}
static u32 sane3(V3 a) {
    return sane(a.x) && sane(a.y) && sane(a.z);
}
// sin and cos for |x| <= pi/2 (the gun's elevation and Look Up / Down stay well within it).
static f32 fp_sin(f32 x) {
    f32 x2 = x * x;
    return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f * (1.0f - x2 / 72.0f))));
}
static f32 fp_cos(f32 x) {
    f32 x2 = x * x;
    return 1.0f - x2 / 2.0f * (1.0f - x2 / 12.0f * (1.0f - x2 / 30.0f * (1.0f - x2 / 56.0f)));
}
static f32 clampf(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
// A direction tilted up by the angle whose cosine and sine are c and s (around its own horizontal axis).
static V3 tilt(V3 d, f32 c, f32 s) {
    f32 h = fp_sqrt(d.x * d.x + d.z * d.z), nc, ns;
    if (!(h > 1e-4f)) {
        return d;
    }
    nc = h * c - d.y * s;
    ns = d.y * c + h * s;
    if (nc < 0.05f) {
        nc = 0.05f;  // (never past straight up or down)
    }
    return unit(v3(d.x / h * nc, ns, d.z / h * nc), d);
}
// A direction turned up (+) or down (-) by degrees.
static V3 look_up(V3 d, f32 degrees) {
    f32 a = degrees * 0.017453293f;
    return degrees == 0.0f ? d : tilt(d, fp_cos(a), fp_sin(a));
}
// A yaw in the game's units (65536 a turn) into 0 .. 65536, and the signed difference a - b (-32768 .. 32768).
static f32 yaw_wrap(f32 a) {
    while (a >= 65536.0f) {
        a -= 65536.0f;
    }
    while (a < 0.0f) {
        a += 65536.0f;
    }
    return a;
}
static s32 yaw_diff(u32 a, u32 b) {
    return (s32)(s16)(u16)((a - b) & 0xFFFFu);
}
// sin and cos of a yaw in the game's units, any value (the quarter turn nearest it, then the series).
static void yaw_sincos(f32 yaw, f32* sn, f32* cs) {
    f32 a = yaw_wrap(yaw), r, s0, c0;
    u32 q = (u32)((a + 8192.0f) / 16384.0f);
    r = (a - (f32)q * 16384.0f) * (6.2831853f / 65536.0f);
    s0 = fp_sin(r);
    c0 = fp_cos(r);
    switch (q & 3u) {
    case 0: *sn = s0; *cs = c0; break;
    case 1: *sn = c0; *cs = -s0; break;
    case 2: *sn = -s0; *cs = -c0; break;
    default: *sn = -c0; *cs = s0; break;
    }
}
// A vector turned about the vertical by a yaw in the game's units (+ = to her left, as her heading turns).
static V3 turn_to(V3 d, f32 yaw) {
    f32 sn, cs;
    yaw_sincos(yaw, &sn, &cs);
    return v3(d.x * cs + d.z * sn, d.y, d.z * cs - d.x * sn);
}
// A heading turned toward another by at most a step (65536 a turn).
static u32 turn_toward(u32 from, u32 to, s32 step) {
    s32 d = yaw_diff(to, from);
    d = d > step ? step : (d < -step ? -step : d);
    return (u32)((s32)from + d) & 0xFFFFu;
}
// atan2(y, x) in doubles (to about 1e-15): the series of atan for |t| <= tan(15 degrees), atan(t) = 30 degrees +
// atan((t sqrt(3) - 1) / (t + sqrt(3))) above it, and the octants.
static double atan_small(double t) {
    double t2 = t * t, term = t, sum = t;
    s32 k;
    for (k = 1; k <= 12; k++) {
        term *= -t2;
        sum += term / (double)(2 * k + 1);
    }
    return sum;
}
static double atan_unit(double t) {
    return t > 0.2679491924311228 ? 0.5235987755982989 +
                                        atan_small((t * 1.7320508075688772 - 1.0) / (t + 1.7320508075688772))
                                  : atan_small(t);
}
static double fp_atan2(double y, double x) {
    double ay = y < 0.0 ? -y : y, ax = x < 0.0 ? -x : x, a;
    if (ax == 0.0 && ay == 0.0) {
        return 0.0;
    }
    a = ay <= ax ? atan_unit(ay / ax) : 1.5707963267948966 - atan_unit(ax / ay);
    if (x < 0.0) {
        a = 3.141592653589793 - a;
    }
    return y < 0.0 ? -a : a;
}
// The game's camera yaw for the camera block's look-at minus its eye (dx, dz): the step of 16 of 65536 a turn the
// walking input takes (0 .. 4095).
static s32 yaw_step(f32 dx, f32 dz) {
    double deg, u;
    if (dx == 0.0f) {
        dx = 1e-4f;  // (as the game does)
    }
    deg = fp_atan2((double)dx, (double)dz) * 180.0 / (double)GAME_PI;
    if (deg < 0.0) {
        deg += 360.0;
    }
    u = deg * (double)GAME_UNITS;
    return (s32)(u / GAME_YAW_STEP) & 0xFFF;
}
// A point / direction given in her frame (forward, right, up) in the world, and back.
static V3 from_local(V3 origin, V3 fwd, V3 right, V3 l) {
    return add(origin, add(mul(fwd, l.x), add(mul(right, l.y), v3(0.0f, l.z, 0.0f))));
}
static V3 to_local(V3 d, V3 fwd, V3 right) {
    return v3(dot(d, fwd), dot(d, right), d.y);
}

// ---- Log ----------------------------------------------------------------------------------------------------------------
static u32 put_text(char* buf, u32 n, const char* text) {
    u32 i;
    for (i = 0; text[i] && n < 150; i++) {
        buf[n++] = text[i];
    }
    return n;
}
static u32 put_num(char* buf, u32 n, s32 v) {
    char d[12];
    u32 k = 0, u = v < 0 ? (u32)(-v) : (u32)v;
    buf[n++] = ' ';
    if (v < 0) {
        buf[n++] = '-';
    }
    do {
        d[k++] = (char)('0' + u % 10);
        u /= 10;
    } while (u && k < 11);
    while (k) {
        buf[n++] = d[--k];
    }
    return n;
}
// " -12.5": one decimal.
static u32 put_fixed(char* buf, u32 n, f32 v) {
    s32 t = (s32)(v * 10.0f + (v < 0.0f ? -0.5f : 0.5f));
    u32 u = t < 0 ? (u32)(-t) : (u32)t, whole = u / 10, k = 0;
    char d[12];
    buf[n++] = ' ';
    if (t < 0) {
        buf[n++] = '-';
    }
    do {
        d[k++] = (char)('0' + whole % 10);
        whole /= 10;
    } while (whole && k < 11);
    while (k) {
        buf[n++] = d[--k];
    }
    buf[n++] = '.';
    buf[n++] = (char)('0' + u % 10);
    return n;
}
static void fp_log(const char* text, u32 count, s32 a, s32 b) {
    char buf[160];
    u32 n = put_text(buf, 0, text);
    if (count > 0) {
        n = put_num(buf, n, a);
    }
    if (count > 1) {
        n = put_num(buf, n, b);
    }
    buf[n] = 0;
    reo_log(buf);
}
// "[fp] normal view: forward 0.0 up 0.0 right 0.0 look 0.0 fov 65.0"
static void log_tune(u32 view, const FpTune* t) {
    char buf[160];
    u32 n = put_text(buf, 0, view ? "[fp] aiming view:" : "[fp] normal view:");
    n = put_text(buf, n, " forward");
    n = put_fixed(buf, n, t->fwd);
    n = put_text(buf, n, " up");
    n = put_fixed(buf, n, t->up);
    n = put_text(buf, n, " right");
    n = put_fixed(buf, n, t->right);
    n = put_text(buf, n, " look");
    n = put_fixed(buf, n, t->look);
    n = put_text(buf, n, " fov");
    n = put_fixed(buf, n, t->fov);
    buf[n] = 0;
    reo_log(buf);
}

// ---- Carrying a value on into the frame that will be drawn ---------------------------------------------------------
// The value at the last frame mark and its change in the frame before.
typedef struct {
    u32 have;
    V3 last;
    V3 delta;
} FpTrack;

// The value as it will be when the next frame is drawn: now plus its change of the last frame; a change that grew by
// more than `grow` since the frame before is carried on only at the size of the one before; a change longer than
// FP_PREDICT_JUMP (longer than any step of hers) is not carried on, nor is it the one before for the next frame.
static V3 track(FpTrack* t, V3 now, f32 grow) {
    V3 d;
    f32 n1, n0;
    if (!t->have) {
        t->have = 1;
        t->last = now;
        t->delta = v3(0.0f, 0.0f, 0.0f);
        return now;
    }
    d = sub(now, t->last);
    n1 = fp_sqrt(dot(d, d));
    n0 = fp_sqrt(dot(t->delta, t->delta));
    t->last = now;
    t->delta = d;
    if (n1 > FP_PREDICT_JUMP) {
        t->delta = v3(0.0f, 0.0f, 0.0f);
        return now;
    }
    if (n1 > n0 + grow) {
        d = mul(d, n0 / n1);
    }
    return add(now, d);
}

// ---- State --------------------------------------------------------------------------------------------------------------
static struct {
    u32 init;
    u32 wantFreeze;  // what was last asked of camera.freeze (2 = nothing yet)
    u32 status;      // what was last reported (0xFF = nothing yet)
    u32 driving;     // the camera is ours
    u32 yielded;     // the game took it for a scripted shot
    V3 yieldPos;
    u32 userOff;     // switched off with the key while playing
    u32 keyHeld;
    u32 haveHide;    // the characters' hide radius is ours (the game's value kept)
    f32 hideOrig;
    u32 haveLast;    // what was written last frame (to see the game write over it)
    V3 lastEye;
    V3 lastAt;
    u32 taken;
    u32 lastVbl;
    u32 model;       // the skeleton driven (a new one = another character; a door keeps it: ROOM_NOW)
    u32 room;        // the room at the last mark
    u32 newRoom;     // a door took her into another room: FP_ROOM_* until she is yours again there
    f32 aimBlend;    // 0 normal view, 1 aiming view
    f32 sightBlend;  // 0 the aiming view's camera at the eye spot, 1 on the sight line
    u32 haveSight;   // the sight line was taken: the step from the eye spot onto it, in her frame
    V3 lineLocal;
    u32 holding;     // the gun left the line on its own (a shot, a gesture): the camera keeps the last step
    u32 havePrev;    // the gun's direction last frame (to see whether it is held still)
    V3 prevBarrel;
    u32 prevAim;
    u32 crossOn;
    u32 haveTune;    // the tuning values last seen (logged when they change)
    FpTune tune[2];
    FpTrack trY, trZ, trT, trF;  // the head's Y / Z axes and joint, her facing: at the last mark and their change
    u32 easing;                  // a swing of her head taken out: the camera's way back onto her head (FP_EYE_EASE)
    V3 ease;
    u32 quickHold;               // the game's quick turn: the normal view's spot kept by her feet
    V3 quickSpot;
    u32 haveOut;                 // the normal view's spot, her feet, her head joint from them and her heading, last
    V3 lastOut;                  //   frame, her feet's step then (carried on into the drawn frame) and her turn
    V3 lastPos;                  //   carried on into it
    V3 lastR;
    u32 outH;
    V3 feetStep;
    s32 turnNext;
    u32 planWas;                 //   (that turn was the pad mapper's planned one)
    u32 plan;                    // the pad mapper's turn of her in the next frame (FP_TURN_*): her heading then
    u32 planH;
    s32 planMost;                //   (its largest turn a frame)
    s32 padMag;                  // the left stick as the pad mapper last saw it (magnitude, direction, the dash held)
    u32 padDir;
    u32 padDash;
    f32 stickX;      // the right stick this frame (0: none, or a menu of the program takes the input)
    f32 stickY;
    u32 padCentred;  // the game's right stick is held centred
    u32 haveView;    // the view below is set (from her heading when the camera comes into her eyes)
    f32 viewYaw;     // where you look: the view's yaw in the game's units (65536 a turn, as her heading)
    f32 viewPitch;   //                 degrees up (+) / down (-)
    u32 lastH;       // her heading at the last mark
    s32 aimTurn;     // while she aims: the right stick's turn of her aim for the pad mapper (game units)
    s32 aimApplied;  //                 the turn the pad mapper applied in this frame
    u32 wasRaised;   // her weapon was raised at the last mark
    f32 aimYaw;      // while she aims: where the picture looks (yaw in the game's units, pitch in degrees), for the
    f32 aimPitch;    //                 normal view when her aim ends
} s;

static u32 freeze(u32 on) {
    on = on ? 1u : 0u;
    if (s.wantFreeze != on) {
        s.wantFreeze = on;
        return reo_feature_set("camera.freeze", on);
    }
    return reo_feature_state("camera.freeze");
}

static void status(u32 st) {
    if (st != s.status) {
        s.status = st;
        reo_cam_status(st, 0);
    }
}

// The right stick is Outbreak's ad-lib control: while the camera is in her eyes it looks around, and the game sees it
// centred (so looking around triggers no ad-lib line), and a press of its click no new press (recomp_on_pad_map);
// handed back, the game has both again.
static void pad_centre(u32 on) {
    if (on || s.padCentred) {
        reo_pad_override(on ? FP_PAD_CENTRE_RIGHT : 0u);
    }
    s.padCentred = on ? 1u : 0u;
}

// One axis of the right stick from its word (0 when it is not a number).
static f32 stick_axis(u32 word) {
    FpWord w;
    w.u = word;
    return sane(w.f) ? clampf(w.f, -1.0f, 1.0f) : 0.0f;
}

// The game's walking input reads the stick by the camera (not as the d-pad's directions) in this action.
static u32 stick_by_camera(u32 rec) {
    u32 act = rd8(rec + REC_PAD_ACTION);
    return !(act == 0x1A || act == 0x27 || act == 0x31 || act == 0x80 || act == 0xB0 ||
             (act == 0x41 && rd8(rec + REC_PAD_MODE) == 1));
}

// A small white dot in the middle of the picture while aiming (the program draws it over the game picture).
static void crosshair(u32 on) {
    u32 w[9];
    on = on ? 1u : 0u;
    if (on == s.crossOn) {
        return;
    }
    s.crossOn = on;
    w[0] = on;          // visible
    w[1] = 0;           // a dot
    w[2] = 2;           // its radius, pixels
    w[3] = 0;           // (cross only)
    w[4] = 1;           // (cross only)
    w[5] = 0xFFFFFFu;   // white
    w[6] = 0;           // centred
    w[7] = 0;
    w[8] = bitsf(on ? 1.0f : 0.0f);
    reo_cam_crosshair(w);
}

// The room's current cut camera back (eye and look-at: s32 x3 at +0xA0 / +0xB8; FOV f32 at +0x14), so the game's own
// view returns at once and not only at its next cut change.
static void restore_room_camera(void) {
    u32 n = rd8(CUT_HEADER + 1), cur = rd8(CUT_HEADER + 3), rec, i;
    f32 fov;
    if (n == 0 || n > 64 || cur >= n) {
        return;
    }
    rec = CUT_HEADER + 0x34 + cur * 0x198;
    for (i = 0; i < 3; i++) {
        wrf(CAM_EYE + 4 * i, (f32)(s32)rd32(rec + 0xA0 + 4 * i));
        wrf(CAM_AT + 4 * i, (f32)(s32)rd32(rec + 0xB8 + 4 * i));
    }
    fov = rdf(rec + 0x14);
    if (sane(fov) && fov > 1.0f && fov < 120.0f) {
        wrf(CAM_FOV, fov);
    }
}

static void restore_hide(void) {
    if (s.haveHide) {
        wrf(HIDE_RADII + 4, s.hideOrig);
        s.haveHide = 0;
    }
}

// ---- The room's walls and pictures --------------------------------------------------------------------------------
// Each of the room's fixed cameras (cuts) draws only the objects of its mask: at every cut change the game sets each
// object's bit 0 from the new cut's mask. A cut leaves out what stands between its lens and the room (the wall it looks
// through, the pictures on that wall, a bulb next to the lens), and the cut still follows her position while this
// camera drives: those walls and pictures were missing in her view (J's Bar's restroom: the far wall and its framed
// poster, black). While the camera drives, every cut's mask also holds the objects the cuts that look at them draw, so
// the game's own cut change keeps them (no flicker at a cut change). An object every cut that looks at it leaves out
// stays out (the restroom's vent grille: the vent is open, its grille lies on the floor); so do objects no cut draws and
// an event's hide (bit 7). Some objects no cut draws while the camera drives (the drop: J's Bar's restroom, see the
// mirrors below). Handed back, the masks and the current cut's objects are the game's again.
#define OBJ_MATRIX 0x0049FA10   // f32 4x4 per object (0x40 bytes): its world matrix, the origin at +0x30
#define ART_MARGIN_TAN 0.267949f // a cut "looks at" an object whose origin is within its field of view + 15 degrees
static struct {
    u32 on;                          // the masks below are written
    u32 cuts, words;
    u32 add[CUT_MASK_WORDS];         // what every cut also draws
    u32 drop[CUT_MASK_WORDS];        // what no cut draws
    u32 orig[CUT_MAX][CUT_MASK_WORDS];
} sArt;

// A cut's camera: eye, axes and the tangents of its half fields of view (+ the margin; 0 = no limit).
typedef struct {
    u32 ok;
    V3 eye, fw, rt, up;
    f32 tx, ty;
} ArtCam;
static ArtCam sArtCam[CUT_MAX];

static u32 cut_mask(u32 cut, u32 w) {
    return CUT_HEADER + 0x34 + cut * 0x198 + CUT_MASK + 4 * w;
}

static f32 tan_add(f32 a, f32 b) {
    f32 d = 1.0f - a * b;
    return d > 0.01f ? (a + b) / d : 0.0f;
}

static void art_cam(u32 cut, ArtCam* k) {
    u32 rec = CUT_HEADER + 0x34 + cut * 0x198, i;
    f32 e[3], t[3], fov = rdf(rec + 0x14), tv;
    for (i = 0; i < 3; i++) {
        e[i] = (f32)(s32)rd32(rec + 0xA0 + 4 * i);
        t[i] = (f32)(s32)rd32(rec + 0xB8 + 4 * i);
    }
    if (!sane(fov) || fov < 1.0f || fov > 170.0f) {
        fov = 60.0f;
    }
    k->eye = v3(e[0], e[1], e[2]);
    k->fw = unit(v3(t[0] - e[0], t[1] - e[1], t[2] - e[2]), v3(0.0f, 0.0f, 0.0f));
    k->ok = dot(k->fw, k->fw) > 0.5f;
    k->rt = unit(v3(k->fw.z, 0.0f, -k->fw.x), v3(1.0f, 0.0f, 0.0f));
    k->up = cross(k->rt, k->fw);
    fov *= 0.5f * 0.017453293f;
    tv = fp_sin(fov) / fp_cos(fov);
    k->ty = tan_add(tv, ART_MARGIN_TAN);
    k->tx = tan_add(tv * (4.0f / 3.0f), ART_MARGIN_TAN);
}

static u32 art_seen(const ArtCam* k, V3 p) {
    V3 rel = sub(p, k->eye);
    f32 z = dot(rel, k->fw), x = dot(rel, k->rt), y = dot(rel, k->up);
    if (!(z > 0.0f)) {
        return 0;
    }
    x = x < 0.0f ? -x : x;
    y = y < 0.0f ? -y : y;
    return (k->tx == 0.0f || x <= z * k->tx) && (k->ty == 0.0f || y <= z * k->ty);
}

// A cut's mask as the camera has it while it drives.
static u32 art_mask(u32 c, u32 w) {
    return (sArt.orig[c][w] | sArt.add[w]) & ~sArt.drop[w];
}

// The room's masks are still the ones written (the same room: a new room's are the game's).
static u32 art_ours(void) {
    u32 c, w;
    if (rd8(CUT_HEADER + 1) != sArt.cuts || (rd16(OBJ_COUNT) + 31) / 32 != sArt.words) {
        return 0;
    }
    for (c = 0; c < sArt.cuts; c++) {
        for (w = 0; w < sArt.words; w++) {
            if (rd32(cut_mask(c, w)) != art_mask(c, w)) {
                return 0;
            }
        }
    }
    return 1;
}

// Bit 0 of the objects whose bit the camera's mask of the current cut changes (another cut draws it, or it is
// dropped): as the camera's mask has it (ours), or as the game's has it.
static void art_current(u32 ours) {
    u32 cur = rd8(CUT_HEADER + 3), count = rd16(OBJ_COUNT), i, f, bit, game, want;
    if (cur >= sArt.cuts) {
        return;
    }
    for (i = 0; i < count; i++) {
        bit = 1u << (i & 31);
        game = sArt.orig[cur][i >> 5] & bit;
        if ((art_mask(cur, i >> 5) & bit) == game) {
            continue;
        }
        want = ours ? (art_mask(cur, i >> 5) & bit) != 0 : game != 0;
        f = rd8(OBJ_FLAGS + i);
        if ((f & 1u) != want) {
            wr8(OBJ_FLAGS + i, want ? (f | 1u) : (f & ~1u));
        }
    }
}

// Every cut's mask as the camera has it.
static void art_write(void) {
    u32 c, w;
    for (c = 0; c < sArt.cuts; c++) {
        for (w = 0; w < sArt.words; w++) {
            if (rd32(cut_mask(c, w)) != art_mask(c, w)) {
                wr32(cut_mask(c, w), art_mask(c, w));
            }
        }
    }
}

static void mirror_drop(u32* drop);

static void keep_artwork(void) {
    u32 n = rd8(CUT_HEADER + 1), count = rd16(OBJ_COUNT), c, w, i, any, looked, drawn, same;
    u32 drop[CUT_MASK_WORDS];
    V3 p;
    mirror_drop(drop);
    if (sArt.on && art_ours()) {
        for (same = 1, w = 0; w < CUT_MASK_WORDS; w++) {
            same &= drop[w] == sArt.drop[w];
        }
        if (!same) {
            // The same room, another drop: the current cut's objects back as the game's, then the new masks.
            art_current(0);
            for (w = 0; w < CUT_MASK_WORDS; w++) {
                sArt.drop[w] = drop[w];
            }
            art_write();
            art_current(1);
        }
        return;
    }
    sArt.on = 0;
    if (n == 0 || n > CUT_MAX || count == 0 || count > 32 * CUT_MASK_WORDS) {
        return;
    }
    sArt.cuts = n;
    sArt.words = (count + 31) / 32;
    for (c = 0; c < n; c++) {
        for (w = 0; w < sArt.words; w++) {
            sArt.orig[c][w] = rd32(cut_mask(c, w));
        }
        art_cam(c, &sArtCam[c]);
    }
    // Per object: drawn by a cut that looks at it; one no cut looks at: drawn by any cut.
    for (w = 0; w < CUT_MASK_WORDS; w++) {
        sArt.add[w] = 0;
    }
    for (i = 0; i < count; i++) {
        p = rdv(OBJ_MATRIX + 0x40 * i + 0x30);
        any = looked = drawn = 0;
        for (c = 0; c < n; c++) {
            u32 bit = (sArt.orig[c][i >> 5] >> (i & 31)) & 1u;
            any |= bit;
            if (sArtCam[c].ok && sane3(p) && art_seen(&sArtCam[c], p)) {
                looked = 1;
                drawn |= bit;
            }
        }
        if (looked ? drawn : any) {
            sArt.add[i >> 5] |= 1u << (i & 31);
        }
    }
    for (w = 0; w < CUT_MASK_WORDS; w++) {
        sArt.drop[w] = drop[w];
    }
    art_write();
    sArt.on = 1;
    art_current(1);  // (the current cut was set with the game's mask)
}

static void glow_restore(void);

static void restore_artwork(void) {
    u32 c, w;
    glow_restore();
    if (!sArt.on) {
        return;
    }
    if (art_ours()) {
        for (c = 0; c < sArt.cuts; c++) {
            for (w = 0; w < sArt.words; w++) {
                if (rd32(cut_mask(c, w)) != sArt.orig[c][w]) {
                    wr32(cut_mask(c, w), sArt.orig[c][w]);
                }
            }
        }
        art_current(0);
    }
    sArt.on = 0;
}

// ---- J's Bar's restroom mirrors -------------------------------------------------------------------------------------
// The two mirrors over the sinks are no mirrors: grimy glass panes (room object 46) in front of a mirrored copy of the
// room's door end and poster wall built behind the wall (objects 14 and 15) for the game's own fixed camera of the
// sinks (cut 0) only; from her eyes the copy showed black glass and two posters floating in it. While this camera
// drives there, the mirrors show the room reflected, as the game's own mirror shows it from its fixed camera:
//   - at the room's draw (recomp_on_room_draw: the game's room loop, before any of its objects; a game build with the
//     mod's own camera.roomDraw mark) every object the frame draws in front of the mirror wall whose reflection the
//     glass can show is drawn once more, its world matrix mirrored about the mirror plane (the glass, z = 13025), with
//     the game's own functions (reo_guest_call: the world matrix and material slots, the render states, the model) and
//     clipped to the glass panes on the screen (the game's own scissor routine: GS SCISSOR_1 around the part of the
//     panes on the screen, the game's value back right after). The walls and frames drawn after it hide it outside the
//     glass, and the glass blends over it as it does over the copy. Whether the glass can show an object's reflection:
//     its model's bounds (read from the model's vertices when the camera first drives in the room), mirrored, seen
//     from the eye through the part of a pane on the screen. What is mounted on the mirror wall (origin less than
//     MIR_WALL_DEPTH in front of the plane: the glass, its frames, the light bars and their plates) is not mirrored:
//     its reflection lies on itself, and parts of it reach into the wall, which mirrored would stand out of the wall
//     next to the glass (measured: the bars' pieces). Neither is the counter top (38): it reaches more than 10 units
//     into the wall, so mirrored it would lie on the real counter top in front of the wall, and its reflection lies
//     below the glass (seen through it only from nearer than about 90 units and higher than 240: never from her eyes);
//   - the copy (14, 15) is in no cut while the camera drives there (the artwork's drop: a cut change never brings it
//     back);
//   - the light bars' glow cards (below) look from her eyes as they look from the fixed camera.
// Characters are not reflected: the game's own mirror never reflected them either. Without the mark (an older game
// build) the copy stays as before. Handed back, everything is the game's again (the drop goes with the artwork).
#define ROOM_DRAW 0x0049EEF0     // the room's draw data, the room loop's a0 (OBJ_COUNT, OBJ_FLAGS, OBJ_MATRIX in it)
#define ROOM_GROUPS 0x003        // u8: its object groups
#define ROOM_GROUP_COUNT 0x004   // u8 per group: its objects
#define ROOM_MATERIAL 0x068      // u16 per material index: the number of its material record
#define ROOM_MODELS 0x868        // u32: the model table, a 0x98-byte entry per object (MODEL_*)
#define ROOM_OFFSET 0x880        // f32 x3: added to every object's origin
#define ROOM_RSTATES 0x920       // u32 per object: the render states the game hands FN_RSTATES
#define ROOM_MATRIX 0xB20        // f32 4x4 per object (= OBJ_MATRIX)
#define ROOM_GROUP_FIRST 0x2B20  // u8 per group: its first object
#define ROOM_MAT_SEL 0x2C70      // 4 words: bit i set = object i's matrix is the one ROOM_MAT_PTR + 4 i points at
#define ROOM_MAT_PTR 0x2C80
#define MODEL_STRIDE 0x98        // a model entry: +0 the model handle (0 or -1: none), +4 its material count, +8 their
#define MODEL_COUNT 0x04         // indices (u32 each), +0x88 u8: the model has render states (FN_RSTATES hands the
#define MODEL_INDEX 0x08         // game's slots its bytes up to +0x8F; +0x8F: the depth write, 1 = on)
#define MODEL_HAS_RSTATES 0x88
#define MODEL_ZWRITE 0x8F
#define MODEL_RECORDS 0x002941A0 // u32: the model records, 0x180 bytes each, the handle's at (handle - 1) * 0x180:
#define MODEL_REC_STRIDE 0x180   // +0x20 its packet (a DMA "ret" chain per material: VIF codes, UNPACK V4-32 blocks
#define MREC_PACKET 0x20         // with a GIF tag of ST, RGBAQ, XYZF2 and its vertices, 4 qwords each, the position
#define MREC_CHAINS_END 0x2C     // first), +0x2C the chains' size, +0x40 its vertices
#define MREC_VERTICES 0x40
#define MATERIALS 0x004EDD90     // the material records, 0x4C bytes each
#define FN_SET_SLOT 0x001AF780   // (slot, data): slot 0x1A = the world matrix, 0x3A + k = material k
#define FN_RSTATES 0x00374B20    // (model entry, render states)
#define FN_DRAW 0x0018C170       // (handle, 0): the model drawn with the slots set; returns 1
#define FN_SCISSOR 0x00197490    // (x, y, width, height, $t0 = 0): GS SCISSOR_1 from here on (a packet of the frame's
                                 // list; the value kept at SCISSOR_NOW); returns 1
#define SCISSOR_NOW 0x0029FDF8   // u32 x0, y0, x1, y1: SCISSOR_1 as the game set it last
#define SCREEN_H 0x0029FED0      // u32: the frame's height (448) and width (512)
#define SCREEN_W 0x0029FED8
#define VIEW_MATRIX 0x003240E0   // f32 4x4 (rows): the frame's view; x right, y up, the camera looks along -z
#define PROJ_ASPECT 0x0038CA78   // f32: the projection's aspect ratio (4:3; wider with Aspect Ratio EXPAND)
#define MIR_OBJECTS 98           // the restroom: 98 objects, the glass and the copy where the game puts them
#define MIR_COPY 14
#define MIR_POSTERS 15
#define MIR_COUNTER 38           // the counter top
#define MIR_GLASS 46
#define MIR_PLANE 13025.0f       // the glass's plane: z (its normal +z, the room on the +z side)
#define MIR_WALL_DEPTH 12.0f     // objects whose origin is nearer the plane are on the mirror wall (not mirrored)
#define MIR_PANE_Y0 180.2f       // the two panes (the glass model's corners, world units; x: kMirPaneX)
#define MIR_PANE_Y1 241.8f
#define MIR_MARGIN 2.0f          // GS pixels around the panes' outline on the screen (the frames meet the glass there)
#define MIR_CULL_MARGIN 2.0f     // world units around a pane's part on the screen: a reflection seen there is drawn
#define MIR_NEAR 1.0f            // the projection's near plane
#define MIR_MAX_CALLS 3000       // calls of the game's code in one room draw (the host makes at most 4096)
#define MIR_BOUNDS_STEP 1500     // model vertices read for the bounds in one frame (the rest in the next frames)
static const f32 kMirPaneX[4] = {9684.7f, 9734.4f, 9850.0f, 9899.7f};
static struct {
    u32 marked;                  // the game build raises recomp_on_room_draw (seen once)
    u32 calls;
    f32 px0[2], py0[2], px1[2], py1[2]; // per pane: the part of it on the screen, on the plane (world x, y)
    u32 seen[2];                 // the pane is on the screen
} sMir;
// The objects' bounds: per object its model's vertices' box (model space), read once per room (the model table), and
// that box in the world (for the matrix whose origin it was made with).
static struct {
    u32 table;                   // the model table they were read from (0: none)
    u32 next;                    // the next object to read
    u32 ok[CUT_MASK_WORDS];      // bit i: object i's box is known
    u32 inWorld[CUT_MASK_WORDS]; // bit i: object i's world box is made
    f32 box[MIR_OBJECTS][6];     // x0, y0, z0, x1, y1, z1
    f32 world[MIR_OBJECTS][6];
    f32 at[MIR_OBJECTS][3];      // the origin (+ the room's offset) the world box was made with
} sBox;
// The frame's plan: per object the panes (bit k: pane k) whose part on the screen the place of its reflection on the
// plane meets (both: no box yet); 0 = not drawn.
static u8 sPlanPanes[MIR_OBJECTS];
// The material slots as this room draw set them last (a slot holding the material is not set again).
static u32 sSlotMat[32];

static u32 near_at(u32 obj, f32 x, f32 y, f32 z) {
    V3 d = sub(rdv(OBJ_MATRIX + 0x40 * obj + 0x30), v3(x, y, z));
    return sane3(d) && dot(d, d) < 0.25f;
}

// J's Bar's restroom: 98 objects, the glass and the copy at their places.
static u32 mirror_room(void) {
    return rd16(OBJ_COUNT) == MIR_OBJECTS && near_at(MIR_GLASS, 9792.24f, 211.01f, MIR_PLANE) &&
           near_at(MIR_COPY, 9490.0f, 250.0f, 12687.5f);
}

// The artwork's drop: in the restroom the copy, when the reflection is drawn.
static void mirror_drop(u32* drop) {
    u32 w;
    for (w = 0; w < CUT_MASK_WORDS; w++) {
        drop[w] = 0;
    }
    if (!mirror_room() || !sMir.marked) {
        return;
    }
    drop[MIR_COPY >> 5] |= 1u << (MIR_COPY & 31);
    drop[MIR_POSTERS >> 5] |= 1u << (MIR_POSTERS & 31);
}

// ---- The light bars' glow cards -------------------------------------------------------------------------------------
// Each bulb of the restroom's two light bars has a glow card (63, 66, ..., 84): a glowing fin across the wall next to
// its bulb, drawn after its bulb and before the next bulb, writing depth. The game's fixed camera sees the bars from
// their right end: every card from its right, the bulbs drawn after a card in front of it. From the left of a card
// the bulbs drawn after it lie behind its fin, which hid them (transparent, but writing depth): a flat glowing board
// over the bar's next bulbs. While this camera drives there, a card it sees from its left is drawn without writing
// depth (its model entry's depth-write byte, which the game's render states hand on), so the bulbs behind it show
// through its glow as the fixed camera shows them; a card seen from its right is drawn as the game draws it (its fin
// still hides its own small inner card: the fixed camera's glow). Handed back, the bytes are the game's again.
#define GLOW_CARDS 8
static const u8 kGlowCard[GLOW_CARDS] = {63, 66, 69, 72, 75, 78, 81, 84};
static struct {
    u32 on;
    u32 table;                   // the model table the bytes were written in
    u8 orig[GLOW_CARDS];         // the game's depth-write bytes
    u8 mine[GLOW_CARDS];         // as written
} sGlow;

static u32 in_ram(u32 a, u32 n) {
    return a >= 0x00100000u && a < 0x02000000u && n <= 0x02000000u - a;
}

static void glow_restore(void) {
    u32 k, a;
    if (!sGlow.on) {
        return;
    }
    if (mirror_room() && rd32(ROOM_DRAW + ROOM_MODELS) == sGlow.table) {
        for (k = 0; k < GLOW_CARDS; k++) {
            a = sGlow.table + MODEL_STRIDE * kGlowCard[k] + MODEL_ZWRITE;
            if (rd8(a) == sGlow.mine[k] && sGlow.mine[k] != sGlow.orig[k]) {
                wr8(a, sGlow.orig[k]);
            }
        }
    }
    sGlow.on = 0;
}

static void glow_update(void) {
    u32 table = rd32(ROOM_DRAW + ROOM_MODELS), k, a, want;
    V3 eye = rdv(CAM_EYE), p;
    if (sGlow.on && (table != sGlow.table || !mirror_room())) {
        glow_restore();
    }
    if (!mirror_room() || !in_ram(table, MODEL_STRIDE * MIR_OBJECTS) || !sane3(eye)) {
        return;
    }
    if (!sGlow.on) {
        for (k = 0; k < GLOW_CARDS; k++) {
            p = rdv(OBJ_MATRIX + 0x40 * kGlowCard[k] + 0x30);
            if (!sane3(p) || p.y < 274.5f || p.y > 275.5f || p.z < 13033.0f || p.z > 13034.0f) {
                return;  // (not the cards where the game puts them)
            }
        }
        for (k = 0; k < GLOW_CARDS; k++) {
            sGlow.orig[k] = (u8)rd8(table + MODEL_STRIDE * kGlowCard[k] + MODEL_ZWRITE);
            sGlow.mine[k] = sGlow.orig[k];
        }
        sGlow.table = table;
        sGlow.on = 1;
    }
    for (k = 0; k < GLOW_CARDS; k++) {
        a = table + MODEL_STRIDE * kGlowCard[k] + MODEL_ZWRITE;
        p = rdv(OBJ_MATRIX + 0x40 * kGlowCard[k] + 0x30);
        want = (sane(p.x) && eye.x < p.x + rdf(ROOM_DRAW + ROOM_OFFSET)) ? 0u : sGlow.orig[k];
        if (rd8(a) != want) {
            wr8(a, want);
        }
        sGlow.mine[k] = (u8)want;
    }
}

// ---- The objects' bounds --------------------------------------------------------------------------------------------
// A model's box from its packet's vertices (model space); 0 when the packet is not as expected or its vertices are not
// all found (the object is then always drawn).
static u32 model_box(u32 handle, f32* box) {
    u32 rec = rd32(MODEL_RECORDS) + MODEL_REC_STRIDE * (handle - 1), d, end, a, n = 0, want, k;
    if (!in_ram(rec, MODEL_REC_STRIDE)) {
        return 0;
    }
    d = rd32(rec + MREC_PACKET);
    end = rd32(rec + MREC_CHAINS_END);
    want = rd32(rec + MREC_VERTICES);
    if ((d & 15u) || end == 0 || end > 0x100000u || !in_ram(d, end)) {
        return 0;
    }
    end += d;
    for (k = 0; k < 3; k++) {
        box[k] = 1e9f;
        box[3 + k] = -1e9f;
    }
    for (a = d; a < end;) {
        u32 tag = rd32(a), b = a + 8, stop = a + 16 + 16 * (tag & 0xFFFFu);
        if (((tag >> 28) & 7u) != 6u || stop > end) {
            return 0;
        }
        while (b < stop) {
            u32 w = rd32(b), cmd = (w >> 24) & 0x7Fu;
            b += 4;
            if (cmd >= 0x60u) {
                u32 vn = (cmd >> 2) & 3u, vl = cmd & 3u, num = (w >> 16) & 0xFFu, q, qend;
                num = num ? num : 256u;
                qend = b + ((32u >> vl) * (vn + 1u) * num + 31u) / 32u * 4u;
                if ((cmd & 0x6Fu) == 0x6Cu) {  // V4-32: a GIF tag of the three registers, then its vertices
                    for (q = b; q + 16u <= qend && q + 16u <= stop;) {
                        if ((rd32(q + 4) >> 28) == 3u && rd32(q + 8) == 0x412u) {
                            u32 loop = rd32(q) & 0x7FFFu, v;
                            q += 16;
                            for (v = 0; v < loop && q + 64u <= qend; v++, q += 64) {
                                f32 x = rdf(q), y = rdf(q + 4), z = rdf(q + 8);
                                box[0] = x < box[0] ? x : box[0];
                                box[1] = y < box[1] ? y : box[1];
                                box[2] = z < box[2] ? z : box[2];
                                box[3] = x > box[3] ? x : box[3];
                                box[4] = y > box[4] ? y : box[4];
                                box[5] = z > box[5] ? z : box[5];
                            }
                            n += v;
                        } else {
                            q += 16;
                        }
                    }
                }
                b = qend;
            } else if (cmd == 0x20u) {
                b += 4;  // (STMASK)
            } else if (cmd == 0x30u || cmd == 0x31u) {
                b += 16;  // (STROW, STCOL)
            } else if (cmd > 0x17u || (cmd > 0x07u && cmd < 0x10u) || cmd == 0x12u || cmd == 0x16u) {
                return 0;  // (DIRECT, MPG and the unknown: not in these models)
            }
        }
        a = stop;
    }
    // (a NaN or a far vertex makes the box unusable: the object is then always drawn)
    return n != 0 && n == want && sane(box[0]) && sane(box[1]) && sane(box[2]) && sane(box[3]) && sane(box[4]) &&
           sane(box[5]);
}

// The next objects' boxes (at most MIR_BOUNDS_STEP vertices a frame), once per room.
static void bounds_step(void) {
    u32 table = rd32(ROOM_DRAW + ROOM_MODELS), used = 0, i, handle, w;
    if (table != sBox.table) {
        sBox.table = table;
        sBox.next = 0;
        for (w = 0; w < CUT_MASK_WORDS; w++) {
            sBox.ok[w] = 0;
            sBox.inWorld[w] = 0;
        }
    }
    if (!in_ram(table, MODEL_STRIDE * MIR_OBJECTS)) {
        sBox.next = MIR_OBJECTS;
        return;
    }
    while (sBox.next < MIR_OBJECTS && used < MIR_BOUNDS_STEP) {
        i = sBox.next++;
        handle = rd32(table + MODEL_STRIDE * i);
        if (handle == 0 || handle == 0xFFFFFFFFu || handle > 0x10000u) {
            continue;
        }
        if (model_box(handle, sBox.box[i])) {
            sBox.ok[i >> 5] |= 1u << (i & 31);
        }
        used += rd32(rd32(MODEL_RECORDS) + MODEL_REC_STRIDE * (handle - 1) + MREC_VERTICES);
    }
}

static u32 game_call(u32 fn, u32 a0, u32 a1, u32 a2, u32 a3, u32 t0, u32 count) {
    u32 w[5];
    w[0] = a0;
    w[1] = a1;
    w[2] = a2;
    w[3] = a3;
    w[4] = t0;
    sMir.calls++;
    return reo_guest_call(fn, w, count);
}

// The part of a pane on the screen: its corners in the view (v' = p V, rows), the part in front of the near plane onto
// the frame, that outline cut to the frame. 1 when some of the pane is on the screen (its outline's box on the frame
// then in o: x0, y0, x1, y1).
static u32 mirror_pane(const f32* m, f32 x0, f32 x1, f32 kx, f32 ky, f32 w, f32 h, f32* o) {
    f32 pa[2][20];
    V3 c[4], a, b, q;
    u32 i, n = 0, e, k, cnt, src = 0;
    f32 t;
    for (i = 0; i < 4; i++) {
        f32 x = (i == 1 || i == 2) ? x1 : x0, y = i >= 2 ? MIR_PANE_Y1 : MIR_PANE_Y0, z = MIR_PLANE;
        c[i] = v3(x * m[0] + y * m[4] + z * m[8] + m[12], x * m[1] + y * m[5] + z * m[9] + m[13],
                  x * m[2] + y * m[6] + z * m[10] + m[14]);
    }
    // In front of the near plane, onto the frame (a convex outline of at most 5 corners; each cut adds at most one).
    for (i = 0; i < 4; i++) {
        a = c[i];
        b = c[(i + 1) & 3u];
        for (k = 0; k < 2; k++) {
            if (k == 0 && !(a.z <= -MIR_NEAR)) {
                continue;
            }
            if (k == 1) {
                if ((a.z <= -MIR_NEAR) == (b.z <= -MIR_NEAR)) {
                    continue;
                }
                t = (-MIR_NEAR - a.z) / (b.z - a.z);
                q = lerp(a, b, t);
            } else {
                q = a;
            }
            pa[0][2 * n] = w * 0.5f * (1.0f + kx * q.x / -q.z);
            pa[0][2 * n + 1] = h * 0.5f * (1.0f - ky * q.y / -q.z);
            if (!sane(pa[0][2 * n]) || !sane(pa[0][2 * n + 1])) {
                return 0;
            }
            n++;
        }
    }
    // Cut to the frame: x >= 0, y >= 0, x <= w, y <= h.
    for (e = 0; e < 4 && n >= 3; e++) {
        f32* in = pa[src];
        f32* out = pa[src ^ 1u];
        u32 axis = e & 1u;
        f32 lim = e < 2 ? 0.0f : (axis ? h : w), sgn = e < 2 ? 1.0f : -1.0f;
        cnt = 0;
        for (i = 0; i < n; i++) {
            f32* p0 = in + 2 * i;
            f32* p1 = in + 2 * ((i + 1) % n);
            f32 d0 = sgn * (p0[axis] - lim), d1 = sgn * (p1[axis] - lim);
            if (d0 >= 0.0f && cnt < 10) {
                out[2 * cnt] = p0[0];
                out[2 * cnt + 1] = p0[1];
                cnt++;
            }
            if ((d0 >= 0.0f) != (d1 >= 0.0f) && cnt < 10) {
                t = d0 / (d0 - d1);
                out[2 * cnt] = p0[0] + (p1[0] - p0[0]) * t;
                out[2 * cnt + 1] = p0[1] + (p1[1] - p0[1]) * t;
                cnt++;
            }
        }
        n = cnt;
        src ^= 1u;
    }
    if (n < 3) {
        return 0;
    }
    o[0] = o[2] = pa[src][0];
    o[1] = o[3] = pa[src][1];
    for (i = 1; i < n; i++) {
        f32 px = pa[src][2 * i], py = pa[src][2 * i + 1];
        o[0] = px < o[0] ? px : o[0];
        o[1] = py < o[1] ? py : o[1];
        o[2] = px > o[2] ? px : o[2];
        o[3] = py > o[3] ? py : o[3];
    }
    return 1;
}

// The part of pane k on the screen, on the plane: the rays from the eye through the corners of its outline on the
// frame (+ the margin) met with the plane, cut to the pane. The whole pane when a ray does not meet the plane.
static void mirror_pane_part(u32 k, const f32* m, V3 eye, const f32* o, f32 kx, f32 ky, f32 w, f32 h) {
    f32 x0 = kMirPaneX[2 * k], x1 = kMirPaneX[2 * k + 1], y0 = MIR_PANE_Y0, y1 = MIR_PANE_Y1;
    f32 lx = 1e9f, ly = 1e9f, hx = -1e9f, hy = -1e9f;
    u32 i;
    for (i = 0; i < 4; i++) {
        f32 sx = (i & 1u) ? o[2] + MIR_MARGIN : o[0] - MIR_MARGIN;
        f32 sy = (i & 2u) ? o[3] + MIR_MARGIN : o[1] - MIR_MARGIN;
        f32 vx = (2.0f * sx / w - 1.0f) / kx, vy = (1.0f - 2.0f * sy / h) / ky, t;
        V3 d = v3(vx * m[0] + vy * m[1] - m[2], vx * m[4] + vy * m[5] - m[6], vx * m[8] + vy * m[9] - m[10]);
        if (!(d.z < -1e-4f)) {
            lx = x0;
            ly = y0;
            hx = x1;
            hy = y1;
            break;
        }
        t = (MIR_PLANE - eye.z) / d.z;
        lx = eye.x + d.x * t < lx ? eye.x + d.x * t : lx;
        hx = eye.x + d.x * t > hx ? eye.x + d.x * t : hx;
        ly = eye.y + d.y * t < ly ? eye.y + d.y * t : ly;
        hy = eye.y + d.y * t > hy ? eye.y + d.y * t : hy;
    }
    sMir.px0[k] = lx > x0 ? lx : x0;
    sMir.px1[k] = hx < x1 ? hx : x1;
    sMir.py0[k] = ly > y0 ? ly : y0;
    sMir.py1[k] = hy < y1 ? hy : y1;
    sMir.seen[k] = sMir.px0[k] <= sMir.px1[k] && sMir.py0[k] <= sMir.py1[k];
}

// Where object i's world matrix is (the room's own, or the one its pointer gives).
static u32 obj_matrix(u32 i) {
    u32 src = ROOM_DRAW + ROOM_MATRIX + 0x40 * i;
    if ((rd32(ROOM_DRAW + ROOM_MAT_SEL + 4 * (i >> 5)) >> (i & 31)) & 1u) {
        u32 at = rd32(ROOM_DRAW + ROOM_MAT_PTR + 4 * i);
        src = at ? at : src;
    }
    return src;
}

// Object i's box in the world: its centre and half extents through the matrix's rows (at src), its origin o.
static u32 world_box(u32 i, u32 src, V3 o) {
    const f32* b = sBox.box[i];
    f32* w = sBox.world[i];
    f32 c[3], e[3];
    u32 j, k;
    c[0] = o.x;
    c[1] = o.y;
    c[2] = o.z;
    e[0] = e[1] = e[2] = 0.0f;
    for (k = 0; k < 3; k++) {
        f32 bc = 0.5f * (b[k] + b[3 + k]), be = 0.5f * (b[3 + k] - b[k]);
        for (j = 0; j < 3; j++) {
            f32 a = rdf(src + 16 * k + 4 * j);
            c[j] += bc * a;
            e[j] += be * (a < 0.0f ? -a : a);
        }
    }
    for (j = 0; j < 3; j++) {
        w[j] = c[j] - e[j];
        w[3 + j] = c[j] + e[j];
        if (!sane(w[j]) || !sane(w[3 + j])) {
            return 0;
        }
    }
    sBox.at[i][0] = o.x;
    sBox.at[i][1] = o.y;
    sBox.at[i][2] = o.z;
    sBox.inWorld[i >> 5] |= 1u << (i & 31);
    return 1;
}

// The frame's plan for the group's objects: the ones the frame draws in front of the mirror wall (not the counter
// top), each with the panes whose part on the screen the place of its reflection on the plane meets: the part of its
// world box in front of the plane, mirrored, seen from the eye (the rays of its corners met with the plane).
static void mirror_plan(u32 first, u32 count, V3 eye) {
    V3 off = rdv(ROOM_DRAW + ROOM_OFFSET), o;
    u32 table = rd32(ROOM_DRAW + ROOM_MODELS), i, k, f, src, bit;
    for (i = first; i < first + count && i < MIR_OBJECTS; i++) {
        const f32* w = sBox.world[i];
        f32 zn, tf, tn, a, b, r[4];
        sPlanPanes[i] = 0;
        f = rd8(OBJ_FLAGS + i);
        if (!(f & 1u) || (f & 0x80u) || i == MIR_COUNTER) {
            continue;
        }
        src = obj_matrix(i);
        o = add(rdv(src + 0x30), off);
        if (!(o.z >= MIR_PLANE + MIR_WALL_DEPTH) || !sane3(o)) {
            continue;  // (behind the glass - the copy - or on the mirror wall)
        }
        sPlanPanes[i] = 3;
        bit = 1u << (i & 31);
        if (table != sBox.table || !(sBox.ok[i >> 5] & bit)) {
            continue;  // (no box yet: drawn)
        }
        if ((!(sBox.inWorld[i >> 5] & bit) || o.x != sBox.at[i][0] || o.y != sBox.at[i][1] || o.z != sBox.at[i][2]) &&
            !world_box(i, src, o)) {
            continue;
        }
        sPlanPanes[i] = 0;
        if (w[5] <= MIR_PLANE) {
            continue;  // (all of it behind the plane)
        }
        // Mirrored: its far side at 2 plane - z1, its near side at 2 plane - max(z0, plane); t of the eye's ray there.
        zn = w[2] > MIR_PLANE ? w[2] : MIR_PLANE;
        tf = (MIR_PLANE - eye.z) / (2.0f * MIR_PLANE - w[5] - eye.z);
        tn = (MIR_PLANE - eye.z) / (2.0f * MIR_PLANE - zn - eye.z);
        a = eye.x + tf * (w[0] - eye.x);
        b = eye.x + tn * (w[0] - eye.x);
        r[0] = a < b ? a : b;
        a = eye.y + tf * (w[1] - eye.y);
        b = eye.y + tn * (w[1] - eye.y);
        r[1] = a < b ? a : b;
        a = eye.x + tf * (w[3] - eye.x);
        b = eye.x + tn * (w[3] - eye.x);
        r[2] = a > b ? a : b;
        a = eye.y + tf * (w[4] - eye.y);
        b = eye.y + tn * (w[4] - eye.y);
        r[3] = a > b ? a : b;
        for (k = 0; k < 2; k++) {
            if (sMir.seen[k] && r[2] >= sMir.px0[k] - MIR_CULL_MARGIN && r[0] <= sMir.px1[k] + MIR_CULL_MARGIN &&
                r[3] >= sMir.py0[k] - MIR_CULL_MARGIN && r[1] <= sMir.py1[k] + MIR_CULL_MARGIN) {
                sPlanPanes[i] |= (u8)(1u << k);
            }
        }
    }
}

// Object i drawn mirrored about the glass's plane, as the game's room loop draws it.
static void mirror_object(u32 i) {
    u32 entry = rd32(ROOM_DRAW + ROOM_MODELS) + MODEL_STRIDE * i, handle = rd32(entry), src, n, k, scratch;
    u32 m[16];
    V3 o;
    if (handle == 0 || handle == 0xFFFFFFFFu) {
        return;
    }
    src = obj_matrix(i);
    for (k = 0; k < 16; k++) {
        m[k] = rd32(src + 4 * k);
    }
    // Its origin + the room's offset, then z mirrored: the axes' z negated, the origin's z to 2 plane - z.
    o = add(rdv(src + 0x30), rdv(ROOM_DRAW + ROOM_OFFSET));
    m[12] = bitsf(o.x);
    m[13] = bitsf(o.y);
    m[14] = bitsf(2.0f * MIR_PLANE - o.z);
    m[2] ^= 0x80000000u;
    m[6] ^= 0x80000000u;
    m[10] ^= 0x80000000u;
    scratch = reo_guest_scratch(m, 16);
    if (!scratch) {
        return;
    }
    game_call(FN_SET_SLOT, 0x1A, scratch, 0, 0, 0, 2);
    if (rd8(entry + MODEL_HAS_RSTATES)) {
        game_call(FN_RSTATES, entry, ROOM_DRAW + ROOM_RSTATES + 4 * i, 0, 0, 0, 2);
    }
    n = rd32(entry + MODEL_COUNT);
    n = n > 32 ? 32 : n;
    for (k = 0; k < n; k++) {
        u32 mat = MATERIALS + 0x4C * rd16(ROOM_DRAW + ROOM_MATERIAL + 2 * rd32(entry + MODEL_INDEX + 4 * k));
        if (sSlotMat[k] != mat) {
            game_call(FN_SET_SLOT, 0x3A + k, mat, 0, 0, 0, 2);
            sSlotMat[k] = mat;
        }
    }
    game_call(FN_DRAW, handle, 0, 0, 0, 0, 2);
}

// A box on the frame (x0, y0, x1, y1) + the margin, cut to the frame: the scissor rectangle r (x0, y0, x1, y1); its
// area (0: none).
static u32 mirror_rect(const f32* o, u32 w, u32 h, u32* r) {
    r[0] = o[0] - MIR_MARGIN <= 0.0f ? 0 : (u32)(o[0] - MIR_MARGIN);
    r[1] = o[1] - MIR_MARGIN <= 0.0f ? 0 : (u32)(o[1] - MIR_MARGIN);
    r[2] = o[2] + MIR_MARGIN >= (f32)(w - 1) ? w - 1 : (u32)(o[2] + MIR_MARGIN);
    r[3] = o[3] + MIR_MARGIN >= (f32)(h - 1) ? h - 1 : (u32)(o[3] + MIR_MARGIN);
    return r[2] < r[0] || r[3] < r[1] ? 0 : (r[2] - r[0] + 1) * (r[3] - r[1] + 1);
}

// One pass of the reflection: the scissor rectangle r, the objects the panes of `panes` can show.
static void mirror_pass(u32 first, u32 count, const u32* r, u32 panes) {
    u32 i;
    if (game_call(FN_SCISSOR, r[0], r[1], r[2] - r[0] + 1, r[3] - r[1] + 1, 0, 5) != 1) {
        return;  // (refused: not the camera that drives the camera)
    }
    for (i = first; i < first + count && i < MIR_OBJECTS && sMir.calls < MIR_MAX_CALLS; i++) {
        if (sPlanPanes[i] & panes) {
            mirror_object(i);
        }
    }
}

// The reflection of the room, at the room's draw (before the group's objects). The GS renderers fill a primitive's
// box within the scissor rectangle: each pane on the screen has a pass with its own rectangle, unless one rectangle
// around both is smaller or the two meet.
static void mirror_draw(u32 group) {
    u32 w, h, k, area[2], both, r[3][4], first, count;
    u32 old[4];
    f32 m[16], o[3][4], fov, aspect, kx, ky, half;
    V3 eye;
    if (group >= rd8(ROOM_DRAW + ROOM_GROUPS)) {
        return;
    }
    // The copy is not drawn (the artwork's drop): never the copy and the reflection together.
    if ((rd8(OBJ_FLAGS + MIR_COPY) & 1u) || (rd8(OBJ_FLAGS + MIR_POSTERS) & 1u)) {
        return;
    }
    // The panes on the screen: the frame's view and its projection (the vertical field of view, the aspect ratio).
    for (k = 0; k < 16; k++) {
        m[k] = rdf(VIEW_MATRIX + 4 * k);
        if (!sane(m[k])) {
            return;
        }
    }
    // The frame's eye (v = p V: p = -t R^T).
    eye = v3(-(m[12] * m[0] + m[13] * m[1] + m[14] * m[2]), -(m[12] * m[4] + m[13] * m[5] + m[14] * m[6]),
             -(m[12] * m[8] + m[13] * m[9] + m[14] * m[10]));
    if (!sane3(eye) || !(eye.z > MIR_PLANE + 1.0f)) {
        return;
    }
    fov = rdf(CAM_FOV);
    aspect = rdf(PROJ_ASPECT);
    w = rd32(SCREEN_W);
    h = rd32(SCREEN_H);
    if (!sane(fov) || fov < 1.0f || fov > 170.0f || !sane(aspect) || aspect < 0.5f || aspect > 5.0f || w < 64 ||
        w > 2048 || h < 64 || h > 2048) {
        return;
    }
    half = fov * 0.5f * 0.017453293f;
    ky = fp_cos(half) / fp_sin(half);
    kx = ky / aspect;
    for (k = 0; k < 2; k++) {
        sMir.seen[k] = mirror_pane(m, kMirPaneX[2 * k], kMirPaneX[2 * k + 1], kx, ky, (f32)w, (f32)h, o[k]);
        area[k] = sMir.seen[k] ? mirror_rect(o[k], w, h, r[k]) : 0;
        if (area[k]) {
            mirror_pane_part(k, m, eye, o[k], kx, ky, (f32)w, (f32)h);
        }
        sMir.seen[k] = area[k] && sMir.seen[k];
    }
    if (!sMir.seen[0] && !sMir.seen[1]) {
        return;  // (the glass is not on the screen)
    }
    both = 0;
    if (sMir.seen[0] && sMir.seen[1]) {
        o[2][0] = o[0][0] < o[1][0] ? o[0][0] : o[1][0];
        o[2][1] = o[0][1] < o[1][1] ? o[0][1] : o[1][1];
        o[2][2] = o[0][2] > o[1][2] ? o[0][2] : o[1][2];
        o[2][3] = o[0][3] > o[1][3] ? o[0][3] : o[1][3];
        // (two rectangles that meet would draw what both panes show twice there: one pass then)
        both = mirror_rect(o[2], w, h, r[2]) <= area[0] + area[1] ||
               (r[0][0] <= r[1][2] && r[1][0] <= r[0][2] && r[0][1] <= r[1][3] && r[1][1] <= r[0][3]);
    }
    for (k = 0; k < 4; k++) {
        old[k] = rd32(SCISSOR_NOW + 4 * k);
    }
    if (old[2] < old[0] || old[3] < old[1] || old[2] >= 2048 || old[3] >= 2048) {
        return;
    }
    first = rd8(ROOM_DRAW + ROOM_GROUP_FIRST + group);
    count = rd8(ROOM_DRAW + ROOM_GROUP_COUNT + group);
    mirror_plan(first, count, eye);
    sMir.calls = 0;
    for (k = 0; k < 32; k++) {
        sSlotMat[k] = 0;  // (the room loop sets its own; this draw's are set at least once)
    }
    if (both) {
        mirror_pass(first, count, r[2], 3u);
    } else {
        for (k = 0; k < 2; k++) {
            if (sMir.seen[k]) {
                mirror_pass(first, count, r[k], 1u << k);
            }
        }
    }
    game_call(FN_SCISSOR, old[0], old[1], old[2] - old[0] + 1, old[3] - old[1] + 1, 0, 5);
}

static void reset_view(void) {
    s.trY.have = 0;
    s.trZ.have = 0;
    s.trT.have = 0;
    s.trF.have = 0;
    s.haveLast = 0;
    s.taken = 0;
    s.aimBlend = 0.0f;
    s.sightBlend = 0.0f;
    s.haveSight = 0;
    s.holding = 0;
    s.havePrev = 0;
    s.haveOut = 0;
    s.plan = 0;
    s.planWas = 0;
    s.padMag = 0;
    s.haveView = 0;
    s.aimTurn = 0;
    s.aimApplied = 0;
    s.wasRaised = 0;
}

// Everything back to the game: its camera writes, its hide radius, no crosshair, its right stick.
static void hand_back(u32 restoreCamera) {
    u32 was = s.driving;
    freeze(0);
    if (was && restoreCamera) {
        restore_room_camera();
    }
    restore_hide();
    restore_artwork();
    crosshair(0);
    pad_centre(0);
    s.driving = 0;
    reset_view();
}

static u32 node_ok(u32 p) {
    return p >= 0x00100000u && p < 0x02000000u && (p & 15u) == 0 && rd32(p + 0x3C) == 0x3F800000u;
}

static u32 node_table(u32 rec) {
    u32 model = rd32(rec + REC_MODEL), table;
    if (model < 0x00100000u || model >= 0x02000000u || (model & 3u)) {
        return 0;
    }
    table = rd32(model);
    if (table < 0x00100000u || table >= 0x02000000u || (table & 3u)) {
        return 0;
    }
    return table;
}

// The head bone, checked: valid matrices, and her eye point a grandchild of it.
static u32 head_node(u32 table) {
    u32 head = rd32(table + 4 * NODE_HEAD), eye = rd32(table + 4 * NODE_EYE), parent;
    if (!node_ok(head) || !node_ok(eye)) {
        return 0;
    }
    parent = rd32(eye + 0xC8);
    if (!node_ok(parent) || rd32(parent + 0xC8) != head) {
        return 0;
    }
    return head;
}

// The right hand's weapon node, checked: a child of the right wrist.
static u32 hand_node(u32 table) {
    u32 hand = rd32(table + 4 * NODE_HAND), wrist = rd32(table + 4 * NODE_WRIST);
    if (!node_ok(hand) || !node_ok(wrist) || rd32(hand + 0xC8) != wrist) {
        return 0;
    }
    return hand;
}

static const FpSight* sight_of(u32 item) {
    u32 i;
    for (i = 0; i < FP_SIGHTS; i++) {
        if (kSights[i].item == item) {
            return &kSights[i];
        }
    }
    return 0;
}

// One tuning value, clamped to its range (a value that is not a number: the default).
static f32 tune_value(const char* key, f32 lo, f32 hi, f32 fallback) {
    f32 v = (f32)recomp_get_config_double(key);
    return sane(v) ? clampf(v, lo, hi) : fallback;
}

// A view's five tuning values; logged when they change.
static void read_tune(FpTune* t, u32 view) {
    const char* const* k = kTuneKeys[view];
    const FpTune* d = &kTuneDefaults[view];
    t->fwd = tune_value(k[0], -FP_OFFSET_MAX, FP_OFFSET_MAX, d->fwd);
    t->up = tune_value(k[1], -FP_OFFSET_MAX, FP_OFFSET_MAX, d->up);
    t->right = tune_value(k[2], -FP_OFFSET_MAX, FP_OFFSET_MAX, d->right);
    t->look = tune_value(k[3], -FP_LOOK_MAX, FP_LOOK_MAX, d->look);
    t->fov = tune_value(k[4], FP_FOV_MIN, FP_FOV_MAX, d->fov);
    if (!s.haveTune || t->fwd != s.tune[view].fwd || t->up != s.tune[view].up || t->right != s.tune[view].right ||
        t->look != s.tune[view].look || t->fov != s.tune[view].fov) {
        s.tune[view] = *t;
        log_tune(view, t);
    }
}

static u32 free_to_turn(u32 rec);

// Where the pad mapper sets her heading while she is free to turn (free_to_turn): with the left stick pushed (mag, dir:
// the mapped pad record's) in front of the view (FP_PAD_FRONT) or with the dash held (dash), the way her walking input
// walks her by the camera's yaw step cam16 (steer: the game is to walk her the way she faces); further back without the
// dash, the view's yaw while she stands or steps back (she keeps facing it; the game steps her back); with the stick at
// rest, the view's yaw while she stands. 0: it leaves her heading to the game.
static u32 pad_want(u32 rec, s32 mag, u32 dir, u32 dash, u32 cam16, u32* want, u32* steer) {
    u32 kind = rd8(rec + REC_AIMSTATE), stand = rd8(rec + REC_ACTION) == ACTION_STAND;
    s32 rel = (s32)(s16)dir;
    *want = (u32)(s32)(s.viewYaw + 0.5f) & 0xFFFFu;
    *steer = 0;
    if (mag >= PAD_STICK_MOVE && stick_by_camera(rec)) {
        if ((rel <= FP_PAD_FRONT && rel >= -FP_PAD_FRONT) || dash) {
            *want = (cam16 + (dir & 0xFFF0u)) & 0xFFFFu;
            *steer = 1;
            return 1;
        }
        return stand || (kind >= KIND_BACK_FIRST && kind <= KIND_BACK_LAST);
    }
    return stand;
}

static void frame(void) {
    u32 rec = CHAR_BASE, table, head, hand, id, stance, st, i, vbl, item, aimReady, raised, ride, h16, viewWas, room;
    u32 jumped = 0;
    u32 input[12];
    s32 gameTurn, aimCarry;
    f32 dt, step, n, fx, fz, elev, fov, rc, rs, sec, turn, aimYaw, asn, acs;
    V3 pos, fwd, right, hy, hz, ht, hyU, hzU, fH, uH, rH, eyeSpot, headDir, aimDir, normalPos, aimPos, normalDir,
        aimView, camPos, camDir, fwdNow, rightNow, hyNow, hzNow, htNow, shift, aimDirNow, aimFwd, aimRight;
    FpTune tn, ta;
    const FpSight* sight;
    const FpEyes* lock;

    if (!s.init) {
        s.init = 1;
        s.wantFreeze = 2;
        s.status = 0xFFu;
        s.lastVbl = rd32(VBLANK_COUNT);
    }
    vbl = rd32(VBLANK_COUNT);
    dt = (f32)(vbl - s.lastVbl);
    s.lastVbl = vbl;
    dt = clampf(dt, 1.0f, 8.0f);
    step = dt / FP_BLEND_VBLANKS;

    // The On / Off key while playing (the camera input service; a press switches, holding does nothing more), and the
    // right stick (not while a menu of the program takes the input).
    s.stickX = 0.0f;
    s.stickY = 0.0f;
    if (reo_cam_input(input)) {
        u32 held = (input[FP_INPUT_HELD] & FP_ACTION_TOGGLE) && input[FP_INPUT_FOCUSED] && !input[FP_INPUT_UI_OPEN];
        if (held && !s.keyHeld) {
            s.userOff = !s.userOff;
            reo_log(s.userOff ? "[fp] key: off" : "[fp] key: on");
        }
        s.keyHeld = held;
        if (!input[FP_INPUT_UI_OPEN]) {
            s.stickX = stick_axis(input[FP_INPUT_RX]);
            s.stickY = stick_axis(input[FP_INPUT_RY]);
        }
    }
    if (!recomp_get_config_u32("enabled")) {
        s.userOff = 0;  // (the Mods tab's switch decides; the key starts on again)
    }
    if (!recomp_get_config_u32("enabled") || s.userOff) {
        if (s.driving || s.yielded) {
            hand_back(1);
            s.yielded = 0;
            reo_log("[fp] off: the game's camera is back");
        }
        status(FP_STATUS_OFF);
        return;
    }
    // The tuning values (live: a change in Configure or the camera window counts from this frame).
    read_tune(&tn, 0);
    read_tune(&ta, 1);
    s.haveTune = 1;

    // A level: game.bin in memory, her record in use with a position, a skeleton with a head.
    pos = rdv(rec + REC_POS);
    table = 0;
    head = 0;
    if (reo_image_loaded("game.bin") && rd8(rec + REC_ALIVE) && sane3(pos) && (pos.x != 0.0f || pos.z != 0.0f)) {
        table = node_table(rec);
        head = table ? head_node(table) : 0;
    }
    id = rd32(rec + REC_ID) & 0xFFu;
    if (!head || id >= 8) {
        if (s.driving || s.yielded) {
            hand_back(0);
            s.yielded = 0;
            reo_log("[fp] no level: waiting");
        }
        status(FP_STATUS_WAITING);
        return;
    }
    if (rd32(rec + REC_MODEL) != s.model) {
        // Another room or another character: start from her eyes.
        s.model = rd32(rec + REC_MODEL);
        reset_view();
        s.yielded = 0;
    }
    stance = rd8(rec + REC_STANCE);
    raised = stance == 2;  // (a hit, stance 3, is no aim: the game throws her about; the view stays)

    // The game took the camera for a scripted shot: it is the game's until she moves on her own or raises her weapon.
    if (s.yielded) {
        V3 d = sub(pos, s.yieldPos);
        if (d.x * d.x + d.z * d.z < FP_RESUME_DIST2 && stance != 2) {
            status(FP_STATUS_YIELDED);
            return;
        }
        s.yielded = 0;
        reo_log("[fp] she moves again: back in her eyes");
    }
    if (s.driving && s.haveLast) {
        V3 e = sub(rdv(CAM_EYE), s.lastEye), a = sub(rdv(CAM_AT), s.lastAt);
        s.taken = (dot(e, e) > FP_TAKEN_DIST2 || dot(a, a) > FP_TAKEN_DIST2) ? s.taken + 1 : 0;
        if (s.taken >= 2) {
            hand_back(0);
            s.yielded = 1;
            s.yieldPos = pos;
            reo_log("[fp] the game took the camera: handed back");
            status(FP_STATUS_YIELDED);
            return;
        }
    }

    st = freeze(1);
    if (st == FP_FEATURE_UNAVAILABLE || st == FP_FEATURE_UNKNOWN) {
        hand_back(0);
        status(FP_STATUS_UNAVAILABLE);
        return;
    }
    if (!s.driving) {
        s.driving = 1;
        reset_view();
        fp_log("[fp] in her eyes: character", 1, (s32)id, 0);
    }
    pad_centre(1);
    keep_artwork();
    if (sArt.on) {
        glow_update();
        if (sMir.marked && mirror_room()) {
            bounds_step();
        }
    } else {
        glow_restore();
    }

    // Her frame: feet, facing (level), right.
    fx = rdf(rec + REC_FWD_X);
    fz = rdf(rec + REC_FWD_Z);
    n = fp_sqrt(fx * fx + fz * fz);
    if (!(n > 1e-4f) || !sane(n)) {
        fx = 0.0f;
        fz = 1.0f;
    } else {
        fx /= n;
        fz /= n;
    }
    fwdNow = v3(fx, 0.0f, fz);
    rightNow = v3(-fz, 0.0f, fx);

    // Her facing and her head as they will be drawn: the game draws the next frame after it has moved her once more,
    // so each is carried on by its change of the last frame (track). A step longer than FP_PREDICT_JUMP (a door, a new
    // place) starts again from where she is.
    hyNow = rdv(head + 0x10);
    hzNow = rdv(head + 0x20);
    htNow = rdv(head + 0x30);
    if (s.trT.have) {
        V3 j = sub(htNow, s.trT.last);
        if (dot(j, j) > FP_PREDICT_JUMP * FP_PREDICT_JUMP) {
            jumped = 1;
            s.trY.have = 0;
            s.trZ.have = 0;
            s.trT.have = 0;
            s.trF.have = 0;
        }
    }
    fwd = track(&s.trF, fwdNow, FP_PREDICT_GROW_AXIS);
    fwd = unit(v3(fwd.x, 0.0f, fwd.z), fwdNow);
    right = v3(-fwd.z, 0.0f, fwd.x);
    hy = track(&s.trY, hyNow, FP_PREDICT_GROW_AXIS);
    hz = track(&s.trZ, hzNow, FP_PREDICT_GROW_AXIS);
    ht = track(&s.trT, htNow, FP_PREDICT_GROW);

    // The eye spot: the head bone's matrix applied to her fixed point; the view along the head, levelled at rest.
    lock = &kEyes[id];
    rc = lock->restCos;
    rs = lock->restSin;
    eyeSpot = add(ht, add(mul(hy, lock->up), mul(hz, lock->forward)));
    // (how far the eye spot moves until the frame is drawn: the sights below are measured in this frame's skeleton)
    shift = sub(eyeSpot, add(htNow, add(mul(hyNow, lock->up), mul(hzNow, lock->forward))));
    headDir = tilt(unit(hz, fwd), rc, -rs);
    // The tuning offsets' axes: the head's own axes turned by her rest pitch (standing still: forward is level ahead,
    // up is straight up, right is her right), unit length (game units); they move with the head like the eye spot.
    hyU = unit(hy, v3(0.0f, 1.0f, 0.0f));
    hzU = unit(hz, fwd);
    fH = unit(sub(mul(hzU, rc), mul(hyU, rs)), headDir);
    uH = unit(add(mul(hyU, rc), mul(hzU, rs)), v3(0.0f, 1.0f, 0.0f));
    rH = unit(cross(fH, uH), right);
    aimPos = add(eyeSpot, add(mul(fH, ta.fwd), add(mul(uH, ta.up), mul(rH, ta.right))));

    // Where you look. The view starts from her heading when the camera comes into her eyes (a new character, back from
    // a scripted shot) and where a door into another room places her (FP_ROOM_*); then only the right stick turns it
    // left / right and tilts it up / down: the pad mapper turns her with it while she stands (she faces where you
    // look), the left stick's walking never turns it, and when the game turns her itself - in an action of its own (a
    // door, an item, its quick turn, a hit, a grab) or while she stands (the d-pad) - the view stays where it is. While
    // her weapon is raised the view is her aim's: the right stick turns her aim (at the pad mapper) and the view
    // follows her, also when the game turns her (her aiming controls); raising it, she is turned to where you look.
    // Lowering it, or when a hit ends her aim, the view looks on from where the picture looked while she aimed (its
    // pitch too), whatever the game does with her.
    sec = dt * FP_VBLANK_SECONDS;
    h16 = rd16(rec + REC_HEADING);
    turn = -s.stickX * FP_STICK_TURN * sec * FP_YAW_UNITS;  // (a push to the right turns to her right: the yaw falls)
    viewWas = s.haveView;
    gameTurn = yaw_diff(h16, s.lastH) - s.aimApplied;  // (her turn of the last frame the game made itself)
    s.aimApplied = 0;
    room = rd32(ROOM_NOW);
    if (room != s.room) {
        s.room = room;
        s.newRoom = FP_ROOM_LOADING;
    }
    if (s.newRoom && (jumped || (s.newRoom == FP_ROOM_LOADING && (raised || free_to_turn(rec))))) {
        s.haveView = 0;  // (where the game places her in the new room: from her heading there, once)
        s.newRoom = FP_ROOM_PLACED;
    }
    if (s.newRoom == FP_ROOM_PLACED && (raised || free_to_turn(rec))) {
        s.newRoom = 0;
    }
    if (!s.haveView) {
        s.haveView = 1;
        s.viewYaw = (f32)h16;
        s.viewPitch = 0.0f;
    } else if (raised) {
        if (s.wasRaised) {
            s.viewYaw = (f32)h16;  // (her aim as the game has it)
        }
    } else if (s.wasRaised) {
        s.viewYaw = s.aimYaw;
        s.viewPitch = clampf(s.aimPitch - tn.look, -FP_STICK_DOWN_MAX, FP_STICK_UP_MAX);
    }
    s.viewYaw = yaw_wrap(s.viewYaw + turn);
    if (raised) {
        s.aimTurn = yaw_diff((u32)(s32)(s.viewYaw + 0.5f), h16);
    } else {
        s.aimTurn = 0;
        s.viewPitch = clampf(s.viewPitch + s.stickY * FP_STICK_TILT * sec, -FP_STICK_DOWN_MAX, FP_STICK_UP_MAX);
    }
    s.lastH = h16;
    s.wasRaised = raised;

    // Her aim: her facing as it will be drawn, tilted with the gun's elevation. Her facing: her heading now, the right
    // stick's turn the pad mapper writes next (exact), and the game's own turn of the next frame as far as it is known
    // (FP_AIM_CARRY: its auto-aim exactly, else its turn of the last frame when it is slow).
    aimReady = stance == 2 && rd8(rec + REC_AIMSTATE) == 1 && rd8(rec + REC_ACTION) == ACTION_AIM;
    elev = (f32)(s32)(s16)rd16(rec + REC_AIM) * (6.2831853f / 65536.0f);
    elev = clampf(elev, -1.2f, 1.2f);
    aimCarry = 0;
    if (stance == 2 && rd8(rec + REC_AIMSTATE) == 0 && rd8(rec + REC_ACTION) == ACTION_AIM &&
        rd16(rec + REC_AUTOAIM_LEFT) != 0) {
        aimCarry = (s32)(s16)rd16(rec + REC_AUTOAIM_STEP);  // (the game's auto-aim turns her by this next frame)
        aimCarry = aimCarry > FP_AUTOAIM_MAX || aimCarry < -FP_AUTOAIM_MAX ? 0 : aimCarry;
    } else if (viewWas && gameTurn <= FP_AIM_CARRY && gameTurn >= -FP_AIM_CARRY) {
        aimCarry = gameTurn;
    }
    aimYaw = raised ? (f32)h16 + (f32)s.aimTurn + (f32)aimCarry : s.viewYaw;  // (aim ended: the view's, below)
    yaw_sincos(aimYaw, &asn, &acs);
    aimFwd = v3(asn, 0.0f, acs);
    aimRight = v3(-acs, 0.0f, asn);
    aimDir = add(mul(aimFwd, fp_cos(elev)), v3(0.0f, fp_sin(elev), 0.0f));
    aimDirNow = add(mul(fwdNow, fp_cos(elev)), v3(0.0f, fp_sin(elev), 0.0f));
    s.aimBlend = clampf(s.aimBlend + (raised ? step : -step), 0.0f, 1.0f);

    // The sight line: through the weapon's rear-sight spot along her aim (the barrel, the weapon node's -X, points
    // along it). The aiming view's camera spot is stepped square onto it (its place along the line kept), so from there
    // rear sight and front sight line up in the middle of the picture, where her aim - and so her shots - go.
    // (Measured in this frame's skeleton and her facing now; kept in her frame and turned with her drawn facing.)
    item = rd8(rec + REC_WEAPON);
    sight = sight_of(item);
    hand = table ? hand_node(table) : 0;
    ride = 0;
    if (sight && hand && aimReady) {
        V3 nx = unit(rdv(hand + 0x00), fwdNow), ny = unit(rdv(hand + 0x10), rightNow);
        V3 nz = unit(rdv(hand + 0x20), v3(0, 1, 0));
        V3 rear = add(rdv(hand + 0x30), add(mul(nx, sight->x), add(mul(ny, sight->y), mul(nz, sight->z))));
        V3 barrel = mul(nx, -1.0f), bl = to_local(barrel, fwdNow, rightNow);
        u32 aim16 = rd16(rec + REC_AIM), settled, aimMoving;
        // The line is usable where the gun is held still on her aim (or follows her aiming input).
        settled = s.havePrev && dot(bl, s.prevBarrel) > FP_SETTLED_COS;
        aimMoving = s.havePrev && aim16 != s.prevAim;
        s.prevBarrel = bl;
        s.prevAim = aim16;
        s.havePrev = 1;
        if (sane3(rear) && dot(barrel, aimDirNow) > FP_STEADY_COS && (settled || aimMoving)) {
            V3 off = sub(rear, sub(aimPos, shift));
            s.lineLocal = to_local(sub(off, mul(aimDirNow, dot(off, aimDirNow))), fwdNow, rightNow);
            if (!s.haveSight) {
                fp_log("[fp] aiming: lined up with the sights, item", 1, (s32)item, 0);
            } else if (s.holding) {
                reo_log("[fp] aiming: the gun is back on the line");
            }
            s.haveSight = 1;
            s.holding = 0;
            ride = 1;
        }
    } else {
        s.havePrev = 0;
    }
    if (s.haveSight && raised && !ride && !s.holding) {
        // A shot's recoil, a gesture, a hit: the gun leaves the line as the game animates it. The camera keeps the
        // step onto the line it had (turning with her), so the gun is seen kicking and the camera stays in the eyes.
        s.holding = 1;
        reo_log("[fp] aiming: the gun left the line (shot / gesture): the view holds the line");
    }
    s.sightBlend = clampf(s.sightBlend + (s.haveSight && raised ? step : -step), 0.0f, 1.0f);
    if (!raised && s.sightBlend <= 0.0f) {
        s.haveSight = 0;
        s.holding = 0;
    }
    if (s.haveSight) {
        aimPos = add(aimPos, mul(from_local(v3(0.0f, 0.0f, 0.0f), aimFwd, aimRight, s.lineLocal), s.sightBlend));
    }
    aimView = look_up(aimDir, ta.look);

    // The pad mapper's turn of her in the next frame (FP_TURN_STEP toward where it sets her: the view's yaw as she
    // stands, the way the left stick walks her by the camera written below - the stick as it was in this frame). The
    // pad mapper turns her so (at most FP_TURN_ASIDE off it toward a new push), and the normal view below carries her
    // head on into the drawn frame by that turn.
    s.plan = 0;
    if (!raised && s.haveView && free_to_turn(rec)) {
        u32 want, steer;
        f32 vs, vc, rh = fp_sqrt((htNow.x - pos.x) * (htNow.x - pos.x) + (htNow.z - pos.z) * (htNow.z - pos.z));
        s32 most = FP_TURN_STEP;
        if (rh * (f32)FP_TURN_STEP > FP_TURN_SWING * FP_YAW_PER_RADIAN) {
            most = (s32)(FP_TURN_SWING * FP_YAW_PER_RADIAN / rh);  // (her head far ahead of her feet: bent forward)
        }
        yaw_sincos(s.viewYaw, &vs, &vc);
        if (pad_want(rec, s.padMag, s.padDir, s.padDash, (u32)yaw_step(vs, vc) * 16u, &want, &steer)) {
            s.plan = 1;
            s.planH = turn_toward(h16, want, most);
            s.planMost = most;
        }
    }

    // The normal view: where you look, from her eyes - at her head as it will be drawn. The eye spot with the offsets
    // (this frame's skeleton: the head joint and the eye spot's place from it, in the head's own frame) is laid out
    // from her head joint in the view's direction instead of her head's: the camera is where her eyes are when she
    // turns her head to where you look. With her head facing the view it is the eye spot itself; walking sideways or
    // back, turned round or thrown about in a hit, it stays at her head, about 16 units in front of her head joint
    // along the view, looking out of it - never a spot turned about her feet, never at her own face, never through a
    // wall. Her head's own turns do not move it, its steps do. Her head is carried on into the drawn frame by her
    // feet's step of the last frame, only while she goes on that way (never the way back: it never runs ahead of her
    // head and comes back), and by her turn about her feet in that frame as far as it is known: the pad mapper's
    // planned turn (above), else the game's own steady turn (FP_GAME_TURN), her step turned with it. A turn nobody
    // foresaw swings her head round her feet: that swing is taken out and the camera eases onto her head again
    // (FP_EYE_EASE), her head never in front of the lens (FP_HEAD_CLEAR).
    {
        V3 hyN = unit(hyNow, v3(0.0f, 1.0f, 0.0f)), hzN = unit(hzNow, fwdNow), fN, uN, rN, spot, rel, hf, hr, vf, vr;
        V3 head, jp, r, carry = v3(0.0f, 0.0f, 0.0f);
        f32 sp, cp, sy, cy, vs, vc;
        s32 turnNow, turnNext = 0;
        u32 kind = rd8(rec + REC_AIMSTATE);
        fN = unit(sub(mul(hzN, rc), mul(hyN, rs)), fwdNow);
        uN = unit(add(mul(hyN, rc), mul(hzN, rs)), v3(0.0f, 1.0f, 0.0f));
        rN = unit(cross(fN, uN), rightNow);
        spot = add(htNow, add(mul(hyNow, lock->up), mul(hzNow, lock->forward)));
        spot = add(spot, add(mul(fN, tn.fwd), add(mul(uN, tn.up), mul(rN, tn.right))));
        rel = sub(spot, htNow);  // (the eye spot from her head joint)
        hf = unit(v3(fN.x, 0.0f, fN.z), fwdNow);
        hr = v3(-hf.z, 0.0f, hf.x);
        yaw_sincos(s.viewYaw, &vs, &vc);
        vf = v3(vs, 0.0f, vc);
        vr = v3(-vc, 0.0f, vs);
        r = sub(htNow, pos);              // (her head joint from her feet)
        turnNow = yaw_diff(h16, s.outH);  // (her turn about her feet since the last mark)
        if (s.haveOut) {
            V3 d = sub(pos, s.lastPos);
            if (dot(d, d) > FP_PREDICT_JUMP * FP_PREDICT_JUMP) {
                s.haveOut = 0;  // (her feet placed elsewhere: a door, a new room - from her head there)
            }
        }
        if (!s.haveOut) {
            s.easing = 0;
            s.quickHold = 0;
            s.feetStep = v3(0.0f, 0.0f, 0.0f);
        } else {
            // Her feet's step: carried on as track() does (a step that grew by more than FP_PREDICT_GROW only at the
            // size of the one before; none longer than FP_PREDICT_JUMP), and only on the way she went the frame before.
            V3 d = sub(pos, s.lastPos), sw, sr;
            f32 n1 = fp_sqrt(dot(d, d)), n0 = fp_sqrt(dot(s.feetStep, s.feetStep)), n;
            u32 unseen = turnNow - s.turnNext > FP_TURN_UNSEEN || s.turnNext - turnNow > FP_TURN_UNSEEN;
            if (n1 <= FP_PREDICT_JUMP && dot(d, s.feetStep) > 0.0f) {
                carry = n1 > n0 + FP_PREDICT_GROW ? mul(d, n0 / n1) : d;
                n = fp_sqrt(dot(carry, carry));
                if (n > FP_CARRY_MAX) {
                    carry = mul(carry, FP_CARRY_MAX / n);  // (the game's one-off long step as she changes her way)
                }
            }
            s.feetStep = n1 <= FP_PREDICT_JUMP ? d : v3(0.0f, 0.0f, 0.0f);
            if (s.easing) {
                f32 st;
                n = fp_sqrt(dot(s.ease, s.ease));
                st = clampf(n * FP_EYE_EASE, FP_EYE_EASE_MIN, FP_EYE_EASE_MAX) * dt * 0.5f;
                if (!(n > st)) {
                    s.easing = 0;
                    s.ease = v3(0.0f, 0.0f, 0.0f);
                } else {
                    s.ease = mul(s.ease, (n - st) / n);
                }
            }
            // (a turn the last mark did not foresee: the swing of her head round her feet by its unforeseen part, as
            // far as her head did go that way - not where the game turns her about her head, as in a grab - taken out)
            sr = sub(turn_to(s.lastR, (f32)turnNow), turn_to(s.lastR, (f32)s.turnNext));
            n = fp_sqrt(dot(sr, sr));
            if (!s.quickHold && unseen && n > 0.5f) {
                sw = mul(sr, clampf(dot(sub(r, turn_to(s.lastR, (f32)s.turnNext)), sr) / (n * n), 0.0f, 1.0f));
                s.ease = s.easing ? sub(s.ease, sw) : mul(sw, -1.0f);
                s.easing = 1;
                n = fp_sqrt(dot(s.ease, s.ease));
                if (n > FP_EASE_REACH) {
                    s.ease = mul(s.ease, FP_EASE_REACH / n);
                }
            }
            if (s.plan) {
                turnNext = yaw_diff(s.planH, h16);
            } else if (!s.planWas && turnNow <= FP_GAME_TURN && turnNow >= -FP_GAME_TURN) {
                turnNext = turnNow;  // (the game's own steady turn, carried on)
            }
        }
        s.lastR = r;
        s.outH = h16;
        s.turnNext = turnNext;
        s.planWas = s.plan;
        // Her head joint as it will be drawn, and the eye spot laid out from it along the view.
        jp = add(pos, add(turn_to(carry, (f32)turnNext), turn_to(r, (f32)turnNext)));
        head = add(jp, add(mul(vf, dot(rel, hf)), add(mul(vr, dot(rel, hr)), v3(0.0f, rel.y, 0.0f))));
        // The game's quick turn (her action's kind FP_KIND_QUICK_TURN): it spins her round in its animation and only
        // then commits her new heading. Her own turn does not move the camera: it keeps its spot by her feet through it,
        // and while she stands on after it and the pad mapper turns her back to the view, until her eyes are back there
        // (within FP_EYE_EASE_MAX, or she faces the view, or her head as drawn would come nearer the spot than
        // FP_HEAD_CLEAR along the view), and goes on from there (FP_EYE_EASE) - not round to her back with her head and
        // back.
        if (stance == 1 && s.haveOut &&
            (kind == FP_KIND_QUICK_TURN ||
             (s.quickHold && kind == 0 && dot(fwdNow, vf) < FP_QUICK_BACK_COS &&
              dot(sub(add(pos, s.quickSpot), head), sub(add(pos, s.quickSpot), head)) >
                  FP_EYE_EASE_MAX * FP_EYE_EASE_MAX &&
              dot(sub(add(pos, s.quickSpot), jp), vf) >= FP_HEAD_CLEAR))) {
            if (!s.quickHold) {
                s.quickHold = 1;
                s.quickSpot = sub(s.lastOut, s.lastPos);
            }
            normalPos = add(pos, s.quickSpot);
        } else {
            if (s.quickHold) {
                s.quickHold = 0;
                s.ease = sub(add(pos, s.quickSpot), head);
                s.easing = 1;
            }
            if (s.easing) {
                // (never nearer her head joint along the view than FP_HEAD_CLEAR, nor nearer than the eye spot is)
                f32 lo = FP_HEAD_CLEAR - dot(sub(head, jp), vf), e = dot(s.ease, vf);
                lo = lo > 0.0f ? 0.0f : lo;
                if (e < lo) {
                    s.ease = add(s.ease, mul(vf, lo - e));
                }
                head = add(head, s.ease);
            }
            normalPos = head;
        }
        s.haveOut = 1;
        s.lastOut = normalPos;
        s.lastPos = pos;
        sp = clampf(s.viewPitch + tn.look, -85.0f, 85.0f) * 0.017453293f;
        sy = fp_sin(sp);
        cy = fp_cos(sp);
        yaw_sincos(s.viewYaw + (raised ? (f32)aimCarry : 0.0f), &sp, &cp);  // (raised: her aim, carried on alike)
        normalDir = v3(cy * sp, sy, cy * cp);
    }

    // The view: normal <-> aiming in 0.2 s (position, direction and field of view). Her aim ended, the picture keeps
    // the normal view's direction (where it looked while she aimed) and only the camera's spot goes back to the eyes.
    if (!raised) {
        aimView = normalDir;
    }
    camPos = lerp(normalPos, aimPos, s.aimBlend);
    camDir = unit(lerp(normalDir, aimView, s.aimBlend), normalDir);
    fov = tn.fov + (ta.fov - tn.fov) * s.aimBlend;

    if (!sane3(camPos) || !sane3(camDir)) {
        return;  // (never write a broken camera; the last one stays)
    }
    if (raised) {
        s.aimYaw = yaw_wrap((f32)(fp_atan2((double)camDir.x, (double)camDir.z) * (32768.0 / 3.141592653589793)));
        s.aimPitch = (f32)(fp_atan2((double)camDir.y, (double)fp_sqrt(camDir.x * camDir.x + camDir.z * camDir.z)) *
                           (180.0 / 3.141592653589793));
    }
    for (i = 0; i < 3; i++) {
        f32 p = i == 0 ? camPos.x : (i == 1 ? camPos.y : camPos.z);
        f32 d = i == 0 ? camDir.x : (i == 1 ? camDir.y : camDir.z);
        wrf(CAM_EYE + 4 * i, p);
        wrf(CAM_AT + 4 * i, p + d * FP_LOOK);
    }
    s.lastEye = camPos;
    s.lastAt = add(camPos, mul(camDir, FP_LOOK));
    s.haveLast = 1;
    wrf(CAM_FOV, fov);

    // Characters right in front of the camera stay drawn while it drives (her own arms and gun, a zombie up close).
    if (!s.haveHide) {
        s.hideOrig = rdf(HIDE_RADII + 4);
        s.haveHide = 1;
    }
    wrf(HIDE_RADII + 4, 0.0f);

    crosshair(stance == 2);
    status(FP_STATUS_DRIVING);
}

FP_EVENT(recomp_on_play_main)
void Fp_OnPlayMain(void) {
    frame();
}

// The button of the dash (run) in the player's control type (0: none found).
static u32 dash_button(void) {
    static const u32 kButtons[3] = {0x100u, 0x10u, 0x20u};
    u32 row = CONTROL_TABLE + 12u * rd8(CONTROL_TYPE), c;
    if (rd8(CONTROL_TYPE) >= 8) {
        return 0;
    }
    for (c = 0; c < 3; c++) {
        if (rd32(row + 4u * c) == ACTION_DASH) {
            return kButtons[c];
        }
    }
    return 0;
}

// Her action lets the player turn and walk her: she stands, walks, runs, turns or steps back (stance 1; her action's
// kind (+0x009) none or a step of the walking input (3 walk, 4 turn, 5 walk and turn, 6 - 8 run, 9 - 0x0B step back),
// its phase starting (1), going (2), stopping (0x0A) or standing (0x0B); the walking input's action 0 .. 3).
static u32 free_to_turn(u32 rec) {
    u32 kind = rd8(rec + REC_AIMSTATE), act = rd8(rec + REC_ACTION), pad = rd8(rec + REC_PAD_ACTION);
    return rd8(rec + REC_STANCE) == 1 && kind <= 0x0B && (act == 0x0B || act == 0x0A || act == 0x01 || act == 0x02) &&
           pad <= 3;
}

// The pad mapper (the camera.padMap mark: before it copies the mapped pad record into her record; after the game worked
// out its system pad, before the ad-lib reads it; right before her walking input reads the stick and the camera).
// While the right stick is held centred, a new press of its click (R3) is taken out of the system pad's new presses:
// it is an ad-lib line of the game too. While the camera drives, her heading is set here, as the game keeps it:
//   - her weapon raised: turned by the right stick (raising it, to where you look);
//   - she stands or walks (pad_want): with the left stick pushed, toward the way her walking input sends her (the
//     camera's yaw step plus the stick's direction, each in steps of 16), and the game is given her heading as the
//     stick's direction, so she sets off and goes the way she faces; a push backward without the dash keeps her facing
//     the view while she stands or steps back (the game steps her back; where it turns her round instead, her turn is
//     the game's); with the stick at rest, toward the view's yaw while she stands (as she stops after a walk, her
//     heading stays the walk's). She turns as the mark planned it (FP_TURN_STEP a frame toward that from the stick as
//     it was the frame before), and at most FP_TURN_ASIDE off that plan toward a new push of the stick.
// Nothing is written in any other action, or while the camera does not drive.
FP_EVENT(recomp_on_pad_map)
void Fp_OnPadMap(u32 held, u32 rec) {
    u32 h, want, stance, dir, dash, steer, live, cam16, base, to;
    s32 mag;
    if (rec != CHAR_BASE) {
        return;
    }
    if (s.padCentred && (rd32(SYS_PAD_PRESSED) & PAD_R3)) {
        wr32(SYS_PAD_PRESSED, rd32(SYS_PAD_PRESSED) & ~PAD_R3);
    }
    if (!s.driving || !s.haveView) {
        s.plan = 0;
        return;
    }
    h = rd16(rec + REC_HEADING);
    stance = rd8(rec + REC_STANCE);
    mag = (s32)(s16)rd16(held + PAD_LSTICK_MAG);
    dir = rd16(held + PAD_LSTICK_DIR);
    dash = (rd16(held) & dash_button()) != 0;
    s.padMag = mag;  // (the stick as it is in this frame: the mark plans her turn of the next frame from it)
    s.padDir = dir;
    s.padDash = dash;
    if (stance == 2 || stance == 3) {
        if (s.aimTurn != 0) {
            wr16(rec + REC_HEADING, h + (u32)s.aimTurn);
        }
        s.aimApplied = s.aimTurn;
        s.aimTurn = 0;
    } else if (free_to_turn(rec)) {
        cam16 = (u32)yaw_step(rdf(CAM_AT) - rdf(CAM_EYE), rdf(CAM_AT + 8) - rdf(CAM_EYE + 8)) * 16u;
        live = pad_want(rec, mag, dir, dash, cam16, &want, &steer);
        base = s.plan ? s.planH : h;
        to = turn_toward(base, live ? turn_toward(h, want, s.plan ? s.planMost : FP_TURN_STEP) : base, FP_TURN_ASIDE);
        if (steer) {
            // (the game walks her the way she faces: her heading, in steps of 16, as the stick's direction)
            to = (to + 8u) & 0xFFF0u;
            if (((cam16 + (dir & 0xFFF0u)) & 0xFFFFu) != to) {
                wr16(held + PAD_LSTICK_DIR, (to - cam16) & 0xFFFFu);
            }
        }
        if ((live || s.plan) && to != h) {
            wr16(rec + REC_HEADING, to);
        }
    }
    s.plan = 0;
}

// The room's draw (the camera.roomDraw mark: the room loop, once per object group, before its objects).
FP_EVENT(recomp_on_room_draw)
void Fp_OnRoomDraw(u32 room, u32 group) {
    sMir.marked = 1;
    if (room == ROOM_DRAW && s.driving && sArt.on && mirror_room()) {
        mirror_draw(group);
    }
}
