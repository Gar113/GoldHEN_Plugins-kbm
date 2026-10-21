#!/bin/sh
# Bangun KBM Config PKG.
#
# Kenapa perlu script: PkgTool.Core adalah aplikasi .NET Core 3.0 yang
# menuntut OpenSSL 1.1, sedangkan sistem ini hanya punya OpenSSL 3.
# Folder /tmp sering dibersihkan, jadi library-nya disalin ulang tiap
# kali script ini jalan. Salinannya diambil dari runtime Steam yang
# kebetulan masih menyertakan OpenSSL 1.1.

SRC="$HOME/.local/share/Steam/steamrt64/pv-runtime/steam-runtime-steamrt/steamrt3c_platform_3c.0.20260824.257593/files/lib/x86_64-linux-gnu"
DST="/tmp/opencode/ossl11/usr/lib"

if [ ! -f "$DST/libssl.so.1.1" ]; then
    mkdir -p "$DST"
    cp -a "$SRC/libssl.so.1.1" "$SRC/libcrypto.so.1.1" "$DST/" 2>/dev/null
fi

if [ ! -f "$DST/libssl.so.1.1" ]; then
    echo "PERINGATAN: OpenSSL 1.1 tidak ketemu di $SRC"
    echo "PKG tidak akan bisa dibuild. Cari libssl.so.1.1 di sistem."
    exit 1
fi

cd "$(dirname "$0")" || exit 1
export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$HOME/OpenOrbis/PS4Toolchain}"
export LD_LIBRARY_PATH="$DST:$LD_LIBRARY_PATH"

make "$@"
