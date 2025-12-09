# [Megabonk](https://store.steampowered.com/app/3405340/Megabonk/)-like game

 The game built with the [McLaren](https://github.com/21pack/McLaren) engine.

# Quick start

### Dependencies
```
sudo apt update
sudo apt install \
    libxrandr-dev \
    libxcursor-dev \
    libxi-dev \
    libudev-dev \
    libfreetype-dev \
    libflac-dev \
    libvorbis-dev \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libfreetype-dev
```
### build
```
./scripts/build.sh --release
```
### run game
```
./scripts/run.sh
```
### format or check formatting
```
./scripts/format
./scripts/format --check
```

### Controls

| Key            | Action                       |
|----------------|------------------------------|
| W / A / S / D  | Move up / left / down / right|
| Esc            | Pause / unpause              |
| `+` / `-`      | Increase / decrease game speed |
| 1 / 2 / 3      | Choose upgrade in level-up menu |


