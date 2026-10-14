#!/usr/bin/env bash
set -euo pipefail
mix_root=/workspace/125A-MixEngine-Final
mix_deps=/workspace/dependencies
mix_tools=/workspace/.setup-tools
mkdir -p "$mix_deps"
python -m pip install --disable-pip-version-check --upgrade --target "$mix_tools" cmake==4.4.4 ninja==1.13.2 pillow==12.3.0
export PYTHONPATH="$mix_tools${PYTHONPATH:+:$PYTHONPATH}"
ensure_pin() {
 local remote="$1" path="$2" pin="$3"
 if [ ! -e "$path" ]; then git clone "$remote" "$path"; git -C "$path" checkout "$pin"; fi
 test "$(git -C "$path" rev-parse HEAD)" = "$pin"
 git -C "$path" diff --quiet
}
ensure_pin https://github.com/steinbergmedia/vst3sdk.git "$mix_deps/vst3sdk" 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
 git -C "$mix_deps/vst3sdk" submodule update --init --recursive
ensure_pin https://github.com/unevens/hiir.git "$mix_deps/hiir" 4589fedb4d08b899514cb605ccd7418bf262ab18
ensure_pin https://github.com/challanger2000/125A-Knob-Designer.git "$mix_deps/knob-assets" c09cff5d958969eb025fe09255173e65103deae9
 test -f "$mix_root/scripts/audit_headless.py"
python "$mix_root/scripts/prepare_gui_assets.py" "$mix_deps/knob-assets/assets/mixengine"
# Rootless Debian dependencies: signed metadata/checksums remain enabled.
mix_apt="$mix_deps/apt"
mix_sysroot="$mix_deps/sysroot"
mkdir -p "$mix_apt/lists/partial" "$mix_apt/archives/partial" "$mix_apt/empty-conf" "$mix_sysroot"
cat > "$mix_apt/sources.list" <<'APT_SOURCE'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
APT_SOURCE
cat > "$mix_apt/apt.conf" <<'APT_CONF'
Dir::Etc::parts "/workspace/dependencies/apt/empty-conf";
Dir::Etc::sourcelist "/workspace/dependencies/apt/sources.list";
Dir::Etc::sourceparts "-";
Dir::State::lists "/workspace/dependencies/apt/lists";
Dir::Cache::archives "/workspace/dependencies/apt/archives";
APT::Sandbox::User "agent";
Debug::NoLocking "true";
Acquire::Languages "none";
APT_CONF
export APT_CONFIG="$mix_apt/apt.conf"
apt-get update
apt-get --download-only --no-install-recommends -y install libx11-dev libfreetype-dev libxcb-util-dev libxcb-cursor-dev libxcb-keysyms1-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libglib2.0-dev libcairo2-dev libpango1.0-dev libfontconfig-dev libwayland-dev wayland-protocols libpcre2-dev libexpat1-dev libbrotli-dev zlib1g-dev libpng-dev libffi-dev libgtkmm-3.0-dev libsqlite3-dev xvfb xfonts-base gdb
python - <<'PY'
from pathlib import Path
import subprocess,os
archives=Path('/workspace/dependencies/apt/archives');sysroot=Path('/workspace/dependencies/sysroot')
def extract():
 for deb in archives.glob('*.deb'):subprocess.run(['dpkg-deb','-x',str(deb),str(sysroot)],check=True)
extract();packages=set()
for link in (sysroot/'usr/lib/x86_64-linux-gnu').glob('lib*.so'):
 if link.is_symlink() and not link.exists():
  result=subprocess.run(['dpkg-query','-S','*/'+link.readlink().name],capture_output=True,text=True)
  if result.returncode==0:
   packages.update(line.split(': ')[0] for line in result.stdout.splitlines())
if packages:subprocess.run(['apt-get','download',*sorted(packages)],cwd=archives,check=True)
extract()
PY
export PATH="$mix_sysroot/usr/bin:$PATH"
export PKG_CONFIG_SYSROOT_DIR="$mix_sysroot"
export PKG_CONFIG_LIBDIR="$mix_sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$mix_sysroot/usr/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig"
export LIBRARY_PATH="$mix_sysroot/usr/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$mix_sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$mix_tools/bin/cmake" -S "$mix_root" -B /workspace/build/gui -G Ninja -DCMAKE_MAKE_PROGRAM="$mix_tools/bin/ninja" -DCMAKE_BUILD_TYPE=Release -DVST3_SDK_ROOT="$mix_deps/vst3sdk" -DMIXENGINE_KNOB_ASSET_ROOT="$mix_deps/knob-assets/assets/mixengine" -DFETCHCONTENT_SOURCE_DIR_HIIR="$mix_deps/hiir" -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF -DSMTG_CREATE_PLUGIN_LINK=OFF -DCMAKE_PREFIX_PATH="$mix_sysroot/usr" -DCMAKE_FIND_ROOT_PATH="$mix_sysroot" -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_CXX_FLAGS="-I$mix_deps" -DEXPAT_LIBRARY="$mix_sysroot/usr/lib/x86_64-linux-gnu/libexpat.so"
"$mix_tools/bin/cmake" --build /workspace/build/gui --target 125A-MixEngine-V3 125A-MixEngine-V3-Channel validator controller-lifecycle-diagnostic audit-host-contract audit-channel-contract audit-linux-editor --parallel 4
python "$mix_root/scripts/audit_headless.py" --sdk-root "$mix_deps/vst3sdk" --hiir-root "$mix_deps/hiir" --build-root /workspace/build/reusable --cmake "$mix_tools/bin/cmake" --ninja "$mix_tools/bin/ninja" --parallel 4
