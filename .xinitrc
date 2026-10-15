#!/bin/sh
# ~/.xinitrc - Void Linux X11 startup for dwm
# iwd + dhcpcd for network
# Status (no slstatus)

# ==============================================================================
# X11 Resources & Settings
# ==============================================================================

# Disable beep
xset -b &

# DPMS / Screen blanking
xset s off
xset -dpms
xset s noblank

# ==============================================================================
# Autostart Applications
# ==============================================================================

# Start notification daemon
dunst -config ~/.config/dunst/dunstrc &

# Set wallpaper (using xwallpaper)
[ -f ~/.local/share/wallpaper.jpg ] && xwallpaper --zoom ~/.local/share/wallpaper.jpg &

# Start PipeWire audio
pipewire >/dev/null 2>&1 &
PW_PID=$!
sleep 0.5
pipewire-pulse >/dev/null 2>&1 &
PWPULSE_PID=$!
wireplumber >/dev/null 2>&1 &
WP_PID=$!

# Start polkit agent (for GUI authentication)
/usr/libexec/polkit-gnome-authentication-agent-1 &

# Start power management (TLP defaults)
tlp start &

# ==============================================================================
# Status: tiny C program (source and binary in ~/.config/status/) writes
# the dwm bar text directly with Xlib once a minute. No xsetroot, no forks.
# ==============================================================================
"$HOME/.config/status/status" &

# ==============================================================================
# Window Manager
# ==============================================================================

# Execute dwm
exec dwm

# Cleanup (on dwm exit)
kill $PW_PID $PWPULSE_PID $WP_PID 2>/dev/null
