#!/usr/bin/env bash
ALSA_LIB=$(nix eval --raw nixpkgs#alsa-lib.outPath)
export LD_LIBRARY_PATH="$ALSA_LIB/lib:$LD_LIBRARY_PATH"
exec ./build/game/game "$@"
