#!/bin/bash
# Builds sce_sys/ for the Vita VPK from the vita block of config/build_settings.txt:
# the icon, the LiveArea images and template.xml. What it produced is written to
# the manifest (default build/livearea_files.txt) as <host path>=<path inside the
# VPK> lines, which the VPK rule turns into vita-pack-vpk -a arguments.
set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

CONFIG="config/build_settings.txt"
MANIFEST="${1:-build/livearea_files.txt}"
CONTENTS="sce_sys/livearea/contents"

# The manifest is only moved into place once every image converted, so a run
# that dies halfway leaves the older one behind and make runs the script again
# instead of packing whatever happened to be on disk.
PENDING="$MANIFEST.pending"
trap 'rm -f "$PENDING"' EXIT

config_value() {
    [ -f "$CONFIG" ] || return 0
    sed -n "s/^vita:$1=//p" "$CONFIG" | tail -n 1
}

require_tool() {
    if ! command -v "$1" > /dev/null 2>&1; then
        echo "build_livearea.sh: $1 not found. Install it with: sudo apt install $1" >&2
        exit 1
    fi
}

require_source() {
    if [ ! -f "$1" ]; then
        echo "build_livearea.sh: source image $1 does not exist (set in $CONFIG)" >&2
        exit 1
    fi
}

pack() {
    echo "$1=$1" >> "$PENDING"
}

# Scales to the size the Vita expects, then lets pngquant build the palette.
convert_indexed() {
    local source="$1" size="$2" output="$3"
    local scaled="$output.scaled.png"

    ffmpeg -v error -y -i "$source" -vf "scale=$size:flags=neighbor" -pix_fmt rgba "$scaled"
    pngquant --force -o "$output" "$scaled"
    rm -f "$scaled"
}

# pic0.png needs a full 256 colour palette, so ffmpeg generates and applies one
# instead of pngquant, which only quantises down to at most 256.
convert_pic0() {
    local source="$1" output="$2"
    local scaled="$output.scaled.png" palette="$output.palette.png"

    ffmpeg -v error -y -i "$source" -vf "scale=960:544:flags=neighbor" -pix_fmt rgba "$scaled"
    ffmpeg -v error -y -i "$scaled" -vf palettegen "$palette"
    ffmpeg -v error -y -i "$scaled" -i "$palette" -filter_complex paletteuse "$output"
    rm -f "$scaled" "$palette"
}

ICON0="$(config_value icon0)"
PIC0="$(config_value pic0)"
BG0="$(config_value bg0)"
STARTUP="$(config_value startup)"
STYLE="$(config_value style)"
[ -n "$STYLE" ] || STYLE="a1"

# Dropped first so a slot cleared in the editor stops shipping its old image.
rm -f sce_sys/icon0.png sce_sys/pic0.png \
      "$CONTENTS/bg0.png" "$CONTENTS/startup.png" "$CONTENTS/template.xml"

mkdir -p "$(dirname "$MANIFEST")"
: > "$PENDING"

if [ -z "$ICON0$PIC0$BG0$STARTUP" ]; then
    echo "No LiveArea images configured in $CONFIG, the VPK ships without sce_sys assets."
    mv "$PENDING" "$MANIFEST"
    exit 0
fi

require_tool ffmpeg
require_tool pngquant

mkdir -p sce_sys "$CONTENTS"

if [ -n "$ICON0" ]; then
    require_source "$ICON0"
    convert_indexed "$ICON0" "128:128" sce_sys/icon0.png
    pack sce_sys/icon0.png
    echo "icon0.png    <- $ICON0"
fi

if [ -n "$PIC0" ]; then
    require_source "$PIC0"
    convert_pic0 "$PIC0" sce_sys/pic0.png
    pack sce_sys/pic0.png
    echo "pic0.png     <- $PIC0"
fi

if [ -n "$BG0" ]; then
    require_source "$BG0"
    convert_indexed "$BG0" "840:500" "$CONTENTS/bg0.png"
    pack "$CONTENTS/bg0.png"
    echo "bg0.png      <- $BG0"
fi

if [ -n "$STARTUP" ]; then
    require_source "$STARTUP"
    convert_indexed "$STARTUP" "280:158" "$CONTENTS/startup.png"
    pack "$CONTENTS/startup.png"
    echo "startup.png  <- $STARTUP"
fi

if [ -n "$BG0$STARTUP" ]; then
    {
        echo '<?xml version="1.0" encoding="utf-8"?>'
        echo "<livearea style=\"$STYLE\" format-ver=\"01.00\" content-rev=\"1\">"
        if [ -n "$BG0" ]; then
            echo '    <livearea-background>'
            echo '        <image>bg0.png</image>'
            echo '    </livearea-background>'
        fi
        if [ -n "$STARTUP" ]; then
            echo '    <gate>'
            echo '        <startup-image>startup.png</startup-image>'
            echo '    </gate>'
        fi
        echo '</livearea>'
    } > "$CONTENTS/template.xml"
    pack "$CONTENTS/template.xml"
    echo "template.xml <- style $STYLE"
fi

mv "$PENDING" "$MANIFEST"
