#!/bin/sh
# Release archive: the contents of app/dist/ (built by build-device.sh) as InkToday-<version>.zip,
# plus its SHA-256. Unpack the archive into the root of the reader's internal storage.
set -e
here="$(cd "$(dirname "$0")/.." && pwd)"
version="$(sed -n 's/^VERSION = //p' "$here/common.pri")"
out="$(pwd)/InkToday-$version.zip"
[ -d "$here/dist/system" ] || { echo "Run app/package/build-device.sh first." >&2; exit 1; }
rm -f "$out"
(cd "$here/dist" && zip -q -r -X "$out" applications system)
(cd "$(dirname "$out")" && sha256sum "$(basename "$out")" > "$out.sha256")
echo "Ready: $out"
cat "$out.sha256"
