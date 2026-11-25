# MHGU Recomp: a try at hunter NPCs

An experiment to add an AI-controlled helper hunter to **Monster Hunter Generations Ultimate**, running on
[MHGURecomp](https://github.com/morerokk/MHGURecomp) (the native PC port).

Monster Hunter Rise: Sunbreak has Followers, NPC hunters that join your hunts. MHGU has nothing like that: only
Palicoes. This project tries to give MHGU a helper hunter that hunts with you as a real party member.

It is a **Forge plugin** (`.nro`), written in C, that hooks the game's own code. No game files are included or changed.

## What it does today (version 0.1.12)

- **Joins your quest:** a second hunter, joins your solo quests. For now it is a copy of your own
  hunter: same weapon, armour and looks.
- **Counts as a party member:** it shows in the party list on the HUD, and MHGURecomp's monster HP scaling counts it as
  a second hunter.
- **Hunts with you:** it follows you, runs to the large monster in its area, attacks, rolls when hit, sheathes its
  weapon to run, and appears next to you if it gets left behind in another area. Monsters target it too.
- **Carts like a player:** when it faints, the quest's cart counter goes up and it is carried back to camp, while you
  keep hunting. When you faint, it keeps fighting the monster.

Everything it does goes through the game's own code: its moves are real moves, made by pressing buttons on a "virtual
controller" the game reads for it.

## Known problems

- No mapping / waypoints, so the helper teleports to you when it needs to change area.
- The AI is very basic; still testing and finding more bugs.

## Goal

A helper hunter that feels like the Sunbreak Followers:

1. behaves exactly like a real party member (carts, camp, Palicoes, rewards);
2. fights well with any weapon;
3. later: more than one helper, choosing their gear, maybe working in multiplayer too.

## Using it

You need MHGURecomp (tested with 0.18.0) and your own copy of the game (Switch version 1.4.0).

1. Build `npc_hunter.nro` (see below), or take it from a release if there is one.
2. Put it in `mods\NPC-Hunter\romfs\nativeNX\plugins\` in your MHGURecomp folder.
3. Start a quest. The helper joins on its own.

| Key | Does |
|---|---|
| F4 | helper on / off (from the next quest) |
| F5 | make the helper now (testing) |
| F6 | write its state to `log.txt` |
| F7 | AI on / off |

## Building

Needs `clang`, `ld.lld` and `python3` (no devkitPro):

```sh
cd src
bash build.sh
```

## More

- [CLAUDE.md](CLAUDE.md): how it works and every game address used.
- [HISTORY.md](HISTORY.md): what each version changed.
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md): credits for code taken from forge-rs.

This is a fan project, not affiliated with Capcom or Nintendo. Built with the help of Claude (Anthropic).
