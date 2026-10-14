#!/bin/bash
set -euo pipefail
[[ $# -eq 1 ]] || { echo 'Usage: bash tools/install_local.sh /path/to/plugin.vst3' >&2; exit 2; }
source_bundle="$1"
[[ -d "$source_bundle/Contents/MacOS" ]] || { echo 'Not a VST3 bundle' >&2; exit 1; }
codesign --verify --deep --strict "$source_bundle"
plugin_root="$HOME/Library/Audio/Plug-Ins/VST3"
dest="$plugin_root/Virus TI Rework Development.vst3"
mkdir -p "$plugin_root"
stage="$(mktemp -d "$plugin_root/.virus-ti-install.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
ditto "$source_bundle" "$stage/Virus TI Rework Development.vst3"
codesign --verify --deep --strict "$stage/Virus TI Rework Development.vst3"
if [[ -e "$dest" || -L "$dest" ]]; then
 backup_root="$HOME/Library/Application Support/VirusTIRework/PluginBackups"
 mkdir -p "$backup_root"
 backup="$(mktemp -d "$backup_root/previous.XXXXXX")"
 mv "$dest" "$backup/"
 echo "Previous plug-in preserved at: $backup"
fi
mv "$stage/Virus TI Rework Development.vst3" "$dest"
echo "Installed for this user: $dest"
echo 'Fully restart your DAW before testing; an already loaded plug-in stays in memory.'
