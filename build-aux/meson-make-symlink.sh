#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
	echo "usage: $0 TARGET LINK_PATH" >&2
	exit 1
fi

target=$1
link_path=${DESTDIR:-}$2

mkdir -p "$(dirname "$link_path")"
rm -f "$link_path"
ln -s "$target" "$link_path"
