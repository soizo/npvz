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
| Menu | Up/Down or W/S/J/K | Choose level or endless mode |
| Menu | Enter / Space | Open card selection |
| Card selection | Up/Down or W/S/J/K | Choose a plant |
| Card selection | Left/Right or A/D/H/L | Set 6–9 card slots |
| Card selection | Enter / Space | Add or remove a plant |
| Card selection | G | Start with the selected deck |
| Game | Arrow keys / WASD / HJKL | Move cursor |
| Game | 1–9 | Select a deck slot |
| Game | 0 | Toggle shovel |
| Game | Enter / Space | Place plant or dig |
| Game | P | Pause / unpause |
| Game | ? | Open / close command help |
| End screen | R | Return to card selection |
| End screen | M | Return to menu |
| Any screen except card selection | Q | Quit |
| Card selection | Q | Return to menu |

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
