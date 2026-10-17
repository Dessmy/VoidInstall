## Quick Install (from Void live ISO)

```bash
# Boot Void live ISO (glibc)
# Clone dotfiles
git clone https://github.com/Dessmy/VoidInstall && cd VoidInstall && sudo bash setup


## Key Bindings (dwm)

| Key | Action |
|-----|--------|
| Super+Shift+Enter | Open st |
| Super+q | Open qutebrowser |
| Super+Shift+p | Power menu |
| Super+Shift+q | Quit dwm |
| Super+Shift+l | slock |
| Super+j/k | Focus next/prev window |
| Super+h/l | Resize master/stack |
| Super+Shift+c | Kill window |
| Super+Space | Toggle layout |
| Super+Shift+Space | Toggle floating |
| Super+1-9 | Switch tag |
| Super+Shift+1-9 | Move window to tag |
| Super+Tab | Previous tag |
| Super+-/= | Decrease/increase brightness |
| Super+Shift+s | Screenshot (area) |
| Super+s | Screenshot (full) |

## NVIDIA GPU (Prime Render Offload)

Use prime-run for NVIDIA GPU:

```bash
# Run any app on NVIDIA GPU
prime-run steam

# Test it works
prime-run glxinfo | grep "OpenGL renderer"
prime-run vulkaninfo | grep deviceName


# Check devices
pactl list short sinks
```