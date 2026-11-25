// NPC hunter for MHGU (Forge plugin). Test 1: put a second hunter, a copy of yours, into a free slot of your quest.
//
// How the game does it for a real partner (MHGU 1.4.0, runtime addresses: image offset + 0x200000):
//   - each member's hunter data (0x560 bytes) sits in sPlayer at +0x5dc + slot * 0x560 (yours at +0x7c), sent over
//     the network as netPlData; +0x504 bit 3 marks it valid (0x46fddc reads it, 0x46fde8 sets it)
//   - when a quest loads, for every other member slot the loader (0x2066a4...) calls 0x474cf0(sPlayer, slot, 0),
//     which builds that hunter (uPlayerQuestNN by weapon type) from the data and puts it in sPlayer +0x1c + slot * 4
// Test 2: the helper counts as a member of the quest. When you depart (the town's depart method, 0x8c02a4), it is
// added to the quest's members (sQuestSelector +0xd750 mJoinNum, entries of 12 bytes at +0xd754: u8 host flag, u32
// slot), as a player who joined at the board would be; once the quest has started it is made in its slot by itself.
// F4 turns the helper on / off (on at start). F5 makes it now, in a quest. F6 writes the state to the log.
#include <stdbool.h>
#include <stdint.h>

#define NPC_VERSION "0.1.12"
#ifndef NPC_BUILD
#define NPC_BUILD "dev"
#endif
#define NPC_NAME "NPC Hunter " NPC_VERSION " (build " NPC_BUILD ")"

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int16_t s16;

// ---- Forge API (implemented by Forge on the console / emulators, and by the port itself) ----
enum { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };
extern void forge_log(int level, const char *fmt, ...);
extern u32 forge_mem_getMainTextAddr(void);
extern bool forge_input_isKeyPressed(int key);
#define KEY_F4 61
#define KEY_F5 62
#define KEY_F6 63
#define KEY_F7 64

typedef struct { u16 major, minor, patch; } ForgeVersion;
typedef struct { ForgeVersion required; char name[64]; } PluginInitParam;
typedef struct { void *target, *detour, *original, *context; void **ctx_cell; bool has_ctx; } Hook;
extern Hook forge_hook_create(void *target, void *detour, void **original);
// Dear ImGui (cimgui), as Forge gives it to plugins: drawn over the game by forge_onImGuiFreeRender
typedef struct { float x, y; } ImVec2;
extern void igSetNextWindowPos(ImVec2 pos, int cond, ImVec2 pivot);
extern void igSetNextWindowBgAlpha(float alpha);
extern bool igBegin(const char *name, bool *open, int flags);
extern void igEnd(void);
extern void igTextColored(float r, float g, float b, float a, const char *fmt, ...);
extern void *igGetMainViewport(void);

void init(void) {}
void fini(void) {}

// ---- game addresses ----
#define RT_BASE      0x200000u
#define G_SPLAYER    0x1a85b58u  // sPlayer *
#define G_GAMECTL    0x1a8ca48u  // sGameControl *
#define G_NETWORK    0x1a843f0u  // sFestaNetwork *
#define FN_DATA_COPY 0x46e600u   // (dst data, src data)
#define FN_DATA_OK   0x46fddcu   // (data): valid
#define FN_DATA_SETOK 0x46fde8u  // (data, bool)
#define FN_MAKE_PL   0x474cf0u   // (sPlayer, slot, mode 0 quest / 1 hub): builds that slot's hunter from its data
#define FN_MY_SLOT   0x5f8398u   // (sGameControl): our slot
#define SP_SLOTS     0x1cu       // sPlayer: the 4 hunters
#define SP_MY_DATA   0x7cu       // sPlayer: our hunter data
#define SP_MEM_DATA  0x5dcu      // sPlayer: members' hunter data, 0x560 each
#define DATA_SIZE    0x560u
#define DATA_NAME    0x514u      // hunter data: the name (as hle/chat.c reads it)
#define DATA_WEAPON  0x4d4u      // hunter data: weapon type
#define PL_POS       0x40u
#define PL_AREA      0x1054u
#define PL_DATA      0x13c4u
#define PL_MODE      0x13c8u     // 0 quest, 1 hub, ...
#define PL_NET_POS   0x1480u     // a remote hunter's position target (netPlPositionQuest)
#define G_PAD        0x1a8c670u  // sFestaPad *: the gameplay view of the pad (button object at +0x6b98)
#define PAD_CUR      0x6b9cu     // u32 held: buttons (A 0x100, B 0x80, X 0x20, Y 0x10, R 0x40, ZL 0x200, ZR 0x800) and the
                                 // left stick past half way (bits 16-19)
#define PAD_PREV     0x6ba0u
#define PAD_TRIG     0x6ba4u     // pressed this update
#define PAD_REL      0x6ba8u
#define PAD_LANGLE   0x6bacu     // u16: the left stick's direction, 0x10000 a turn, 0 right, 0x4000 up
#define PAD_RANGLE   0x6baeu
#define PAD_LMAG     0x6bb0u     // u32: how far it is pushed, 0 (below 30) .. 127
#define PAD_RMAG     0x6bb4u
#define PAD_VIEW_LEN 0x1cu       // +0x6b9c .. +0x6bb7
#define FN_PL_PAD    0x4a79ccu   // a hunter's update from the pad (vtable +0x228), called only for the local one
                                 // (player +0x13cc bit 0, 0x47c97c)
#define PL_LOCAL     0x13ccu     // u32: bit 0 this game's own hunter: it reads the pad
#define PL_DRAWN     0x114cu     // u8: 1 weapon out
#define PL_HP        0x13d6u     // s16
#define G_CAMERAS    0x2326504u  // the camera manager *: +0x54 the camera in use (eye +0x40, target +0x60)
#define G_SENEMY     0x1a51fe0u  // sEnemy *: +0x1c the 10 live enemies of the zone
#define EN_INFO      0x1428u     // enemy: status block (+0x40 position, +0x1b6 u16 network id, +0x370 s32 HP)
#define PL_ACT_ST    0x2506u     // u16 act_st (0x0001 common, 0x0004 fall, 0x0010 weapon, 0x0100 fallen)
#define PL_ACT_NO    0x250au
#define B_A 0x100u
#define B_B 0x80u
#define B_X 0x20u
#define B_Y 0x10u
#define B_R 0x40u
#define NET_STATE    0xa90u      // sFestaNetwork: each member's state (u32 x 4; 3: in the quest)
#define NET_ONLINE   0x63eu      // sFestaNetwork: u8, a session (0x427ba8)
#define G_SELECTOR   0x1a8ae38u  // sQuestSelector *
#define SEL_JOIN     0xd750u     // mJoinNum: the quest's members
#define SEL_MEMBERS  0xd754u     // 12 bytes each: +0 u8 host, +4 u32 slot, +8 ?
#define SEL_ORDERED  0xd68u
#define FN_DEPART    0x8c02a4u   // the town's depart method (area)
#define FN_MAKE_ME   0x4758a4u   // (sPlayer, mode 0 quest / 1 hub): makes our own hunter; in a quest's loading the
                                 // others' hunters are made right after it (0x2066a4, online only)

static u32 g_text;
#define A(rt) (g_text + ((rt) - RT_BASE))
#define R32(a) (*(volatile u32 *)(uintptr_t)(a))
#define R8(a) (*(volatile u8 *)(uintptr_t)(a))
#define RF(a) (*(volatile float *)(uintptr_t)(a))
#define R16(a) (*(volatile u16 *)(uintptr_t)(a))

typedef void (*fn_copy)(u32, u32);
typedef u32 (*fn_u32_u32)(u32);
typedef void (*fn_setok)(u32, u32);
typedef u32 (*fn_make)(u32, u32, u32);
typedef u32 (*fn_vt0)(u32);
typedef void (*fn_vt1)(u32, u32);
typedef void (*fn_do_action)(u32, u32, u32, u32);  // (player, act_st, act_no, param)

static u32 g_npc;       // the hunter we made (0: none)
static int g_npc_slot = -1;
static u32 g_npc_owner; // our own hunter when we made it (another one: a new quest)

static bool g_enabled = true;
static bool g_drive = true;  // the helper moves by itself (F7)
static u32 v_cur, v_prev;    // its virtual pad: buttons this update / last update (gameplay view bits)
static u16 v_angle;          // left stick direction (pad units)
static u32 v_mag;            // 0..127
static int g_state = -1;     // what it is doing
static s16 g_last_hp = -1;
static u32 g_combo;
static int g_member_slot = -1;  // the slot we added to the quest's members at departure (-1: none)
static u32 g_frames, g_seen_since;
static bool g_placed = true;
static const char *g_toast;  // a short message over the game, for a few seconds
static u32 g_toast_until;
static bool g_toast_good;

static void toast(const char *msg, bool good) {
    g_toast = msg, g_toast_good = good;
    g_toast_until = g_frames + 180;
}  // the helper is beside us (made during loading: not yet)  // frames, and the frame our quest hunter was first seen

static bool sane(u32 a) { return a >= 0x01000000u && a < 0xF0000000u && !(a & 3); }

static u32 my_slot(void) {
    u32 gc = R32(A(G_GAMECTL));
    return sane(gc) ? ((fn_u32_u32)A(FN_MY_SLOT))(gc) : 99;
}

static u32 my_hunter(u32 sp, u32 me) { return me < 4 ? R32(sp + SP_SLOTS + 4 * me) : 0; }

static void copy_name(u32 dst, const char *s) {
    u32 i = 0;
    for (; s[i] && i < 0x1f; i++) R8(dst + i) = (u8)s[i];
    for (; i < 0x20; i++) R8(dst + i) = 0;
}

static void place(u32 npc, u32 mine);
static u32 make_helper(u32 sp, u32 me, int slot);


// Our hunter data copied into the slot's member data, valid, named, and the game's own call that makes a member's
// hunter from it.
static u32 make_helper(u32 sp, u32 me, int slot) {
    (void)me;
    u32 src = sp + SP_MY_DATA, dst = sp + SP_MEM_DATA + DATA_SIZE * (u32)slot;
    forge_log(LOG_INFO, "[npc] slot %d: copying our hunter data (weapon %u, name '%s') to %08x", slot, R8(src + DATA_WEAPON),
              (const char *)(uintptr_t)(src + DATA_NAME), dst);
    ((fn_copy)A(FN_DATA_COPY))(dst, src);
    ((fn_setok)A(FN_DATA_SETOK))(dst, 1);
    copy_name(dst + DATA_NAME, "Ajudante");
    u32 npc = ((fn_make)A(FN_MAKE_PL))(sp, (u32)slot, 0);
    forge_log(LOG_INFO, "[npc] made %08x (slot now %08x)", npc, R32(sp + SP_SLOTS + 4 * (u32)slot));
    if (sane(npc)) g_state = -1, g_last_hp = -1, g_combo = 0, v_cur = v_prev = 0, v_mag = 0;
    return npc;
}

// The quest's loading makes our hunter: the helper is made right after it, where the game makes the other members'.
typedef u32 (*fn_make_me)(u32, u32);
static fn_make_me g_make_me_orig;
static u32 make_me_hook(u32 sp, u32 mode) {
    u32 r = g_make_me_orig(sp, mode);
    if (mode != 0 || !g_enabled || !sane(sp)) return r;
    u32 me = my_slot(), mine = my_hunter(sp, me);
    int slot = -1;
    for (u32 i = 0; i < 4 && slot < 0; i++)
        if (i != me && R32(sp + SP_SLOTS + 4 * i) == 0) slot = (int)i;
    forge_log(LOG_INFO, "[npc] quest loading: our hunter %08x (slot %u) made, helper into slot %d", mine, me, slot);
    if (slot < 0 || !sane(mine)) return r;
    u32 npc = make_helper(sp, me, slot);
    if (sane(npc)) g_npc = npc, g_npc_slot = slot, g_npc_owner = mine, g_placed = false, toast("O Ajudante entrou na missao (" NPC_NAME ")", true);
    return r;
}

static void spawn(void) {
    u32 sp = R32(A(G_SPLAYER));
    if (!sane(sp)) { forge_log(LOG_WARN, "[npc] no sPlayer"); return; }
    u32 me = my_slot(), mine = my_hunter(sp, me);
    if (!sane(mine)) { forge_log(LOG_WARN, "[npc] no hunter of ours (slot %u)", me); return; }
    u32 mode = R32(mine + PL_MODE);
    if (mode != 0) { forge_log(LOG_WARN, "[npc] not in a quest (our hunter's mode %u): start a quest first", mode); return; }
    if (g_npc && g_npc_owner == mine && R32(sp + SP_SLOTS + 4 * (u32)g_npc_slot) == g_npc) {
        forge_log(LOG_INFO, "[npc] already here (slot %d)", g_npc_slot);
        return;
    }
    int slot = -1;
    if (g_member_slot >= 0 && R32(sp + SP_SLOTS + 4 * (u32)g_member_slot) == 0) slot = g_member_slot;
    for (u32 i = 0; i < 4 && slot < 0; i++)
        if (i != me && R32(sp + SP_SLOTS + 4 * i) == 0) slot = (int)i;
    if (slot < 0) { forge_log(LOG_WARN, "[npc] no free slot"); return; }

    u32 npc = make_helper(sp, me, slot);
    if (!sane(npc)) return;
    g_npc = npc, g_npc_slot = slot, g_npc_owner = mine;
    place(npc, mine);
}

static void place(u32 npc, u32 mine) {
    // Into our zone, as the game does for hunters made beside ours (0x4762e4): its +0xa0 with our +0x9c, its +0xa4
    // with our zone.
    u32 vt = R32(npc), myvt = R32(mine);
    u32 v = ((fn_vt0)R32(myvt + 0x9c))(mine);
    ((fn_vt1)R32(vt + 0xa0))(npc, v);
    ((fn_vt1)R32(vt + 0xa4))(npc, R8(mine + PL_AREA));
    // Two metres beside us, and its network target there too (a remote hunter glides to it).
    for (u32 k = 0; k < 3; k++) {
        float f = RF(mine + PL_POS + 4 * k) + (k == 0 ? 200.0f : 0.0f);
        RF(npc + PL_POS + 4 * k) = f;
        RF(npc + PL_NET_POS + 4 * k) = f;
    }
    g_placed = true;
    forge_log(LOG_INFO, "[npc] placed in zone %u", R8(npc + PL_AREA));
}

static void dump_members(const char *when) {
    u32 sel = R32(A(G_SELECTOR)), net = R32(A(G_NETWORK));
    if (!sane(sel)) return;
    u32 n = R32(sel + SEL_JOIN);
    forge_log(LOG_INFO, "[npc] %s: quest %u, members %u, session %u", when, R32(sel + SEL_ORDERED), n,
              sane(net) ? R8(net + NET_ONLINE) : 99);
    for (u32 i = 0; i < 4; i++) {
        u32 e = sel + SEL_MEMBERS + 12 * i;
        forge_log(LOG_INFO, "[npc]   member %u: %08x %08x %08x", i, R32(e), R32(e + 4), R32(e + 8));
    }
}

typedef void (*fn_depart)(u32);
static fn_depart g_depart_orig;
static void depart_hook(u32 area) {
    dump_members("departing (before)");
    g_depart_orig(area);
    dump_members("departing (after the game)");
    // (offline the game keeps no member list: nothing to add here; logged for comparison)
}

static void report(void) {
    u32 sp = R32(A(G_SPLAYER)), net = R32(A(G_NETWORK)), me = my_slot();
    forge_log(LOG_INFO, "[npc] sPlayer %08x, network %08x, our slot %u, npc %08x (slot %d)", sp, net, me, g_npc, g_npc_slot);
    if (!sane(sp)) return;
    for (u32 i = 0; i < 4; i++) {
        u32 p = R32(sp + SP_SLOTS + 4 * i), d = sp + SP_MEM_DATA + DATA_SIZE * i;
        u32 st = sane(net) ? R32(net + NET_STATE + 4 * i) : 0xffffffffu;
        if (sane(p))
            forge_log(LOG_INFO, "[npc]  slot %u: hunter %08x mode %u zone %u at %d %d %d, net state %u, data valid %u", i, p,
                      R32(p + PL_MODE), R8(p + PL_AREA), (int)RF(p + PL_POS), (int)RF(p + PL_POS + 4), (int)RF(p + PL_POS + 8),
                      st, ((fn_u32_u32)A(FN_DATA_OK))(d));
        else
            forge_log(LOG_INFO, "[npc]  slot %u: empty, net state %u, data valid %u", i, st, ((fn_u32_u32)A(FN_DATA_OK))(d));
    }
}


// ------------------------------------------------------------------ the helper's virtual pad
static u32 g_pad_calls;

static float fabsf_(float x) { return x < 0 ? -x : x; }
static float atan2f_(float y, float x) {  // (max error ~0.005 rad)
    float ax = fabsf_(x), ay = fabsf_(y);
    if (ax < 1e-6f && ay < 1e-6f) return 0.0f;
    float a = ax > ay ? ay / ax : ax / ay, s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    if (y < 0) r = -r;
    return r;
}
static u16 yaw16(float dx, float dz) { return (u16)(int)(atan2f_(dx, dz) * 10430.378f); }
static float dist2(u32 a, u32 b) {  // squared, on the ground (positions at +0x40 of two objects or blocks)
    float dx = RF(a) - RF(b), dz = RF(a + 8) - RF(b + 8);
    return dx * dx + dz * dz;
}

// The direction the camera looks, as the game works it out for the stick (0x47f5d0): hunter's move yaw = camera yaw +
// stick + 0xc000.
static bool camera_yaw(u16 *out) {
    u32 m = R32(A(G_CAMERAS));
    if (!sane(m)) return false;
    u32 cam = R32(m + 0x54);
    if (!sane(cam)) return false;
    *out = yaw16(RF(cam + 0x60) - RF(cam + 0x40), RF(cam + 0x68) - RF(cam + 0x48));
    return true;
}

// Point the virtual stick so the helper moves along world yaw w (the yaw16 of a vector), fully pushed.
static void stick_to(u16 w) {
    u16 cy;
    if (!camera_yaw(&cy)) { v_mag = 0; return; }
    v_angle = (u16)(w - cy - 0xc000u);
    v_mag = 127;
}

static u32 stick_bits(void) {  // the held word's left stick directions past half way (up, down, left, right)
    if (v_mag < 64) return 0;
    u32 b = 0, a = v_angle;
    if (a > 0x1000 && a < 0x7000) b |= 0x10000;   // up
    if (a > 0x9000 && a < 0xf000) b |= 0x20000;   // down
    if (a > 0x5000 && a < 0xb000) b |= 0x40000;   // left
    if (a < 0x3000 || a > 0xd000) b |= 0x80000;   // right
    return b;
}

// The pad as the helper's update reads it: the gameplay view swapped for the virtual one around the game's own code.
typedef void (*fn_pl_pad)(u32);
static fn_pl_pad g_pl_pad_orig;
static void pl_pad_hook(u32 pl) {
    u32 pad = R32(A(G_PAD));
    if (!g_npc || pl != g_npc || !sane(pad)) { g_pl_pad_orig(pl); return; }
    u32 save[PAD_VIEW_LEN / 4];
    for (u32 i = 0; i < PAD_VIEW_LEN / 4; i++) save[i] = R32(pad + PAD_CUR + 4 * i);
    u32 cur = v_cur | stick_bits();
    R32(pad + PAD_CUR) = cur;
    R32(pad + PAD_PREV) = v_prev;
    R32(pad + PAD_TRIG) = cur & ~v_prev;
    R32(pad + PAD_REL) = v_prev & ~cur;
    R16(pad + PAD_LANGLE) = v_mag ? v_angle : 0;
    R16(pad + PAD_RANGLE) = 0;
    R32(pad + PAD_LMAG) = v_mag;
    R32(pad + PAD_RMAG) = 0;
    g_pl_pad_orig(pl);
    for (u32 i = 0; i < PAD_VIEW_LEN / 4; i++) R32(pad + PAD_CUR + 4 * i) = save[i];
    v_prev = cur;
    g_pad_calls++;
}

// Every hunter's update refreshes its input (0x47c97c, from 0x49badc): the pad for our own hunter, nothing for the
// others' (their moves come over the network). The helper stays one of the others (made ours, by +0x13cc bit 0, it
// isn't drawn), but gets its input from the virtual pad, through the same pad update as ours.
#define FN_PL_INPUT  0x47c97cu
#define PL_INPUT     0x13f4u     // the input block (0x70 bytes): +0x13f4 held, +0x13f8 new, +0x13fc word, +0x140c
                                 // stick direction, +0x1410 how far
static u32 g_refresh_calls;
static fn_pl_pad g_pl_input_orig;
static void pl_input_hook(u32 pl) {
    if (g_npc && pl == g_npc && g_drive) {
        g_refresh_calls++;
        pl_pad_hook(pl);
        return;
    }
    g_pl_input_orig(pl);
}

// A hunter's update (uPlayerQuest move, vtable +0x24; the weapon classes' own call it): the helper's runs as if it were
// ours (+0x13cc bit 0 set for the call only), so its input turns into moves like ours does. Outside it the helper is one
// of the others again (drawn as they are).
#define FN_PL_MOVE   0x49b65cu
static fn_pl_pad g_pl_move_orig;
static u32 g_move_calls, g_wait_fixes;
static bool g_me_carted;      // we were carted and have not left the camp yet: the helper stays where it fights
static u32 g_back_at;       // frame the helper came back at the camp (0: not)
static bool g_in_npc_move;  // inside the helper's update (its local window)
#define PL_WAIT      0x1470u     // u32: not 0, the hunter's update is skipped (0x49b8e8)
static void pl_move_hook(u32 pl) {
    if (!(g_npc && pl == g_npc && g_drive)) { g_pl_move_orig(pl); return; }
    u32 f = R32(pl + PL_LOCAL), wait = R32(pl + PL_WAIT);
    R32(pl + PL_LOCAL) = f | 1u;
    g_in_npc_move = true;
    g_pl_move_orig(pl);
    g_in_npc_move = false;
    R32(pl + PL_LOCAL) = (R32(pl + PL_LOCAL) & ~1u) | (f & 1u);
    // Our own hunter's first update puts it on hold for the quest's opening (0x49b7d4: vtable +0x374 with 1, +0x1470),
    // which the quest start lifts for ours only: the helper's stays as the others' are.
    if (R32(pl + PL_WAIT) != wait && R32(pl + PL_WAIT) == 1u) {
        if (g_wait_fixes++ < 5) forge_log(LOG_INFO, "[npc] helper put on hold (%08x): undone", R32(pl + PL_WAIT));
        R32(pl + PL_WAIT) = wait;
    }
    g_move_calls++;
}

// ------------------------------------------------------------------ the helper's mind (once per frame)
enum { S_IDLE, S_FOLLOW, S_CHASE, S_ATTACK, S_DODGE, S_SHEATHE, S_WARP };
static const char *const state_name[] = {"idle", "follow", "chase", "attack", "dodge", "sheathe", "warp"};
static u32 g_t;               // frames in the current state
static u32 g_away_since;      // frames we have been in another zone
static u32 g_log_at;

static void set_state(int s) {
    if (s != g_state) {
        g_state = s, g_t = 0;
        forge_log(LOG_INFO, "[npc] helper: %s", state_name[s]);
    }
}

// The nearest large monster alive in the helper's zone (its status block, for the position), 0 none.
static u32 target_monster(u32 npc) {
    u32 sen = R32(A(G_SENEMY)), best = 0;
    float bd = 0;
    if (!sane(sen)) return 0;
    for (u32 k = 0; k < 10; k++) {
        u32 en = R32(sen + 0x1c + 4 * k);
        if (!sane(en)) continue;
        u32 info = R32(en + EN_INFO);
        if (!sane(info) || R16(info + 0x1b6) >= 10000 || (int32_t)R32(info + 0x370) <= 0) continue;
        if (R8(en + PL_AREA) != R8(npc + PL_AREA)) continue;
        float d = dist2(info + 0x40, npc + PL_POS);
        if (!best || d < bd) best = info, bd = d;
    }
    return best;
}

// A fall (act_st 0x0004, e.g. the drop from area 5 to 8 of the Jurassic Frontier) ends on landing, which the helper
// never does (stays in the air, not moving). After 1.5 s like that: the game's own "set action" back to standing
// (0x4831e0 with 1:0, as the hunter's own code does), in its local window; logged.
#define FN_SET_ACTION 0x4831e0u
static u32 g_fall_frames;
static float g_fall_x, g_fall_y, g_fall_z;
static void unstick_fall(u32 npc) {
    if (R16(npc + PL_ACT_ST) != 0x0004) { g_fall_frames = 0; return; }
    float x = RF(npc + PL_POS), y = RF(npc + PL_POS + 4), z = RF(npc + PL_POS + 8);
    float d = fabsf_(x - g_fall_x) + fabsf_(y - g_fall_y) + fabsf_(z - g_fall_z);
    g_fall_x = x, g_fall_y = y, g_fall_z = z;
    if (d > 1.0f) { g_fall_frames = 0; return; }
    if (++g_fall_frames < 90) return;
    g_fall_frames = 0;
    forge_log(LOG_INFO, "[npc] helper stuck in a fall (act 0004:%04x) at %d %d %d zone %u: set back to standing", R16(npc + PL_ACT_NO),
              (int)x, (int)y, (int)z, R8(npc + PL_AREA));
    u32 f = R32(npc + PL_LOCAL);
    R32(npc + PL_LOCAL) = f | 1u;
    ((fn_do_action)A(FN_SET_ACTION))(npc, 1, 0, 0);
    R32(npc + PL_LOCAL) = f;
}

static void think(u32 npc, u32 mine) {
    v_cur = 0, v_mag = 0;
    g_t++;
    if (g_state < 0) set_state(S_IDLE);
    s16 hp = (s16)R16(npc + PL_HP);
    bool hit = g_last_hp >= 0 && hp < g_last_hp;
    g_last_hp = hp;
    bool drawn = R8(npc + PL_DRAWN) != 0;

    // Another zone than ours for 3 s: beside us again (as the followers of other games do).
    u16 my_act = R16(mine + PL_ACT_ST);
    if (g_me_carted && R8(mine + PL_AREA) != 0 && my_act != 0x0200) g_me_carted = false;  // we left the camp
    bool we_are_down = my_act == 0x0100 || my_act == 0x0200 || g_me_carted;
    if (R8(npc + PL_AREA) != R8(mine + PL_AREA) && !we_are_down && !(g_back_at && g_frames - g_back_at < 1200)) {
        if (++g_away_since > 180) { place(npc, mine); g_away_since = 0; set_state(S_WARP); }
    } else g_away_since = 0;

    u32 mon = target_monster(npc);
    float dm = mon ? dist2(mon + 0x40, npc + PL_POS) : 1e12f, dp = dist2(mine + PL_POS, npc + PL_POS);

    if (g_state == S_DODGE && g_t < 20) {  // the roll goes on
        return;
    }
    if (mon && hit && dm < 600.0f * 600.0f && (g_frames & 1)) {  // hurt near the monster: roll away from it
        u16 away = (u16)(yaw16(RF(npc + PL_POS) - RF(mon + 0x40), RF(npc + PL_POS + 8) - RF(mon + 0x48)) + ((g_frames & 2) ? 0x2000 : 0xe000));
        stick_to(away);
        v_cur = B_B;
        set_state(S_DODGE);
        return;
    }
    if (mon && dm < 3500.0f * 3500.0f) {
        u16 to = yaw16(RF(mon + 0x40) - RF(npc + PL_POS), RF(mon + 0x48) - RF(npc + PL_POS + 8));
        if (dm > 380.0f * 380.0f) {  // run up to it
            set_state(S_CHASE);
            stick_to(to);
            if (!drawn && dm > 1200.0f * 1200.0f) v_cur |= B_R;  // (dash with the weapon away)
            return;
        }
        // In reach: turn to it and swing. X, X, A... one press every 24 frames; the stick points the first blow.
        set_state(S_ATTACK);
        u32 ph = g_t % 24;
        if (ph < 6) {
            stick_to(to);
            v_cur = (g_combo % 3 == 2) ? B_A : B_X;
        }
        if (ph == 5) g_combo++;
        return;
    }
    // No monster near: come along with us; the weapon away for running (not while we are down or in another zone).
    if (we_are_down || R8(npc + PL_AREA) != R8(mine + PL_AREA)) { set_state(S_IDLE); return; }
    if (dp > 600.0f * 600.0f) {
        if (drawn) {
            set_state(S_SHEATHE);
            if (g_t % 30 < 4) v_cur = B_Y;  // (Y with the weapon out puts it away; never pressed with it away)
            return;
        }
        set_state(S_FOLLOW);
        stick_to(yaw16(RF(mine + PL_POS) - RF(npc + PL_POS), RF(mine + PL_POS + 8) - RF(npc + PL_POS + 8)));
        if (dp > 1200.0f * 1200.0f) v_cur |= B_R;
        return;
    }
    set_state(S_IDLE);
}


// ------------------------------------------------------------------ the quest's end
// After the quest is cleared the game counts down (60 s) to the return; with the helper it did not. Logged: the words of
// sQuest +0x40..+0xff that change (each second at most), with the helper's action, to see what it waits for.
static u32 g_qw[48], g_qw_at;
static bool g_qw_init;
static void quest_watch(void) {
    if (g_frames - g_qw_at < 60) return;
    g_qw_at = g_frames;
    u32 q = R32(A(0x1a8ad90u));
    if (!sane(q)) return;
    for (u32 i = 0; i < 48; i++) {
        u32 v = R32(q + 0x40 + 4 * i);
        if (g_qw_init && v != g_qw[i] && i != 32 && i != 33)  // (+0xc0 / +0xc4: the carts, logged elsewhere)
            forge_log(LOG_INFO, "[npc-quest] sQuest +%03x: %08x -> %08x (helper act %04x:%04x)", 0x40 + 4 * i, g_qw[i], v,
                      g_npc ? R16(g_npc + 0x2506) : 0, g_npc ? R16(g_npc + 0x250a) : 0);
        g_qw[i] = v;
    }
    g_qw_init = true;
}


// ------------------------------------------------------------------ the helper's cart
// A local hunter's death counts a faint: 0x5a9754(sQuest, 0, its slot). Offline that ignores the slot and counts it for
// the quest's own member (sQuestSelector +0xd56 -> member entry +0xd758: our slot), and a faint of OUR slot is what sends
// this game's hunter to camp (0x5a9934: message 3, the area reloads at the camp, 0x8a0bcc -> 0x4778cc -> vtable +0x370).
// So for the helper's death the member entry says its slot for that one call: the game counts the faint as the helper's
// ("<name> fainted", message 4) and we stay where we are.
// Then what the helper's own game would do: after its fall, it comes back at the camp's first start point (0x700420,
// as 0x8a0bcc finds it) through the same respawn method (vtable +0x370, 0x4ad6fc) the game uses for ours.
#define FN_FAINT      0x5a9754u
#define FN_START_POS  0x700420u  // (stage, index, 0, vec4 *out, u32 *yaw out)
#define G_STAGE       0x1a977a0u // * the quest's stage (+0x388: start points)
#define SEL_MY_MEMBER 0xd56u
#define SEL_MEM_SLOT  0xd758u
#define FN_CART       0x5ae550u  // (sQuest): the fall's end (0x4c3430, a local hunter, after its timer): to camp
#define FN_CART_POS   0x70049cu  // (stage, index, 0, vec4 *out, u32 *yaw out): the start point a cart uses (0x8a0c98)
#define FN_NET_BACK   0x4011b8u  // (?, packet, slot): a remote hunter says where it came back (0x401128 sends it)
#define CART_DELAY    60         // frames from the helper's fall to the camp (asked: at most 1 s; the game waits ~10 s)
#define PL_MAX_HP     0x13d8u
#define PL_RED_HP     0x13dau
typedef void (*fn_faint)(u32, u32, u32);
typedef void (*fn_start_pos)(u32, u32, u32, u32, u32);
typedef void (*fn_cart)(u32);
typedef void (*fn_net_back)(u32, u32, u32);
static fn_faint g_faint_orig;
static fn_cart g_cart_orig;
static u32 g_cart_at;         // frame the helper's cart began (0: none pending)
static u32 g_wake_check;      // frame to check the helper's health after waking (0: none)
static u32 g_my_cart_at;      // frame our own cart began (for the timing)

static void faint_hook(u32 q, u32 kind, u32 slot) {
    u32 net = R32(A(G_NETWORK)), sel = R32(A(G_SELECTOR));
    bool offline = sane(net) && R8(net + NET_ONLINE) == 0;
    if (!(g_in_npc_move && kind == 0 && offline && sane(sel) && g_npc_slot >= 0)) { g_faint_orig(q, kind, slot); return; }
    int idx = (int8_t)R8(sel + SEL_MY_MEMBER);
    u32 e = sel + SEL_MEM_SLOT + 12u * (u32)(idx < 0 ? 0 : idx);
    u8 was = R8(e);
    R8(e) = (u8)g_npc_slot;
    g_faint_orig(q, kind, slot);
    R8(e) = was;
    forge_log(LOG_INFO, "[npc-cart] frame %u: the helper's faint counted as its own (slot %d)", g_frames, g_npc_slot);
    // The same test as the fall's end (0x5ae550): no cart when the faints are used up, no reward is left or the quest is won.
    bool lost = R8(q + 0x166a) >= R8(q + 0x1669) || (int32_t)R32(q + 0xc0) < 1;
    if (R8(q + 0x1777) != 0 || !(lost || R8(q + 0x55) >= 9)) g_cart_at = g_frames;
}

// 0x5ae550 is where a fallen LOCAL hunter's cart begins: unless the quest is lost (faints used up, no reward left) or
// already won (+0x55 >= 9), it sets the stage's flags |= 0xe, and the scene then reloads the area at the camp for this
// game's hunter (0x89f4d8 -> 0x8a0bcc -> 0x4778cc). The helper's fall runs it too (it is local during its own move): that
// is what took us to the camp with it. So for the helper the stage is left alone, and the helper comes back the way a
// remote hunter does when its own game has carted it: the "came back" message (0x4011b8) with mode 3 (cart), which
// sets +0x1470 = 3 and respawns it (vtable +0x370: mode 3 -> the waking-up action 0x200, 0x4ada34).
static void cart_hook(u32 q) {
    if (!g_in_npc_move) {
        g_my_cart_at = g_frames;
        g_me_carted = true;
        forge_log(LOG_INFO, "[npc-cart] frame %u: our cart begins (stage flags %08x)", g_frames,
                  sane(R32(A(G_STAGE))) ? R32(R32(A(G_STAGE)) + 0x1a4) : 0);
        g_cart_orig(q);
        return;
    }
    bool lost = R8(q + 0x166a) >= R8(q + 0x1669) || (int32_t)R32(q + 0xc0) < 1;
    if (R8(q + 0x1777) == 0 && (lost || R8(q + 0x55) >= 9)) {
        forge_log(LOG_INFO, "[npc-cart] frame %u: the helper's fall ends, no cart (faints %u/%u, reward %d, quest state %u)", g_frames,
                  R8(q + 0x166a), R8(q + 0x1669), (int32_t)R32(q + 0xc0), R8(q + 0x55));
        g_cart_orig(q);  // (nothing, or the won quest's own state)
        return;
    }
    forge_log(LOG_INFO, "[npc-cart] frame %u: the helper's fall ends (its cart was started at the faint; the stage is left alone)", g_frames);
}

// A local hunter's fall also points THIS machine's camera at it: 0x219384(camera manager [0x1a4d750], hunter, 8, 0),
// called from the fall (0x4c383c, 0x4c349c) only when the hunter is local. The helper is local during its move, so its
// fall took our camera (the view from above on it, back to us some seconds later). Calls aimed at the helper from
// inside its move are dropped: a remote hunter's fall does not move our camera.
#define FN_CAM_EVENT  0x219384u
typedef void (*fn_cam_event)(u32, u32, u32, u32);
static fn_cam_event g_cam_orig;
static u32 g_cam_drops;
static void cam_hook(u32 mgr, u32 pl, u32 kind, u32 arg) {
    if (g_in_npc_move && g_npc && pl == g_npc) {
        if (g_cam_drops++ < 20) forge_log(LOG_INFO, "[npc-cart] frame %u: camera event %u on the helper dropped", g_frames, kind);
        return;
    }
    g_cam_orig(mgr, pl, kind, arg);
}

// The fall's own camera: at the start of a fall (0x4c33ac) a LOCAL hunter calls 0x4a0698(hunter, kind), which starts the
// camera manager's fall view on it (0x219240: the view from above) and returns 1; the fall then marks +6 and, at its end,
// gives the camera back (event 8 above). For a hunter that is not local it returns 0 and does nothing. For the helper
// (local only during its move) it now answers as for a remote hunter: 0, no camera, nothing to give back.
#define FN_FALL_CAM   0x4a0698u
typedef u32 (*fn_fall_cam)(u32, u32);
static fn_fall_cam g_fall_cam_orig;
static u32 fall_cam_hook(u32 pl, u32 kind) {
    if (g_in_npc_move && g_npc && pl == g_npc) {
        forge_log(LOG_INFO, "[npc-cart] frame %u: the fall's camera on the helper not started (kind %u)", g_frames, kind);
        return 0;
    }
    return g_fall_cam_orig(pl, kind);
}

static void cart_check(u32 npc, u32 mine) {
    (void)mine;
    if (g_wake_check && g_frames >= g_wake_check) {
        g_wake_check = 0;
        forge_log(LOG_INFO, "[npc-cart] frame %u: helper after waking: hp %d/%d act %04x:%04x zone %u 1470 %u", g_frames,
                  (int)(s16)R16(npc + PL_HP), (int)(s16)R16(npc + PL_MAX_HP), R16(npc + PL_ACT_ST), R16(npc + PL_ACT_NO),
                  R8(npc + PL_AREA), R32(npc + PL_WAIT));
        if ((s16)R16(npc + PL_HP) <= 0 && (s16)R16(npc + PL_MAX_HP) > 0) {  // a cart gives the health back
            R16(npc + PL_HP) = R16(npc + PL_MAX_HP);
            R16(npc + PL_RED_HP) = R16(npc + PL_MAX_HP);
            forge_log(LOG_WARN, "[npc-cart] the helper woke with no health: given back (%d)", (int)(s16)R16(npc + PL_HP));
        }
    }
    if (!g_cart_at || g_frames - g_cart_at < CART_DELAY) return;
    g_cart_at = 0;
    u32 stage = R32(A(G_STAGE));
    if (!sane(stage)) { forge_log(LOG_WARN, "[npc-cart] no stage: the helper stays down"); return; }
    volatile u32 pkt[12];  // as 0x401128 builds it: +0x10 position (x y z 0), +0x20 u16 yaw, +0x22 zone, +0x23 mode
    for (int i = 0; i < 12; i++) pkt[i] = 0;
    volatile u32 yaw = 0;
    ((fn_start_pos)A(FN_CART_POS))(stage, 0, 0, (u32)(uintptr_t)&pkt[4], (u32)(uintptr_t)&yaw);
    pkt[7] = 0;
    pkt[8] = (yaw & 0xffffu) | (0u << 16) | (3u << 24);  // zone 0 (the camp, as the scene sets it: 0x89f62c), mode 3
    ((fn_net_back)A(FN_NET_BACK))(0, (u32)(uintptr_t)pkt, (u32)g_npc_slot);
    g_back_at = g_frames;
    g_wake_check = g_frames + 240;
    forge_log(LOG_INFO, "[npc-cart] frame %u: helper back at the camp (%d %d %d, yaw %04x): hp %d act %04x:%04x zone %u 1470 %u",
              g_frames, (int)*(volatile float *)&pkt[4], (int)*(volatile float *)&pkt[5], (int)*(volatile float *)&pkt[6], yaw & 0xffffu,
              (int)(s16)R16(npc + PL_HP), R16(npc + PL_ACT_ST), R16(npc + PL_ACT_NO), R8(npc + PL_AREA), R32(npc + PL_WAIT));
    toast("O Ajudante foi levado para o acampamento", false);
}

// ------------------------------------------------------------------ metrics: who the monsters go for
// A large monster's status block keeps an attention score per hunter (+0x254, 4 x s32, by slot) and per Palico (+0x264,
// 2 x s32); the target pickers (0x2c5b14) take the highest (docs/STATUS.md of the port, "Monsters' attention scores").
// The bytes at +0x230..+0x237 are its current targets (logged raw: kind / index pairs).
#define INFO_SCORE_PL 0x254u
#define INFO_SCORE_OT 0x264u
#define INFO_TARGETS  0x230u
#define EN_ACTION     0x73e0u
static u32 g_top[7];          // seconds each one had the highest score of the zone's large monsters: 4 hunters, 2 Palicos, none
static u32 g_stat_at;
static int g_hp_npc_prev = -1, g_hp_me_prev = -1;
static u32 g_dmg_npc, g_dmg_me, g_falls_npc, g_falls_me;
static bool g_quest_on;

static void stats_reset(void) {
    for (int i = 0; i < 7; i++) g_top[i] = 0;
    g_hp_npc_prev = g_hp_me_prev = -1;
    g_dmg_npc = g_dmg_me = g_falls_npc = g_falls_me = 0;
}

static void stats_summary(const char *when) {
    forge_log(LOG_INFO, "[npc-stats] %s: seconds as the monsters' first choice: you %u, helper %u, slot2 %u, slot3 %u, "
              "palico1 %u, palico2 %u, nobody %u", when, g_top[0], g_top[1], g_top[2], g_top[3], g_top[4], g_top[5], g_top[6]);
    forge_log(LOG_INFO, "[npc-stats] %s: damage taken: you %u, helper %u; falls: you %u, helper %u", when, g_dmg_me, g_dmg_npc,
              g_falls_me, g_falls_npc);
}

// Each frame in a quest: damage and falls; each second: every large monster of our zone, its scores and targets.
static void stats_frame(u32 sp, u32 mine) {
    int hm = (s16)R16(mine + PL_HP);
    if (g_hp_me_prev >= 0 && hm < g_hp_me_prev) { g_dmg_me += (u32)(g_hp_me_prev - hm); if (hm <= 0) g_falls_me++; }
    g_hp_me_prev = hm;
    if (g_npc && R32(sp + SP_SLOTS + 4 * (u32)g_npc_slot) == g_npc) {
        int hn = (s16)R16(g_npc + PL_HP);
        if (g_hp_npc_prev >= 0 && hn < g_hp_npc_prev) { g_dmg_npc += (u32)(g_hp_npc_prev - hn); if (hn <= 0) g_falls_npc++; }
        g_hp_npc_prev = hn;
    }
    if (g_frames - g_stat_at < 60) return;
    g_stat_at = g_frames;
    u32 sen = R32(A(G_SENEMY));
    if (!sane(sen)) return;
    for (u32 k = 0; k < 10; k++) {
        u32 en = R32(sen + 0x1c + 4 * k);
        if (!sane(en)) continue;
        u32 info = R32(en + EN_INFO);
        if (!sane(info) || R16(info + 0x1b6) >= 10000 || (int32_t)R32(info + 0x370) <= 0) continue;
        int32_t sc[6];
        int best = 6;
        int32_t bv = 0;
        for (u32 i = 0; i < 6; i++) {
            sc[i] = (int32_t)R32(info + (i < 4 ? INFO_SCORE_PL + 4 * i : INFO_SCORE_OT + 4 * (i - 4)));
            if (sc[i] > bv) bv = sc[i], best = (int)i;
        }
        if (R8(en + PL_AREA) == R8(mine + PL_AREA)) g_top[best]++;
        float dme = dist2(info + 0x40, mine + PL_POS), dnpc = g_npc ? dist2(info + 0x40, g_npc + PL_POS) : 0;
        int ime = 0, inpc = 0;  // (whole metres, from the squares: a rough root)
        while ((float)(ime + 1) * (ime + 1) * 10000.0f <= dme && ime < 999) ime++;
        while ((float)(inpc + 1) * (inpc + 1) * 10000.0f <= dnpc && inpc < 999) inpc++;
        forge_log(LOG_INFO, "[npc-mon] monster %u zone %u hp %d act %08x | targets %02x%02x %02x%02x %02x%02x %02x%02x | "
                  "scores you %d helper %d s2 %d s3 %d pal %d %d | %d m from you, %d m from helper", R16(info + 0x1b6),
                  R8(en + PL_AREA), (int)(int32_t)R32(info + 0x370), R32(en + EN_ACTION), R8(info + INFO_TARGETS),
                  R8(info + INFO_TARGETS + 1), R8(info + INFO_TARGETS + 2), R8(info + INFO_TARGETS + 3),
                  R8(info + INFO_TARGETS + 4), R8(info + INFO_TARGETS + 5), R8(info + INFO_TARGETS + 6),
                  R8(info + INFO_TARGETS + 7), sc[0], sc[1], sc[2], sc[3], sc[4], sc[5], ime, inpc);
    }
}

// ------------------------------------------------------------------ faints: what the game does, logged
// The quest keeps the faints (sQuest, 0x5a9934): +0x166a the quest's count, +0x166b + slot each hunter's, +0xc0 what is
// left of the quest's allowance and +0xc4 what a faint costs (at 0 the quest fails). The death code (0x4c3220) counts a
// faint only for a hunter of this game (+0x13cc bit 0, 0x5a9754), and nothing more for others: how a hunter gets from
// fainting to the camp is what this logs, action by action, for ours and the helper.
#define G_SQUEST     0x1a8ad90u
#define PL_ACT_ST    0x2506u
#define PL_ACT_NO    0x250au
static u32 g_act_me, g_act_npc, g_faint_sig;
static bool g_down;

// The monsters' attention (0x2bbf98) skips a hunter whose bit is set in sEnemy +0x25ac (0x26946c). Offline the game sets
// every slot's but ours (0x2692ac, at each zone's setup), as nobody else can be there: the helper's is cleared again.
#define EN_IGNORE    0x25acu
#define FN_PL_READY  0x47c664u   // (player): vtable +0x88, the hunter counts for the attention too
static u32 g_ignore_fixes;
static void let_monsters_see(u32 npc_slot) {
    u32 sen = R32(A(G_SENEMY));
    if (!sane(sen)) return;
    u32 m = R32(sen + EN_IGNORE);
    if (m & (1u << npc_slot)) {
        R32(sen + EN_IGNORE) = m & ~(1u << npc_slot);
        if (g_ignore_fixes++ < 10) forge_log(LOG_INFO, "[npc] monsters were told to ignore the helper (mask %08x): undone", m);
    }
}

// The mask is set again every frame before the attention runs (0.1.10 cleared it after: too late), so the check itself
// (0x26946c: sEnemy, slot -> 1 ignored) answers "not ignored" for the helper's slot.
#define FN_EN_IGNORED 0x26946cu
typedef u32 (*fn_ignored)(u32, u32);
static fn_ignored g_ignored_orig;
static u32 ignored_hook(u32 sen, u32 slot) {
    if (g_npc && (int)slot == g_npc_slot) return 0;
    return g_ignored_orig(sen, slot);
}

static u32 g_trace_until;  // frames: every action and HP change of the helper logged till then (after a fall)
static s16 g_trace_hp = -1;

static void trace_actions(u32 sp, u32 mine) {
    u32 a = (u32)R16(mine + PL_ACT_ST) << 16 | R16(mine + PL_ACT_NO);
    s16 hp = (s16)R16(mine + PL_HP);
    if (a != g_act_me && (hp <= 0 || (a >> 16) >= 0x100 || (g_act_me >> 16) >= 0x100))
        forge_log(LOG_INFO, "[npc-cart] frame %u: you: act %04x:%04x -> %04x:%04x hp %d at %d %d %d zone %u", g_frames, g_act_me >> 16,
                  g_act_me & 0xffff, a >> 16, a & 0xffff, (int)hp, (int)RF(mine + PL_POS), (int)RF(mine + PL_POS + 4),
                  (int)RF(mine + PL_POS + 8), R8(mine + PL_AREA));
    g_act_me = a;
    if (g_npc && R32(sp + SP_SLOTS + 4 * (u32)g_npc_slot) == g_npc) {
        u32 b = (u32)R16(g_npc + PL_ACT_ST) << 16 | R16(g_npc + PL_ACT_NO);
        s16 hn = (s16)R16(g_npc + PL_HP);
        if (b != g_act_npc && (hn <= 0 || (b >> 16) >= 0x100 || (g_act_npc >> 16) >= 0x100))
            forge_log(LOG_INFO, "[npc-cart] frame %u: helper: act %04x:%04x -> %04x:%04x hp %d at %d %d %d zone %u", g_frames, g_act_npc >> 16,
                      g_act_npc & 0xffff, b >> 16, b & 0xffff, (int)hn, (int)RF(g_npc + PL_POS), (int)RF(g_npc + PL_POS + 4),
                      (int)RF(g_npc + PL_POS + 8), R8(g_npc + PL_AREA));
        if (g_frames < g_trace_until && (b != g_act_npc || hn != g_trace_hp))
            forge_log(LOG_INFO, "[npc-cart] helper (after its fall): act %04x:%04x hp %d at %d %d %d zone %u", b >> 16, b & 0xffff,
                      (int)hn, (int)RF(g_npc + PL_POS), (int)RF(g_npc + PL_POS + 4), (int)RF(g_npc + PL_POS + 8), R8(g_npc + PL_AREA));
        g_trace_hp = hn;
        g_act_npc = b;
        bool down = hn <= 0;
        if (down && !g_down) { toast("O Ajudante caiu", false); g_trace_until = g_frames + 60 * 60; }
        g_down = down;
    }
    u32 q = R32(A(G_SQUEST));
    if (!sane(q)) return;
    u32 sig = R8(q + 0x166a) | R8(q + 0x166b) << 8 | R8(q + 0x166c) << 16 | R8(q + 0x166d) << 24;
    sig ^= R32(q + 0xc0) * 2654435761u;
    if (sig != g_faint_sig) {
        g_faint_sig = sig;
        forge_log(LOG_INFO, "[npc-cart] quest faints %u (slots %u %u %u %u), allowance left %d, a faint costs %d, quest state %u %u",
                  R8(q + 0x166a), R8(q + 0x166b), R8(q + 0x166c), R8(q + 0x166d), R8(q + 0x166e), (int)R32(q + 0xc0),
                  (int)R32(q + 0xc4), R32(q + 0x4c), R8(q + 0x50));
    }
}

void forge_onLoad(PluginInitParam *p) {
    p->required.major = 2, p->required.minor = 0, p->required.patch = 0;
    const char *n = NPC_NAME;
    for (int i = 0; n[i] && i < 63; i++) p->name[i] = n[i];
}

void forge_onUpdate(float dt) {
    (void)dt;
    if (!g_text) {
        g_text = forge_mem_getMainTextAddr();
        forge_log(LOG_INFO, "[npc] " NPC_NAME " loaded (main at %08x). F4 helper on/off, F5 make it now, F6 log, F7 its moving on/off", g_text);
    }
    g_frames++;
    static bool hooked;
    if (!hooked) {
        hooked = true;
        forge_hook_create((void *)(uintptr_t)A(FN_DEPART), (void *)depart_hook, (void **)&g_depart_orig);
        forge_hook_create((void *)(uintptr_t)A(FN_MAKE_ME), (void *)make_me_hook, (void **)&g_make_me_orig);
        forge_hook_create((void *)(uintptr_t)A(FN_PL_PAD), (void *)pl_pad_hook, (void **)&g_pl_pad_orig);
        forge_hook_create((void *)(uintptr_t)A(FN_PL_INPUT), (void *)pl_input_hook, (void **)&g_pl_input_orig);
        forge_hook_create((void *)(uintptr_t)A(FN_PL_MOVE), (void *)pl_move_hook, (void **)&g_pl_move_orig);
        forge_hook_create((void *)(uintptr_t)A(FN_EN_IGNORED), (void *)ignored_hook, (void **)&g_ignored_orig);
        forge_log(LOG_INFO, "[npc] attention hook %s", g_ignored_orig ? "ok" : "FAILED");
        forge_hook_create((void *)(uintptr_t)A(FN_FAINT), (void *)faint_hook, (void **)&g_faint_orig);
        forge_hook_create((void *)(uintptr_t)A(FN_CART), (void *)cart_hook, (void **)&g_cart_orig);
        forge_log(LOG_INFO, "[npc] cart hook %s", g_cart_orig ? "ok" : "FAILED");
        forge_hook_create((void *)(uintptr_t)A(FN_CAM_EVENT), (void *)cam_hook, (void **)&g_cam_orig);
        forge_hook_create((void *)(uintptr_t)A(FN_FALL_CAM), (void *)fall_cam_hook, (void **)&g_fall_cam_orig);
        forge_log(LOG_INFO, "[npc] camera hooks %s %s", g_cam_orig ? "ok" : "FAILED", g_fall_cam_orig ? "ok" : "FAILED");
        forge_log(LOG_INFO, "[npc] faint hook %s", g_faint_orig ? "ok" : "FAILED");
        forge_log(LOG_INFO, "[npc] pad hooks %s %s, move hook %s", g_pl_pad_orig ? "ok" : "FAILED", g_pl_input_orig ? "ok" : "FAILED",
                  g_pl_move_orig ? "ok" : "FAILED");
        forge_log(LOG_INFO, "[npc] hooks: depart %s, our hunter's making %s", g_depart_orig ? "ok" : "FAILED",
                  g_make_me_orig ? "ok" : "FAILED");
    }
    if (forge_input_isKeyPressed(KEY_F4)) {
        g_enabled = !g_enabled;
        forge_log(LOG_INFO, "[npc] helper %s (from the next quest)", g_enabled ? "on" : "off");
        toast(g_enabled ? "Ajudante: LIGADO (entra na proxima missao)" : "Ajudante: DESLIGADO", g_enabled);
    }
    if (forge_input_isKeyPressed(KEY_F7)) {
        g_drive = !g_drive;
        toast(g_drive ? "Ajudante: IA LIGADA" : "Ajudante: IA PARADA", g_drive);
        forge_log(LOG_INFO, "[npc] driving %s", g_drive ? "on" : "off");
    }
    if (forge_input_isKeyPressed(KEY_F5)) spawn();
    if (forge_input_isKeyPressed(KEY_F6)) {
        report();
        dump_members("F6");
        if (g_quest_on) stats_summary("so far");
        toast("Estado do Ajudante guardado no log.txt", true);
    }

    // The helper made during loading: beside us once we are in a zone.
    u32 sp = R32(A(G_SPLAYER)), me = my_slot(), mine = sane(sp) ? my_hunter(sp, me) : 0;
    bool in_quest = sane(mine) && R32(mine + PL_MODE) == 0;
    if (!in_quest) {
        g_seen_since = 0;
        if (g_quest_on) { g_quest_on = false; stats_summary("quest over"); }
        return;
    }
    if (!g_seen_since) g_seen_since = g_frames;
    if (!g_quest_on) { g_quest_on = true; stats_reset(); }
    if (g_placed && g_npc && g_npc_owner == mine && R32(sp + SP_SLOTS + 4 * (u32)g_npc_slot) == g_npc) unstick_fall(g_npc);
    quest_watch();
    if (g_placed && g_npc && g_npc_owner == mine && R32(sp + SP_SLOTS + 4 * (u32)g_npc_slot) == g_npc) cart_check(g_npc, mine);
    stats_frame(sp, mine);
    trace_actions(sp, mine);
    if (g_npc && g_npc_owner == mine && R32(sp + SP_SLOTS + 4 * (u32)g_npc_slot) == g_npc) let_monsters_see((u32)g_npc_slot);
    if (g_placed && g_drive && !g_down && g_npc && g_npc_owner == mine && R32(sp + SP_SLOTS + 4 * (u32)g_npc_slot) == g_npc) {
        think(g_npc, mine);
        if (g_frames - g_log_at > 180) {
            g_log_at = g_frames;
            u16 cy = 0;
            camera_yaw(&cy);
            forge_log(LOG_INFO, "[npc] helper %s: at %d %d zone %u hp %d drawn %u | move %u refresh %u pad %u | stick %04x/%u cam %04x",
                      state_name[g_state < 0 ? 0 : g_state], (int)RF(g_npc + PL_POS), (int)RF(g_npc + PL_POS + 8),
                      R8(g_npc + PL_AREA), (int)(s16)R16(g_npc + PL_HP), R8(g_npc + PL_DRAWN), g_move_calls, g_refresh_calls, g_pad_calls,
                      v_angle, v_mag, cy);
            {
                u32 sen = R32(A(G_SENEMY));
                forge_log(LOG_INFO, "[npc]   counts for the monsters: ready %u, ignore mask %08x",
                          ((fn_u32_u32)A(FN_PL_READY))(g_npc), sane(sen) ? R32(sen + EN_IGNORE) : 0xffffffffu);
            }
            forge_log(LOG_INFO, "[npc]   input held %04x new %04x word %08x dir %04x far %u | 13cc %08x 1464 %d 1470 %08x | act %04x %04x",
                      R16(g_npc + 0x13f4), R16(g_npc + 0x13f8), R32(g_npc + 0x13fc), R16(g_npc + 0x140c), R16(g_npc + 0x1410),
                      R32(g_npc + 0x13cc), (int)(RF(g_npc + 0x1464) * 100), R32(g_npc + 0x1470), R16(g_npc + 0x2506), R16(g_npc + 0x250a));
        }
    }
    if (!g_placed && g_npc && g_npc_owner == mine && g_frames - g_seen_since > 30 && R8(mine + PL_AREA) != 255) {
        if (R32(sp + SP_SLOTS + 4 * (u32)g_npc_slot) == g_npc) { place(g_npc, mine); report(); }
        else { forge_log(LOG_WARN, "[npc] the helper is gone from its slot"); g_placed = true; }
    }
}

// The message over the game: a small box at the top centre.
void forge_onImGuiFreeRender(void) {
    if (!g_toast || g_frames >= g_toast_until) return;
    u32 vp = (u32)(uintptr_t)igGetMainViewport();
    float w = vp ? RF(vp + 16) : 1280.0f, top = vp ? RF(vp + 12) : 0.0f, left = vp ? RF(vp + 8) : 0.0f;
    igSetNextWindowPos((ImVec2){left + w * 0.5f, top + 40.0f}, 1, (ImVec2){0.5f, 0.0f});
    igSetNextWindowBgAlpha(0.7f);
    if (igBegin("##npc_toast", 0, 0x3136f)) {
        if (g_toast_good) igTextColored(0.55f, 1.0f, 0.55f, 1.0f, "%s", g_toast);
        else igTextColored(1.0f, 0.6f, 0.45f, 1.0f, "%s", g_toast);
    }
    igEnd();
}
