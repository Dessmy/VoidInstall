#!/bin/sh

[ -z "$DBUS_SESSION_BUS_ADDRESS" ] && exec dbus-run-session -- sh "$HOME/.xinitrc"

"$HOME/.local/bin/screenctl" nobell noblank dpms 0 0 600

"$HOME/.local/bin/autolock" 300 slock &

[ -f ~/.local/share/wallpaper.jpg ] && "$HOME/.local/bin/setbg" ~/.local/share/wallpaper.jpg &

pipewire >/dev/null 2>&1 &
PW_PID=$!
sleep 0.5
pipewire-pulse >/dev/null 2>&1 &
PWPULSE_PID=$!
wireplumber >/dev/null 2>&1 &
WP_PID=$!

"$HOME/.config/status/status" &

exec dwm

kill $PW_PID $PWPULSE_PID $WP_PID 2>/dev/null
