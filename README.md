# npvz

Plants vs. Zombies in your terminal — ncurses-based, emoji-rendered, chiptune-powered.

![NPVZ terminal gameplay](./docs/screenshots/NPVZonGhostty.png)

## Features

- **5×9 grid** with five-wave level and endless modes
- **Card selection:** choose 6–9 plants before each game
- **Plants:** Sunflower, Peashooter, Wall-nut, Cherry Bomb, Snow Pea, Jalapeno, Repeater, Chomper, Potato Mine, and Squash
- **Zombies:** Normal, Conehead, Buckethead, Dancer, Backup, Pole Vaulter, Newspaper, Football, and Screen Door
- **Lawn mowers** 🚜 as last-resort row defense
- **Shovel** ⛏ to remove placed plants
- **Projectiles** rendered as `·` (middle dot)
- **VFX:** hit flash, death flash, and explosions
- **Chiptune SFX** synthesized at launch and played via `afplay`

## Controls

| Screen | Key | Action |
| --- | --- | --- |
| Menu | Up/Down or J/K | Choose level, endless mode, or quit |
| Menu | Enter | Confirm selection |
| Card selection | Up/Down or J/K | Choose a plant |
| Card selection | Left/Right or H/L | Set 6–9 card slots |
| Card selection | Tab | Focus the plant list, Start, or Menu |
| Card selection | Enter / Space | Add or remove a plant |
| Card selection | Enter | Activate the focused Start or Menu button |
| Card selection | G / Q or Esc | Start the game / return to menu |
| Game | Arrow keys / HJKL | Move cursor, wrapping at board edges |
| Game | Tab | Move to the next row in the current column, wrapping to the top |
| Game | 1–9 | Select a deck slot |
| Game | Q/W/E/R | Select deck slots 6/7/8/9 |
| Game | 0 or T | Toggle shovel |
| Game | Enter / Space | Place plant or dig |
| Game | P or Esc | Open the pause menu |
| Pause | P or Esc | Resume the game |
| Pause / end menu | Up/Down or J/K | Choose an action |
| Pause / end menu | Enter | Confirm selection |
| Help | P or Esc | Return to the pause menu |

## Build

```sh
make
./npvz
```

### Requirements

- macOS (uses system `ncurses` and `afplay` for audio)
- C11 compiler (`cc` / `clang`)

No external dependencies.

### Install

```sh
make install          # installs to /usr/local/bin
make install PREFIX=~/.local  # custom prefix
```

## License

Apache 2.0 — see [LICENSE](LICENSE).
