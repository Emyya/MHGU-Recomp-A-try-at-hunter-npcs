# History

What each version changed and what the test showed. 0.1.12 is the base version: from here on, builds get a random id
(`build xxxxxxxx`) and the version number changes only when a new version is decided.

| Version | Change | Result |
|---|---|---|
| test 1 | F5: copies your hunter data and makes a hunter in a free slot (`0x474cf0`) in the middle of a quest | appears, stands still, not in the HUD |
| test 2 | adds it to the quest members on departure (`0x8c02a4`) | offline there is no member list: nothing |
| test 3 | makes it while the quest loads (hook `0x4758a4`) | joins by itself, shows in the HUD, 2 hunters counted |
| 0.1.4 | virtual controller; helper marked local all the time | invisible, no input |
| 0.1.5 | remote hunter with forced input (`0x47c97c`) | input arrives but does nothing |
| 0.1.6 | local only during its own update | invisible: the game puts it on hold (`+0x1470 = 1`) |
| 0.1.7 | undoes that hold | moves, follows, attacks, rolls; stays fallen after fainting |
| 0.1.8 | monster metrics in the log; gets up after 5 s | the "gets up" was dropped: not the game's behaviour |
| 0.1.9 | logs carts and actions | its faint counts for the quest; monsters ignore it |
| 0.1.10 | clears the monsters' ignore mask after the frame | too late: the game sets it again every frame |
| 0.1.11 | hooks the ignore check (`0x26946c`) | monsters target it; its faint carted **you** to camp |
| 0.1.12 | its faint counted as its own; its cart done the way a remote hunter's is (no reload of your area, no camera change); 1 s from faint to camp; keeps fighting when you cart; F6 / F4 messages; random build ids | base version |
