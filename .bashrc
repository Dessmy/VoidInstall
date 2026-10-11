# ~/.bashrc - Void Linux + dwm (X11)
# Catppuccin theme, nnn file manager, qutebrowser

# ==============================================================================
# Environment Variables
# ==============================================================================

export EDITOR="nvim"
export VISUAL="nvim"
export PAGER="bat"
export BROWSER="qutebrowser"
export TERMINAL="st"
export FILE_MANAGER="nnn"

# XDG
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.local/share"
export XDG_CACHE_HOME="$HOME/.cache"
export XDG_STATE_HOME="$HOME/.local/state"

# Path
export PATH="$HOME/.local/bin:$HOME/.cargo/bin:$HOME/go/bin:$PATH"

# GTK / Qt (Catppuccin Mocha default)
export GTK_THEME=Catppuccin-Mocha-Standard-Blue-Dark
export QT_QPA_PLATFORMTHEME=qt5ct
export QT_STYLE_OVERRIDE=kvantum

# Bat theme
export BAT_THEME="catppuccin-mocha"

# Less colors
export LESS="-R"
export LESS_TERMCAP_mb=$'\e[1;31m'
export LESS_TERMCAP_md=$'\e[1;36m'
export LESS_TERMCAP_me=$'\e[0m'
export LESS_TERMCAP_so=$'\e[1;44;33m'
export LESS_TERMCAP_se=$'\e[0m'
export LESS_TERMCAP_us=$'\e[1;32m'
export LESS_TERMCAP_ue=$'\e[0m'

# ==============================================================================
# History
# ==============================================================================

HISTFILE="$XDG_STATE_HOME/bash/history"
HISTSIZE=50000
HISTFILESIZE=50000
HISTCONTROL=ignoreboth:erasedups
HISTIGNORE="ls:ll:la:cd:pwd:exit:clear:history"
HISTTIMEFORMAT="%F %T "

shopt -s histappend
shopt -s cmdhist
shopt -s lithist
shopt -s checkwinsize
shopt -s globstar
shopt -s nocaseglob
shopt -s dirspell
shopt -s autocd 2>/dev/null

PROMPT_COMMAND="history -a; history -n; ${PROMPT_COMMAND}"

# ==============================================================================
# Prompt (Catppuccin Mocha style)
# ==============================================================================

# Git branch in prompt
parse_git_branch() {
  git branch 2>/dev/null | sed -n '/^\*/s/^\* //p'
}

# Catppuccin Mocha colors
RED='\[\033[38;2;243;139;168m\]'
GREEN='\[\033[38;2;166;227;161m\]'
YELLOW='\[\033[38;2;249;226;175m\]'
BLUE='\[\033[38;2;137;180;250m\]'
MAUVE='\[\033[38;2;203;166;247m\]'
RESET='\[\033[0m\]'

PS1="${GREEN}\u${RESET}@${BLUE}\h${RESET} ${YELLOW}\w${RESET} \$(parse_git_branch && echo \"(${MAUVE}\$(parse_git_branch)${RESET})\")\n${BLUE}>${RESET} "

# ==============================================================================
# Aliases
# ==============================================================================

# System
alias sudo='sudo '
alias please='sudo $(fc -ln -1)'
alias reboot='sudo reboot'
alias poweroff='sudo poweroff'
alias suspend='sudo zzz'
alias hibernate='sudo ZZZ'

# Package management (Void xbps)
alias xi='sudo xbps-install -S'
alias xr='sudo xbps-remove -R'
alias xq='xbps-query -Rs'
alias xu='sudo xbps-install -Su'
alias xc='sudo xbps-remove -Oo'
alias xl='xbps-query -l'

# Modern replacements
alias ls='exa --icons --group-directories-first'
alias ll='exa -l --icons --group-directories-first --git'
alias la='exa -la --icons --group-directories-first --git'
alias lt='exa --tree --icons --git-ignore'
alias cat='bat --style=plain'
alias grep='rg'
alias find='fd'
alias du='dust'
alias df='duf'
alias top='htop'
alias ps='procs'
alias diff='delta'

# Git
alias g='git'
alias ga='git add'
alias gc='git commit -v'
alias gp='git push'
alias gl='git pull'
alias gs='git status'
alias gd='git diff'
alias gco='git checkout'
alias gb='git branch'
alias glog='git log --oneline --graph --decorate --all'

# TLP (uses defaults)
alias tlp-stat='sudo tlp-stat -s'
alias tlp-bat='sudo tlp-stat -b'

# iwd
alias wifi='sudo iwctl'
alias wifi-station='sudo iwctl station'
alias wifi-known='sudo iwctl known-networks'
alias wifi-scan='sudo iwctl station wlan0 scan && sleep 2 && sudo iwctl station wlan0 get-networks'

# dmenu wifi/bluetooth
alias dwifi='dmenu-wifi'
alias dbt='dmenu-bluetooth'

# Config editing
alias bashcfg='$EDITOR ~/.bashrc'
alias dwmcfg='$EDITOR ~/.config/dwm/config.h'
alias stcfg='$EDITOR ~/.config/st/config.h'
alias dunstcfg='$EDITOR ~/.config/dunst/dunstrc'
alias tlpconf='sudo $EDITOR /etc/tlp.conf'
alias iwdconf='sudo $EDITOR /etc/iwd/main.conf'

# Reload
alias reload='exec bash'
alias src='source ~/.bashrc'

# ==============================================================================
# Functions
# ==============================================================================

# Rebuild suckless tools
dotsync() {
  for tool in dwm st dmenu; do
    [ -d "$HOME/.config/$tool" ] && [ -f "$HOME/.config/$tool/Makefile" ] &&
      (cd "$HOME/.config/$tool" && sudo make clean install 2>/dev/null) || true
  done
  echo "All packages rebuilt!"
}

# ==============================================================================
# Completion
# ==============================================================================

if [ -f /usr/share/bash-completion/bash_completion ]; then
  . /usr/share/bash-completion/bash_completion
fi

# ==============================================================================
# Local overrides
# ==============================================================================

[ -f ~/.bashrc.local ] && source ~/.bashrc.local
