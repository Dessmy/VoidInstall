#!/bin/sh
# ~/.xinitrc - Void Linux X11 startup for dwm
# iwd + dhcpcd for network
# Simple shell statusbar (no slstatus)

# ==============================================================================
# X11 Resources & Settings
# ==============================================================================

# Disable beep
xset -b &

# Set cursor
xsetroot -cursor_name left_ptr &

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
# Simple Shell Statusbar (no slstatus, no compiled binary)
# Updates root window name every second - dwm reads this for the bar
# ==============================================================================
while true; do
  # Battery
  bat_cap="$(cat /sys/class/power_supply/BAT0/capacity 2>/dev/null || echo '?')"
  bat_status="$(cat /sys/class/power_supply/BAT0/status 2>/dev/null || echo '?')"
  case "$bat_status" in
    Charging) bat_icon="+" ;;
    Discharging) bat_icon="-" ;;
    Full) bat_icon="=" ;;
    *) bat_icon="?" ;;
  esac

  # Volume (wpctl from wireplumber)
  vol_line="$(wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null || echo '0.00')"
  vol="$(echo "$vol_line" | awk '{print int($2*100)}')"
  echo "$vol_line" | grep -q MUTED && mute="true" || mute="false"
  [ "$mute" = "true" ] && vol_icon="M" || vol_icon="V"

  # Date/time
  dt="$(date '+%a %d %b %H:%M')"

  # Set root window name (dwm bar)
  xsetroot -name "BAT $bat_icon$bat_cap% | VOL $vol_icon$vol% | $dt"
  sleep 1
done &

# ==============================================================================
# Window Manager
# ==============================================================================

# Execute dwm
exec dwm

# Cleanup (on dwm exit)
kill $PW_PID $PWPULSE_PID $WP_PID 2>/dev/null