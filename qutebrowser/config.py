import os.path

config.load_autoconfig(True)

font = "FiraCode Nerd Font"

CONFIG_DIR = os.path.dirname(os.path.abspath(__file__))

# Catppuccin Mocha colors
base00 = "#1e1e2e"
base01 = "#181825"
base02 = "#313244"
base03 = "#45475a"
base04 = "#585b70"
base05 = "#cdd6f4"
base06 = "#f5e0dc"
base07 = "#bac2de"
base08 = "#f38ba8"
base09 = "#fab387"
base0A = "#f9e2af"
base0B = "#a6e3a1"
base0C = "#94e2d5"
base0D = "#89b4fa"
base0E = "#cba6f7"
base0F = "#f2cdcd"

c.colors.webpage.darkmode.enabled = True
c.colors.webpage.preferred_color_scheme = "dark"
c.colors.webpage.darkmode.policy.images = "never"
c.colors.webpage.bg = base00

c.url.default_page = str(config.configdir) + "/home.html"
c.url.start_pages = str(config.configdir) + "/home.html"

c.downloads.location.directory = "~/Downloads"
c.downloads.location.prompt = False
c.downloads.location.remember = True
c.downloads.remove_finished = 1000

c.auto_save.session = True

# nnn file picker (direct, no external script)
config.set("fileselect.handler", "external")
config.set(
    "fileselect.single_file.command",
    ["st", "-c", "filechoose_nnn", "sh", "-c", "nnn -p /tmp/nnn_picker -o /tmp/nnn_picker -f -P 'File' ~ && cat /tmp/nnn_picker | head -c -1 > {}"],
)
config.set(
    "fileselect.multiple_files.command",
    ["st", "-c", "filechoose_nnn", "sh", "-c", "nnn -p /tmp/nnn_picker -o /tmp/nnn_picker -f -P 'Files' ~ && cat /tmp/nnn_picker | head -c -1 > {}"],
)
config.set(
    "fileselect.folder.command",
    ["st", "-c", "filechoose_nnn", "sh", "-c", "nnn -p /tmp/nnn_picker -o /tmp/nnn_picker -d -P 'Folder' ~ && cat /tmp/nnn_picker | head -c -1 > {}"],
)

c.statusbar.widgets = ["url", "progress"]

c.scrolling.bar = "never"

c.tabs.favicons.scale = 1.0
c.tabs.position = "top"
c.tabs.width = "3%"
c.window.transparent = False

c.colors.completion.fg = base05
c.colors.completion.odd.bg = base01
c.colors.completion.even.bg = base01
c.colors.completion.category.fg = base05
c.colors.completion.category.bg = base01
c.colors.completion.category.border.top = base01
c.colors.completion.category.border.bottom = base01
c.colors.completion.item.selected.fg = base00
c.colors.completion.item.selected.bg = base0D
c.colors.completion.item.selected.border.top = base0D
c.colors.completion.item.selected.border.bottom = base0D
c.colors.completion.item.selected.match.fg = base00
c.colors.completion.match.fg = base0D
c.colors.completion.scrollbar.fg = base05
c.colors.completion.scrollbar.bg = base01

c.colors.contextmenu.disabled.bg = base01
c.colors.contextmenu.disabled.fg = base04
c.colors.contextmenu.menu.bg = base00
c.colors.contextmenu.menu.fg = base05
c.colors.contextmenu.selected.bg = base02
c.colors.contextmenu.selected.fg = base05

c.colors.downloads.bar.bg = base00
c.colors.downloads.start.fg = base00
c.colors.downloads.start.bg = base0D
c.colors.downloads.stop.fg = base00
c.colors.downloads.stop.bg = base0C
c.colors.downloads.error.fg = base08

c.colors.hints.fg = base00
c.colors.hints.bg = base0A
c.colors.hints.match.fg = base08
c.hints.padding = {"top": 1, "bottom": 1, "left": 3, "right": 3}
c.hints.border = "1px solid " + base01
c.hints.radius = 4

c.colors.keyhint.fg = base05
c.colors.keyhint.suffix.fg = base05
c.colors.keyhint.bg = base00

c.colors.messages.error.fg = base00
c.colors.messages.error.bg = base08
c.colors.messages.error.border = base08
c.colors.messages.warning.fg = base00
c.colors.messages.warning.bg = base0E
c.colors.messages.warning.border = base0E
c.colors.messages.info.fg = base05
c.colors.messages.info.bg = base00
c.colors.messages.info.border = base00

c.colors.prompts.fg = base05
c.colors.prompts.border = base00
c.colors.prompts.bg = base00
c.colors.prompts.selected.bg = base02
c.colors.prompts.selected.fg = base05

c.colors.statusbar.normal.fg = base0B
c.colors.statusbar.normal.bg = base00
c.colors.statusbar.insert.fg = base00
c.colors.statusbar.insert.bg = base0D
c.colors.statusbar.passthrough.fg = base00
c.colors.statusbar.passthrough.bg = base0C
c.colors.statusbar.private.fg = base00
c.colors.statusbar.private.bg = base01
c.colors.statusbar.command.fg = base05
c.colors.statusbar.command.bg = base00
c.colors.statusbar.command.private.fg = base05
c.colors.statusbar.command.private.bg = base00
c.colors.statusbar.caret.fg = base00
c.colors.statusbar.caret.bg = base0E
c.colors.statusbar.caret.selection.fg = base00
c.colors.statusbar.caret.selection.bg = base0D
c.colors.statusbar.progress.bg = base0D
c.colors.statusbar.url.fg = base05
c.colors.statusbar.url.error.fg = base08
c.colors.statusbar.url.hover.fg = base05
c.colors.statusbar.url.success.http.fg = base0C
c.colors.statusbar.url.success.https.fg = base0B
c.colors.statusbar.url.warn.fg = base0E

c.colors.tabs.bar.bg = base00
c.colors.tabs.indicator.start = base0D
c.colors.tabs.indicator.stop = base0C
c.colors.tabs.indicator.error = base08
c.colors.tabs.odd.fg = base05
c.colors.tabs.odd.bg = base01
c.colors.tabs.even.fg = base05
c.colors.tabs.even.bg = base00
c.colors.tabs.pinned.even.bg = base0C
c.colors.tabs.pinned.even.fg = base07
c.colors.tabs.pinned.odd.bg = base0B
c.colors.tabs.pinned.odd.fg = base07
c.colors.tabs.pinned.selected.even.bg = base02
c.colors.tabs.pinned.selected.even.fg = base05
c.colors.tabs.pinned.selected.odd.bg = base02
c.colors.tabs.pinned.selected.odd.fg = base05
c.colors.tabs.selected.odd.fg = base05
c.colors.tabs.selected.odd.bg = base02
c.colors.tabs.selected.even.fg = base05
c.colors.tabs.selected.even.bg = base02

c.fonts.default_family = font
c.fonts.default_size = "16pt"
# Every UI font: 16pt bold
for _opt in (
    "completion.entry", "completion.category", "contextmenu", "debug_console",
    "downloads", "hints", "keyhint", "messages.error", "messages.info",
    "messages.warning", "prompts", "statusbar", "tabs.selected", "tabs.unselected",
):
    config.set("fonts." + _opt, "bold 16pt " + font)

c.fonts.web.family.standard = font
c.fonts.web.family.serif = font
c.fonts.web.family.sans_serif = font
c.fonts.web.family.fixed = font
c.fonts.web.family.fantasy = font
c.fonts.web.family.cursive = font

c.qt.args = [
    "enable-gpu-rasterization",
    "enable-zero-copy",
    "enable-native-gpu-memory-buffers",
    "ignore-gpu-blocklist",
    "enable-accelerated-video-decode",
    "enable-accelerated-mjpeg-decode",
    "enable-accelerated-vpx-decode",
]
c.content.webgl = True