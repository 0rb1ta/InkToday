#!/bin/sh
# Cross-compiles InkToday for PocketBook (SDK B288 6.8) and lays out app/dist/ to mirror
# the reader's internal storage: copy the contents of dist/ onto the reader's root.
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
. "${PB_ENV:-$HOME/pocketbook-sdk/env.sh}"

mkdir -p "$here/build"
cd "$here/build"
qmake ../app.pro CONFIG+=release
make -s -j"$(nproc)"
arm-obreey-linux-gnueabi-strip inktoday

dist="$here/dist"
rm -rf "$dist"
mkdir -p "$dist/applications/icons" "$dist/system/inktoday/bin"
cp inktoday "$dist/system/inktoday/bin/inktoday"
# The firmware has no CA store Qt can use. A fixed copy of the Mozilla set (Ubuntu ca-certificates
# 20260816, taken 2026-10-29) ships with the app, so every build trusts the same roots.
cp "$here/package/cacert.pem" "$dist/system/inktoday/bin/cacert.pem"
cp "$here/package/InkToday.app" "$dist/applications/InkToday.app"
# Launcher icons for the Applications menu (used only if the user adds them to view.json).
cp "$here/third_party/inktoday/icon.bmp" "$dist/applications/icons/inktoday.bmp"
cp "$here/third_party/inktoday/icon_f.bmp" "$dist/applications/icons/inktoday_f.bmp"
echo "Ready: copy the contents of $dist onto the reader's internal storage root."
