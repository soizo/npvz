# npvz

Plants vs. Zombies in your terminal — ncurses-based, emoji-rendered, chiptune-powered.

```
 SUN: 150   Wave: 1  Remaining: 8
 1:🌻  50  2:🫛 100  3:🪨  50  4:🍒 150  5:🧊 175  0:🪏

🚜  □   □   □   □   □   □   □   🧟  □
    □   🌻  □   🫛  □   □   □   □   □
    □   □   □   □   □   □  ·  · 🪖  □
    □   🪨  □   □   □   □   □   □   □
    □   □   □   □   □   □   □   🧟  □
```

## Features

- **5×9 grid**, 5 waves of zombies
- **Plants:** 🌻 🫛 🪨 🍒 🧊 with sun economy
- **Zombies:** 🧟 🪖 🪣 — Conehead and Buckethead have separate armor HP before you can damage the body
- **Lawn mowers** 🚜 as last-resort row defense
- **Shovel** 🪏 to remove placed plants
- **Projectiles** rendered as `·` (middle dot)
- **VFX:** hit flash, death flash, 💥 cherry bomb explosion
- **Chiptune SFX** synthesized at launch — sine, square, sawtooth, noise waveforms played via `afplay`

## Controls

| Key | Action |
|-----|--------|
| Arrow keys / WASD / HJKL | Move cursor |
| 1–5 | Select plant |
| 0 | Toggle shovel |
| Enter / Space | Place plant or dig |
| P | Pause / unpause |
| Q | Quit |
| R | Restart (end screen) |

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
