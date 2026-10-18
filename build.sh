#!/usr/bin/env bash
# Builds the gm/Id Schemify plugin for wasm32-unknown-unknown into ./gmid_lut.wasm (committed:
# a Schemify package is a prebuilt module, config spec §5). Fails if the module reaches 200 KB.
#
# Reproducible: CI rebuilds the module and fails when the committed one differs. The bytes
# depend only on the sources, Cargo.lock and the toolchain: build in `nix develop` (its
# nixpkgs-rust input pins the rustc and wasm-ld) or commit the `gmid-lut-wasm` artifact of a
# CI run. Host paths are remapped out of the module (panic locations of git and registry
# crates would otherwise name this checkout and ~/.cargo).
#
# Without a rust-lld on the toolchain (the nix rustc has none) it links with wasm-ld, re-running
# itself under `nix shell nixpkgs#lld` when wasm-ld is not on PATH either.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
host="$(rustc -vV | sed -n 's/^host: //p')"
pinned="$(sed -n 's/^rust-version = "\(.*\)"/\1/p' "$here/Cargo.toml")"
if ! rustc -V | grep -q "^rustc $pinned\(\.0\)\? "; then
  echo "warning: $(rustc -V) is not the pinned rustc $pinned (nix develop): the module will" \
    "not match the committed one byte for byte" >&2
fi
# Unit separator between flags: paths may contain spaces.
export CARGO_ENCODED_RUSTFLAGS="--remap-path-prefix=$here=/GmIDVisualizer"$'\x1f'"--remap-path-prefix=${CARGO_HOME:-$HOME/.cargo}=/cargo"

if [[ ! -x "$(rustc --print sysroot)/lib/rustlib/$host/bin/rust-lld" ]] \
   && [[ -z "${CARGO_TARGET_WASM32_UNKNOWN_UNKNOWN_LINKER:-}" ]]; then
  if command -v wasm-ld >/dev/null; then
    export CARGO_TARGET_WASM32_UNKNOWN_UNKNOWN_LINKER=wasm-ld
  else
    exec nix shell nixpkgs#lld -c env CARGO_TARGET_WASM32_UNKNOWN_UNKNOWN_LINKER=wasm-ld "$0" "$@"
  fi
fi

cargo build --manifest-path "$here/Cargo.toml" --release --target wasm32-unknown-unknown --lib

target="${CARGO_TARGET_DIR:-$here/target}"
wasm="$target/wasm32-unknown-unknown/release/gmid_lut.wasm"
size=$(wc -c <"$wasm")
if (( size >= 200 * 1024 )); then
  echo "error: gmid_lut.wasm is $size bytes (limit 200 KB)" >&2
  exit 1
fi
cp "$wasm" "$here/gmid_lut.wasm"
echo "gmid_lut.wasm $size bytes"
