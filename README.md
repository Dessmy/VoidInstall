# Dotfiles - Void Linux + dwm (X11)

Migrated from Artix Linux + dwl (Wayland) to Void Linux + dwm (X11).

## System Overview

| Component | Artix (Old) | Void (New) |
|-----------|-------------|------------|
| WM | dwl (Wayland) | dwm (X11) |
| Terminal | foot | st |
| Launcher | wmenu | dmenu |
| Notifications | mako | dunst |
| Bar | waybar | shell script (xsetroot) |
| Compositor | (built-in) | picom |
| Power | powerctl | TLP |
| GPU | custom scripts | prime-run only |
| Shell | zsh | bash |
| Package Mgr | pacman | xbps |

## Key Package Changes

### Wayland to X11 Replacements
- `foot` -> st (suckless terminal)
- `wmenu` -> dmenu (suckless launcher)
- `mako` -> dunst (notification daemon)
- `waybar` -> shell statusbar (simple xsetroot loop, no slstatus)
- `swaylock` -> slock (screen locker)
- `grim/slurp` -> maim (screenshots)
- `wvkbd` -> xvkbd (on-screen keyboard, optional)

### Network
- iwd + dhcpcd (Void live ISO uses iwd by default)

### Removed (Artix-specific)
- powerctl -> TLP (power management)
- GPU switch scripts -> prime-run (simple, no daemon)
- optimus-manager -> not needed (prime-run handles it)
- bbswitch -> not needed (modern kernel handles it)

### Added (Void-specific)
- tlp + tlp-rdw - Advanced power management
- nvidia + bumblebee - NVIDIA Prime support
- xorg-server + xorg-xinit - X11 server
- iwd + dhcpcd - Network management

## Quick Install (from Void live ISO)

```bash
# Boot Void live ISO (glibc)
# Clone dotfiles
git clone https://github.com/youruser/dotfiles.git ~/Dotfiles
cd ~/Dotfiles

# Run setup (partitions disk, installs base, builds dwm/st/dmenu, configures everything)
sudo ./setup

# Reboot into new system
reboot

# Login as your user, start dwm
startx
```

## Key Bindings (dwm)

| Key | Action |
|-----|--------|
| Super+Enter | Open terminal (st) |
| Super+p | dmenu run launcher |
| Super+Shift+p | Power menu |
| Super+Shift+q | Quit dwm |
| Super+Shift+l | Lock screen (slock) |
| Super+j/k | Focus next/prev window |
| Super+h/l | Resize master/stack |
| Super+Shift+c | Kill window |
| Super+Space | Toggle layout |
| Super+Shift+Space | Toggle floating |
| Super+1-9 | Switch tag |
| Super+Shift+1-9 | Move window to tag |
| Super+Tab | Previous tag |
| Super+-/= | Decrease/increase gaps |
| Print | Screenshot (area) |
| Shift+Print | Screenshot (full) |
| XF86Audio* | Volume control |
| XF86MonBrightness* | Brightness control |

## Network (iwd + dhcpcd)

Void live ISO uses iwd by default. The setup script copies the current iwd state (connected WiFi) to the target system.

```bash
# Interactive (iwctl)
wifi              # or: sudo iwctl
wifi-station      # station commands
wifi-known        # known networks
wifi-scan         # scan + list networks

# Quick connect (from bashrc functions)
wifi-connect "SSID"
wifi-disconnect
wifi-list

# Manual
sudo iwctl station wlan0 scan
sudo iwctl station wlan0 get-networks
sudo iwctl station wlan0 connect "SSID"

# Check status
ip a
iwctl station wlan0 show
```

iwd runs as system service (enabled via setup). dhcpcd handles DHCP.

## NVIDIA GPU (Prime Render Offload)

Use prime-run for NVIDIA GPU:

```bash
# Run any app on NVIDIA GPU
prime-run firefox
prime-run steam
prime-run glxgears

# Test it works
prime-run glxinfo | grep "OpenGL renderer"
prime-run vulkaninfo | grep deviceName

# Steam launch options
prime-run %command%

# For Lutris/Wine: enable "NVIDIA Prime Render Offload" in runner options

# Monitor GPU usage
nvidia-smi
watch -n 1 nvidia-smi
```

Environment variables (already in .bashrc):
```bash
export __NV_PRIME_RENDER_OFFLOAD=1
export __GLX_VENDOR_LIBRARY_NAME=nvidia
export __VK_LAYER_NV_optimus=NVIDIA_only
```

## Power Management (TLP)

TLP uses defaults. No custom profiles needed.

```bash
# Check status
sudo tlp-stat -s

# Battery info
sudo tlp-stat -b

# TLP handles automatically:
# - CPU scaling/governors
# - GPU power management
# - WiFi/Bluetooth power save
# - USB autosuspend
# - Disk power management
# - NVIDIA GPU runtime PM
```

## Customization

### Rebuild suckless tools after config changes:
```bash
dotsync
```

### Config locations:
| Tool | Config File |
|------|-------------|
| dwm | ~/.config/dwm/config.h |
| st | ~/.config/st/config.h |
| dmenu | ~/.config/dmenu/config.def.h |
| statusbar | ~/.xinitrc (shell loop with xsetroot) |
| dunst | ~/.config/dunst/dunstrc |
| picom | ~/.config/picom/picom.conf |
| TLP | /etc/tlp.conf (copy from ~/.config/tlp/tlp.conf) |
| iwd | /etc/iwd/main.conf (copy from ~/.config/iwd/main.conf) |

### Statusbar (shell, no slstatus)
The statusbar is a simple shell loop in ~/.xinitrc that runs before exec dwm:
```bash
while true; do
  bat_cap="$(cat /sys/class/power_supply/BAT0/capacity 2>/dev/null || echo '?')"
  bat_status="$(cat /sys/class/power_supply/BAT0/status 2>/dev/null || echo '?')"
  # battery icon logic
  vol="$(pamixer --get-volume 2>/dev/null || echo '?')"
  # volume icon logic
  dt="$(date '+%a %d %b %H:%M')"
  xsetroot -name "BAT $bat_icon$bat_cap% | VOL $vol_icon$vol% | $dt"
  sleep 1
done &
exec dwm
```
Edit the loop in ~/.xinitrc to customize what shows in the bar.

### Theme: Catppuccin Mocha
All tools are hardcoded to Catppuccin Mocha (no theme switcher):
- Background: #1e1e2e (base), Surface: #313244
- Foreground: #cdd6f4 (text)
- Accent: #89b4fa (blue)
- Red: #f38ba8, Green: #a6e3a1, Yellow: #f9e2af, Mauve: #cba6f7

## Directory Structure

```
~/Dotfiles/
├── .config/
│   ├── dwm/           # dwm config.h
│   ├── st/            # st config.h
│   ├── dmenu/         # dmenu config.def.h
│   ├── dunst/         # dunst config
│   ├── picom/         # picom config
│   ├── tlp/           # TLP config
│   ├── iwd/           # iwd config
│   ├── nvim/          # Neovim config
│   └── git/           # Git config
├── void-packages/
│   └── packages.txt   # Package list for xbps
├── scripts/           # User scripts (bt, lock, randr, walls, wifi, etc)
├── setup              # Single installer from live ISO
├── .bashrc            # Bash config with iwd aliases
├── .xinitrc           # X11 startup + shell statusbar loop
├── .Xresources        # X resources
└── README.md          # This file
```

## Development Workflow

```bash
# Edit config
nvim ~/.config/dwm/config.h

# Rebuild and install
dotsync

# Restart dwm (Super+Shift+q) or reload
```

## Troubleshooting

### dwm won't start
```bash
# Check for errors
startx 2>&1 | tee ~/dwm.log

# Common issues:
# - Font not found: install nerd-fonts
# - X11 not configured: check .xinitrc
# - Missing libs: ldd ~/.config/dwm/dwm
```

### NVIDIA not working
```bash
# Check driver loaded
lsmod | grep nvidia

# Check prime
prime-run glxinfo | grep "OpenGL renderer"

# Reinstall nvidia
sudo xbps-install -f nvidia
sudo dracut --force
reboot
```

### TLP not starting
```bash
# Check service
sudo sv status tlp

# Start manually
sudo tlp start

# Check config
sudo tlp-stat -c
```

### iwd / Network issues
```bash
# Check iwd service
sudo sv status iwd

# Check dhcpcd
sudo sv status dhcpcd

# Restart both
sudo sv restart iwd
sudo sv restart dhcpcd

# Debug
iwctl station wlan0 show
ip a
journalctl -u iwd -f
```

### Audio issues
```bash
# Restart pipewire
systemctl --user restart pipewire pipewire-pulse wireplumber

# Check devices
pactl list short sinks
```

## Resources

- dwm suckless.org
- st suckless.org
- Void Linux Handbook
- TLP Configuration
- NVIDIA Prime Render Offload
- Catppuccin Mocha

## License

MIT License - Feel free to use and modify.