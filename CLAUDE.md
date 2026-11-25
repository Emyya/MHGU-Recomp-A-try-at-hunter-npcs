# NPC Hunter — an AI helper hunter for MHGU (MHGURecomp)

A Forge plugin that puts a second, AI-controlled hunter (the "Ajudante", Portuguese for "helper") into Monster Hunter
Generations Ultimate quests running on **MHGURecomp 0.18.0** (native PC port, github.com/morerokk/MHGURecomp). The game
treats it as a real player: it shows in the HUD's party list, counts towards monster HP (the port's `[Monster HP]`
option), moves, attacks and uses up carts.

- **Current version:** 0.1.10 (see "Current state" and "History").
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

## Current state (0.1.10)

Works:
- The helper joins the quest while it loads, at the same step where the game makes the other players' hunters. It takes
  the free slot (usually 1), with a copy of your hunter data (weapon, armour, looks) and the name "Ajudante".
- It shows in the HUD party list; the port counts 2 hunters (`[hpscale] ... 2 hunter(s)`).
- It moves, follows you, runs to the zone's large monster, attacks (X, X, A), rolls when hit and sheathes to run. In
  another zone than yours for more than 3 s, it appears next to you.
- Its cart counts for the quest: the game's own death routine takes it (credited to your slot).

To confirm (0.1.10 change, not tested yet):
- Monsters now paying attention to it (its scores used to be always 0; see "Monster attention").

To do:
1. **Full cart:** after falling, the helper is not carried to camp. It stays at 0 HP for a few seconds, then comes back
   with very little HP, where it fell. Still to find: the part of the game that takes **your** hunter to camp (fade,
   Felyne cart `uQuestNpcCatCart`, waking up in the tent) and doing it for the helper. 0.1.10 logs everything
   (`[npc-cart]`) to copy that sequence: it needs a test where you cart yourself.
2. Palicos: 2 come along (yours). With 2 hunters the total matches the normal game, but both are yours.
3. AI: large monsters only, no items, no dodging before a hit, no changing zones on its own (only the teleport).

Design rule: no made-up shortcuts (e.g. "gets up after 5 s"): behaviour must be the game's own.

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
- When changing the version, update `NPC_VERSION` in `npc.c` (shown in the log and in the join message).
- **Installing from Claude:** the copy into the game folder can pick up an older version of the file. Always give the output file
  a new name (e.g. `npc_hunter_0.1.11.nro`) and check the SHA-1 of the installed file.

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
| 0x1a8ca48 | `sGameControl*` | +0x30: your slot (`0x5f8398`) |
| 0x1a51fe0 | `sEnemy*` | +0x1c: the zone's 10 enemies; +0x25ac: mask of hunters to ignore |
| 0x1a843f0 | `sFestaNetwork*` | +0x63e: in a session (`0x427ba8`); +0xa90: each member's state (3 = in the quest) |
| 0x1a8ae38 | `sQuestSelector*` | +0xd68: ordered quest; +0xd750 `mJoinNum`; +0xd754: members (12 bytes each) — stays 0 offline |
| 0x1a8ad90 | `sQuest*` | +0x4c state; +0xc0 cart allowance (quest fails at 0); +0xc4 cost of a cart; +0x166a the quest's carts; +0x166b + slot carts per player |
| 0x1a8c670 | `sFestaPad*` | the pad's gameplay view at +0x6b98 (see "Pad") |
| 0x2326504 | camera manager* | +0x54: the camera in use (eye +0x40, target +0x60) |

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
| 0x5a9754 → 0x5a9934 | (sQuest, …) | counts a cart (offline credits your slot) |
| 0x5a9bb8 | (sQuest, kind, value) | the same, from the network (`netPLDeathCntUp`) |
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
| +0x13d6 / +0x13da | s16 | HP / red HP |
| +0x13f4 … +0x1463 | | input block (0x70): +0x13f4 held, +0x13f8 new, +0x13fc word, +0x140c stick angle, +0x1410 tilt |
| +0x1470 | u32 | ≠0: update on hold (the opening's wait) |
| +0x1480 | vec3 | a remote hunter's position target (20-frame glide) |
| +0x2506 / +0x250a | u16 | current action: act_st / act_no (0x0001 common, 0x0010 weapon, 0x0100 fallen) |
| +0x28b8 | s32 | >0: stick reversed (confusion) |

Vtable: +0x24 update, +0x88 ready, +0x9c / +0xa0 / +0xa4 (place in another zone, as `0x4762e4` does), +0x228 read
pad, +0x254, +0x2f8 do action, +0x374 set `+0x1470`.

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

