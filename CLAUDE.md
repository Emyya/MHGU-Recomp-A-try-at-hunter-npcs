# NPC Hunter — an AI helper hunter for MHGU (MHGURecomp)

A Forge plugin that puts a second, AI-controlled hunter (the "Ajudante", Portuguese for "helper") into Monster Hunter
Generations Ultimate quests running on **MHGURecomp 0.18.0** (native PC port, github.com/morerokk/MHGURecomp). The game
treats it as a real player: it shows in the HUD's party list, counts towards monster HP (the port's `[Monster HP]`
option), moves, attacks and uses up carts.

- **Current version:** 0.1.12, the base version (see "Current state"; past versions: [HISTORY.md](HISTORY.md)).
- **Language:** C, built for 32-bit ARM (the Switch), packaged as a `.nro` (Forge plugin).
- **Installed at:** `mods\NPC-Hunter\romfs\nativeNX\plugins\npc_hunter.nro` in the MHGURecomp folder.
- **Source:** the `src\` folder next to this file.
- **Game:** MHGU Switch 1.4.0. Every address below is for this version.
- On-screen messages are in Portuguese (Portugal), without accents.

## How to play (keys)

| Key | Does |
|---|---|
| F4 | turns the helper on / off (takes effect from the next quest; on at start) |
| F5 | makes the helper right now, in the middle of a quest (test; not a member from the start) |
| F6 | writes the state to `log.txt` and shows "Estado do Ajudante guardado no log.txt" |
| F7 | turns the AI on / off (the helper stands still) |

On-screen messages (Forge's ImGui overlay, no accents: the font may lack them): F4, F7, F6, "O Ajudante entrou na
missao (NPC Hunter x.y.z)" (the helper joined the quest) and "O Ajudante caiu" (the helper fell).

## Current state (0.1.12)

Works:
- The helper joins the quest while it loads, at the same step where the game makes the other players' hunters. It takes
  the free slot (usually 1), with a copy of your hunter data (weapon, armour, looks) and the name "Ajudante".
- It shows in the HUD party list; the port counts 2 hunters (`[hpscale] ... 2 hunter(s)`).
- It moves, follows you, runs to the zone's large monster, attacks (X, X, A), rolls when hit and sheathes to run. In
  another zone than yours for more than 3 s, it appears next to you.
- Monsters pay attention to it (hook on `0x26946c`).
- Its faint counts as its own ("Ajudante fainted", quest faint slots `0 1 0 0`).
- **Its cart:** it is carried to the camp and wakes up in the tent with full HP; you stay where you are and your camera
  is not touched. Why this needed work: during its move the helper is local, so its fall ran this machine's local-only
  parts:
  - the fall's camera: `0x4a0698` starts the camera manager's view from above on it (`0x219240`); the fall marks `+6`
    and gives the camera back at its end (`0x219384`, event 8);
  - the cart: at the end of the fall (600-frame timer `+0x27a0`, then `0x4c3430`) `0x5ae550(sQuest)` sets the stage
    flags `|= 0xe` and the scene reloads **this machine's** hunter at the camp.
  Inside the helper's move, `0x4a0698` answers 0 (as for a remote hunter), event-8 calls on it are dropped and
  `0x5ae550` leaves the stage alone. The helper comes back the way a remote hunter does after its own game carted it:
  the "came back" message `0x4011b8` with mode 3 (`+0x1470 = 3`, respawn vtable +0x370 → waking-up action `0x200`).
  If it ever wakes at 0 HP, the plugin gives it back (logged).
- **1 s cart** (the owner's choice; the game itself waits ~10 s): the cart starts at the faint (`faint_hook`), with the
  same test as `0x5ae550` (no cart when faints are used up, no reward is left or the quest is won); camp 60 frames later.
- **When you cart, the helper keeps fighting:** no teleport to you while you are fallen (act 0x100), waking (0x200) or
  still in the camp after your cart (`g_me_carted`, cleared when you leave zone 0). With no monster there it waits.

To do:
1. Cart: the Felyne cart animation is not shown.
2. Palicos: 2 come along (yours). With 2 hunters the total matches the normal game, but both are yours.
3. AI: large monsters only, no items, no dodging before a hit, no changing zones on its own (only the teleport).

Design rule: no made-up shortcuts (e.g. "gets up after 5 s"): behaviour must be the game's own, unless the owner asks
for a change (as for the 1 s cart).

## How it works (architecture)

A hunter in the game is either **local** (this machine's: reads the pad, its death costs carts, camera, HUD) or
**remote** (another player's: only replays what arrives over the network). The mark is bit 0 of `player +0x13cc`.

The helper is a hybrid:
1. **Creation (remote):** `make_me_hook` hooks `0x4758a4` (the quest's loading makes your hunter). Right after it the
   plugin copies your data (`sPlayer +0x7c`) to the free slot's member data (`sPlayer +0x5dc + slot*0x560`), marks it
   valid, renames it and calls `0x474cf0(sPlayer, slot, 0)`, which is what the game does for the other players in an
   online quest.
2. **Virtual pad:** `pl_input_hook` hooks `0x47c97c` (every hunter's input update). For the helper it calls the pad
   reading (`0x4a79cc`) through `pl_pad_hook`, which swaps the pad's gameplay view (`sFestaPad +0x6b9c..+0x6bb7`) for the
   virtual pad's values (`v_cur`, `v_angle`, `v_mag`) for the call and puts it back afterwards.
3. **Local only during its update:** `pl_move_hook` hooks `0x49b65c` (the hunter's update) and sets bit 0 of `+0x13cc`
   for that call only, so the buttons turn into moves. Outside it, the helper is remote again (local all the time =
   invisible).
4. **The quest's opening hold:** on a local hunter's first update the game sets `+0x1470 = 1` (waiting for the quest's
   opening, `0x49b7d4` via vtable +0x374). Nothing lifts it for the helper, so `pl_move_hook` puts the old value back.
5. **AI (`think`, once a frame in `forge_onUpdate`):** picks the nearest live large monster in its zone, decides
   (follow / chase / attack / roll / sheathe) and writes the virtual pad for the next frame. Direction:
   `stick_angle = wanted_yaw - camera_yaw - 0xC000` (the game's own formula, `0x47f5d0`), so no calibration is needed.
6. **Monster attention:** `let_monsters_see` clears the helper's bit in `sEnemy +0x25ac` every frame.

## Building and installing

Tools: `clang` (tested with 18), `ld.lld`, `python3`. No devkitPro.

```sh
cd src
bash build.sh          # npc.c + crt0.s -> npc.elf -> npc_hunter.nro (elf2nro.py)
```

- Flags: `--target=armv7a-none-eabi -march=armv8-a+crc -mfloat-abi=softfp -marm -fPIC -ffreestanding -fno-builtin
  -nostdlib`. Forge's API is **softfp** (floats in integer registers); the game's functions are hard-float, so game
  functions with float arguments are not called without a shim.
- Linking: `ld.lld -T switch32.ld -shared --version-script=exported.txt -init=init -fini=fini`. REL relocations only
  (`R_ARM_RELATIVE`, `JUMP_SLOT`, `GLOB_DAT`, `ABS32`): what the port's loader accepts.
- No libc: no `memcpy` / `atan2f` / libgcc divisions (the code has an approximate `atan2f_`).
- Export only: `init`, `fini`, `forge_onLoad`, `forge_onUpdate`, `forge_onImGuiFreeRender`.
- **Versions and builds:** `NPC_VERSION` in `npc.c` changes only when the owner decides a new version is due. Every build
  gets a random 8-hex-digit build id (`build.sh` makes it; `NPC_BUILD=xxxxxxxx sh build.sh` to choose one), shown as
  `NPC Hunter 0.1.12 (build xxxxxxxx)` in the log and in the join message. Refer to builds by that id.
- **Installing from Claude:** the copy into the game folder can pick up an older version of the file. Always give the output file
  a new name (e.g. `npc_hunter_<build>.nro`) and check the SHA-1 of the installed file.

## Testing and reading the log

The game's log is `log.txt` in the MHGURecomp folder (rewritten at every start). The plugin's lines:

| Prefix | What |
|---|---|
| `[npc]` | start, hooks, creation, placing, the AI's state every 3 s (position, zone, HP, weapon, hook counters, stick, camera, input received, `+0x13cc`, `+0x1470`, action, `ready`, ignore mask) |
| `[npc-mon]` | every second, each large monster: HP, action, target bytes `+0x230..+0x237`, attention scores (you, helper, slots 2-3, Palicos), distances |
| `[npc-stats]` | summary at the end of the quest and on F6: seconds as main target for each, damage taken, falls |
| `[npc-cart]` | fall / cart actions (yours and the helper's), the quest's cart counter, and 1 min of the helper's actions and HP after it falls |

Always check the first line: `[npc] NPC Hunter 0.1.x loaded`.

## Reference index (MHGU 1.4.0)

**Runtime** addresses = offset in the `main` image + 0x200000 (the port loads `main` at 0x200000; in the plugin:
`A(rt) = forge_mem_getMainTextAddr() + (rt - 0x200000)`). The globals are **pointers** to the object.

### Globals (pointers)

| Address | Object | Notes |
|---|---|---|
| 0x1a85b58 | `sPlayer*` | +0x1c: the 4 hunters by slot; +0x7c: your data; +0x5dc + slot*0x560: member data (quest); +0x1b5c + slot*0x560: member data (hub) |
| 0x1a8ca48 | `sGameControl*` | +0x30: your slot (`0x5f8398`); +0x49 current zone, +0x4a zone to load (a cart sets 0), +0x4b previous zone |
| 0x1a51fe0 | `sEnemy*` | +0x1c: the zone's 10 enemies; +0x25ac: mask of hunters to ignore |
| 0x1a843f0 | `sFestaNetwork*` | +0x63e: in a session (`0x427ba8`); +0xa90: each member's state (3 = in the quest) |
| 0x1a8ae38 | `sQuestSelector*` | +0xd68: ordered quest; +0xd750 `mJoinNum`; +0xd754: members (12 bytes each) — stays 0 offline |
| 0x1a8ad90 | `sQuest*` | +0x4c state; +0x55 u8 quest state (≥ 9: won; `0x5ae550` sets 0xc); +0xc0 cart allowance (quest fails at 0); +0xc4 cost of a cart; +0x1669 u8 faints allowed; +0x166a u8 the quest's faints; +0x166b + slot faints per player; +0x1777 u8 (≠ 0: `0x5ae550` carts anyway) |
| 0x1a977a0 | stage* | +0x1a4 flags: 2 reload the area, 4 at the camp's start point (`0x700420`), 8 at the cart's start point (`0x70049c`); a cart sets 0xe. +0x388: start points (13 max) |
| 0x1a4d750 | camera manager (events)* | +0x54 the active camera; `0x219240` starts a view on a hunter, `0x219384` / `0x219404` events |
| 0x1a570d0 | scene / event checker* | `0x34c8b8` scripted events |
| 0x1a8c670 | `sFestaPad*` | the pad's gameplay view at +0x6b98 (see "Pad") |
| 0x2326504 | camera manager (view)* | +0x54: the camera in use (eye +0x40, target +0x60) |

### Game functions

| Address | Signature | What it does |
|---|---|---|
| 0x4758a4 | (sPlayer, mode) | makes your hunter (mode 0 quest, 1 hub). **Hook:** make the helper right after |
| 0x474cf0 | (sPlayer, slot, mode) → player | makes that slot's hunter from its member data (the loader calls it for the other players, online only: `0x2066a4`) |
| 0x475a68 | (sPlayer, slot, mode, data) → player | low-level creation (called by the ones above) |
| 0x474ce4 | (sPlayer, slot) → player | the slot's hunter |
| 0x476278 | (sPlayer) → player | your hunter (by slot) |
| 0x46e600 | (dst, src) | copies hunter data (0x560 bytes) |
| 0x46fddc / 0x46fde8 | (data) / (data, bool) | data valid (`+0x504` bit 3): read / set |
| 0x5f8398 | (sGameControl) → slot | your slot |
| 0x49b65c | (player) | the hunter's update (vtable +0x24). **Hook:** local only during this call |
| 0x49b8e8 | (player) | the update's body; skips everything if `+0x1470 != 0` |
| 0x49b7d4 | (player) | first update; if local, vtable +0x374(1) → `+0x1470 = 1` |
| 0x47c97c | (player) | input update: if local (and `+0x1464 <= 0`, `+0x1470 == 0`) calls vtable +0x228, else clears. **Hook** |
| 0x4a79cc | (player) | reads the pad for the hunter (vtable +0x228). **Hook:** swapped for the virtual pad |
| 0x47bcd0 | (player) → bool | is local (`+0x13cc` bit 0) |
| 0x47c664 | (player) → bool | ready (vtable +0x88): `+0x13d0 == 3`, `+0x2501` bit 1, `+0x13d4` |
| 0x47f5d0 | (player) → yaw | move direction = camera yaw + stick (`+0x140c`) + 0xC000 |
| 0x4ab670 | (player, act_st, act_no, param) | "do" an action (vtable +0x2f8) |
| 0x4831e0 | (player, act_st, act_no, param) | "set" an action (e.g. back to idle) |
| 0x4c3220 | (player) | death routine; if local: `0x5a9754(sQuest, 0, slot)` takes a cart |
| 0x5a9754 → 0x5a9934 | (sQuest, …) | counts a cart (offline credits your slot). **Hook:** credited to the helper's slot |
| 0x5ae550 | (sQuest) | end of a local hunter's fall: unless the quest is lost / won, stage `+0x1a4 |= 0xe` = cart this machine's hunter. **Hook** |
| 0x4011b8 | (?, packet, slot) | a remote hunter came back: packet +0x10 pos, +0x20 yaw, +0x22 zone, +0x23 mode (3 = cart); vtable +0x374(mode), +0x370 respawn |
| 0x70049c | (stage, index, 0, vec4 *pos, u32 *yaw) | the start point a cart uses |
| 0x4ad6fc | (player, pos, yaw, zone, 0) | respawn (vtable +0x370): switch on `+0x1470` (3 → action 0x200, wake up at camp); ends with `+0x1470 = 0` |
| 0x5a9bb8 | (sQuest, kind, value) | the same, from the network (`netPLDeathCntUp`) |
| 0x4c3220 phases | | the fall action: routine bytes at player +4..+7 (+5 phase: 0 start, 1 lying, 2 timer done; +6 = 1 when the fall's camera started). Phase 0 (local): faint count, `0x4a0698` camera, timer `+0x27a0 = 600.0`; phase 1: timer (`0x73aa84`), then phase 2, camera event 8 if +6, `0x5ae550` |
| 0x4a0698 | (player, kind) → bool | local only: starts the fall's camera (`0x219240` on the events camera manager), returns 1. **Hook:** 0 for the helper |
| 0x219240 | (camMgr, player, …, player, float) | starts a camera view on a hunter (the fall's view from above) |
| 0x219384 | (camMgr, player, kind, 0) | camera event on a hunter (8 at the end of a fall; also `0x4c383c`). **Hook:** dropped for the helper inside its move |
| 0x89fb68 | (scene) | the quest scene's state machine (+4 routine bytes, +0xb8 mode: 3 → `0x89f4d8`) |
| 0x89f4d8 | (scene) | a scene mode that sets zone 0 (`gc +0x4a`) and stage flags `|= 6` |
| 0x8a0bcc | () | reloads the area: start point from the stage flags (`0x70049c` / `0x700420`), then `0x4778cc` |
| 0x4778cc | (sPlayer, pos, yaw, zone) | respawns **your** hunter there (vtable +0x370), sends it (`0x401128`), vtable +0x134 on all 4 hunters |
| 0x401128 | (net, pos, yaw, zone, mode) | sends "came back" (the packet `0x4011b8` reads) |
| 0x476748 | (sPlayer, slot) → player | the slot's hunter (used by the network handlers) |
| 0x700420 | (stage, index, 0, vec4 *pos, u32 *yaw) | the camp's start point |
| 0x4a4324 | (player, kind) | status setup: max HP `+0x13d8` (vtable +0x15c(0xc)), HP = max |
| 0x2bbf98 | (monster) | updates the attention scores; skips slots with a bit in `sEnemy +0x25ac` (`0x26946c`) or not ready |
| 0x2692ac | (sEnemy) | fills the ignore mask: offline = everyone but you |
| 0x8c02a4 | (area) | depart on a quest from town |
| 0x402894 | (proc, msg, slot) | applies `netPlData` (an arriving player's data) |
| 0x3ffff4 / 0x3ff308 | | apply `netPlPositionQuest` / `netPlAction` (remote player) |

### Hunter (`player`, `uPlayerQuestNN`, NN = weapon)

| Offset | Type | Meaning |
|---|---|---|
| +0x40 | vec3 | position (cm) |
| +0x50 | quat | rotation; forward on the ground = (2(qx·qz+qw·qy), 1−2(qx²+qy²)) |
| +0x1051 | u8 | slot |
| +0x1054 | u8 | zone (0 camp, 255 not in a quest); set through vtable +0xa4 |
| +0x114c | u8 | weapon drawn |
| +0x13c4 | ptr | the hunter's data (+0x4d4 weapon, +0x514 name) |
| +0x13c8 | u32 | mode (0 quest, 1 hub, …) |
| +0x13cc | u32 | bit 0: local |
| +0x13d0 | u32 | loading state (3 = ready) |
| +0x13d6 / +0x13d8 / +0x13da | s16 | HP / max HP / red HP |
| +0x13f4 … +0x1463 | | input block (0x70): +0x13f4 held, +0x13f8 new, +0x13fc word, +0x140c stick angle, +0x1410 tilt |
| +0x1470 | u32 | entry mode, ≠0: update on hold (1 the opening's wait, 3 back from a cart; the respawn sets 0). The move hook undoes only a 1 |
| +0x1480 | vec3 | a remote hunter's position target (20-frame glide) |
| +0x2506 / +0x250a | u16 | current action: act_st / act_no (0x0001 common, 0x0004 falling down a ledge, 0x0010 weapon, 0x0040 hit, 0x0100 fallen, 0x0200 waking up at the camp after a cart) |
| +0x27a0 | float | the fall's timer (600.0 frames) |
| +0x28b8 | s32 | >0: stick reversed (confusion) |

Vtable: +0x24 update, +0x88 ready, +0x9c / +0xa0 / +0xa4 (place in another zone, as `0x4762e4` does), +0x228 read
pad, +0x254, +0x2f8 do action (0x4ab670), +0x370 respawn (0x4ad6fc), +0x374 set `+0x1470` (0x4ae510).
Player vtables in `main`: image offset 0x1746ff0 and others (each weapon class; found by +0x370 = 0x4ad6fc).

### Pad (`sFestaPad +0x6b98`, gameplay view)

| Offset | Type | Meaning |
|---|---|---|
| +0x6b9c | u32 | held: A 0x100, B 0x80, X 0x20, Y 0x10, R 0x40, L 0x400, ZL 0x200, ZR 0x800; left stick past half way: up 0x10000, down 0x20000, left 0x40000, right 0x80000 (directions not confirmed) |
| +0x6ba0 / +0x6ba4 / +0x6ba8 | u32 | last frame / new / released |
| +0x6bac / +0x6bae | u16 | left / right stick angle (0x10000 = a turn; 0 right, 0x4000 up) |
| +0x6bb0 / +0x6bb4 | u32 | left / right tilt (0 below 30, … 127) |

### Monster (`uEm…`; status block `info = *(monster + 0x1428)`)

| Offset | Meaning |
|---|---|
| info +0x40 | position |
| info +0x1b6 | network id (u16; < 10000 large monster) |
| info +0x370 | HP (s32) |
| info +0x230..0x237 | current targets (kind / index pairs; logged raw) |
| info +0x254 | attention score per hunter (4 × s32, by slot) |
| info +0x264 | attention score per Palico (2 × s32) |
| monster +0x1054 | zone |
| monster +0x73e0 | current action |

### Useful classes and messages

`uPlayerQuest00..15` (hunter per weapon, code in `roPlayerQuest%02d.cro`), `uQuestNpcCatCart` (Felyne cart),
`nGuestHunter::cGuestHunter` / `sGuestHunter` (town Guest Hunters), `nFestaNetwork::netPlData` (0x5c0 bytes),
`netPlPositionQuest`, `netPlAction`, `netPlMemberState`, `netPlGuestLoadComplete`, `netPLDeathCntUp`.

## Reverse engineering (how to carry on)

- Executable: `data\exefs\main` of the game install (NSO). Copy it to the workspace; never distribute it.
- MHGURecomp's tools (`tools/`, need `capstone` and `lz4`), with `MHGU_MAIN=<path to main>`:
  - `retool.py dti <regex>`: MT Framework classes (name, size, DTI, parent);
  - `retool.py xref <image address>` / `xref str "<text>"`: what uses an address or a string;
  - `retool.py callers <runtime>`: direct calls and vtables holding that function;
  - `retool.py vtslot <runtime>`: which vtable / offset holds a function;
  - `disasm.py <game folder> <runtime> [n]`.
- `src/re/da.py <runtime> [n]`: a disassembler that resolves literals and globals (`load [GOT] (=value)`). A GOT entry's
  value is an image offset: add 0x200000.
- To find the handler of a `netXxx` message: `xref` the DTI of `netXxxProcedure`; the handler follows right after
  (`(proc, msg, kind, arg)`, compares the DTI and calls the "apply" function).
- MHGURecomp's `docs/STATUS.md` and `runtime/hle/*.c` document many fields already (monster attention, `emsync.c`,
  `hitsync.c`, `dummy.c`, `input_view.c`, `movelog.c`).

