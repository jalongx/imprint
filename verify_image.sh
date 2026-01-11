#!/bin/bash

# ------------------------------------------------------------
# verify_img.sh
#
# Verifies either:
#   • a single Imprint image file (e.g., image.zst, image.lz4)
#   • a chunked Imprint image (image.000, image.001, ...)
#
# Usage:
#     verify_img <imagefile or chunk>
#
# Examples:
#     verify_img backup-2025-01-10.zst
#     verify_img backup-2025-01-10.img.zst.000
#
# The script automatically:
#   • detects chunked vs non-chunked images
#   • reconstructs chunked streams in correct order
#   • uses pv for progress when available
#   • falls back to sha256sum without progress
# ------------------------------------------------------------

# --- Colors ---------------------------------------------------------------

GREEN="\e[32m"
RED="\e[31m"
YELLOW="\e[93m"
BLUE="\e[34m"
RESET="\e[0m"

# --- Argument check -------------------------------------------------------

if [ -z "$1" ]; then
    echo
    echo -e "${YELLOW}Usage:${RESET} $0 <imagefile or chunk>"
    echo
    echo "Examples:"
    echo "  $0 myimage.zst"
    echo "  $0 myimage.img.zst.000"
    exit 1
fi

input="$1"

# Strip trailing .000 if user passed a chunk filename
base="${input%.000}"

# --- Check for checksum file ---------------------------------------------

if [ ! -f "$base.sha256" ]; then
    echo -e "${RED}Error:${RESET} Expected checksum file '$base.sha256' not found."
    exit 1
fi

expected=$(cut -d' ' -f1 "$base.sha256")

# --- Detect chunked vs single-file ---------------------------------------

chunks=$(ls "${base}".[0-9][0-9][0-9] 2>/dev/null | sort -V)

if [ -n "$chunks" ]; then
    mode="chunked"
else
    mode="single"
fi

# --- Verification ---------------------------------------------------------

if [ "$mode" = "chunked" ]; then
    echo
    echo -e "${BLUE}Verifying chunked image:${RESET} $base"
    echo
    echo -e "${YELLOW}Found chunks:${RESET}"
    echo "$chunks"
    echo

    if command -v pv >/dev/null 2>&1; then
        total_size=$(du -cb $chunks | awk 'END{print $1}')
        computed=$(cat $chunks | pv -s "$total_size" | sha256sum | cut -d' ' -f1)
    else
        computed=$(cat $chunks | sha256sum | cut -d' ' -f1)
    fi

else
    image="$base"

    if [ ! -f "$image" ]; then
        echo -e "${RED}Error:${RESET} Image file not found: $image"
        exit 1
    fi

    echo -e "${BLUE}Verifying single image:${RESET} $image"
    echo

    if command -v pv >/dev/null 2>&1; then
        size=$(stat -c%s "$image")
        computed=$(pv -s "$size" "$image" | sha256sum | cut -d' ' -f1)
    else
        computed=$(sha256sum "$image" | cut -d' ' -f1)
    fi
fi

# --- Compare --------------------------------------------------------------

echo
echo "Expected: $expected"
echo "Computed: $computed"
echo

if [ "$expected" = "$computed" ]; then
    echo -e "${GREEN}✔ Verification successful — image matches original${RESET}"
    exit 0
else
    echo -e "${RED}❌ Verification failed — checksum mismatch${RESET}"
    exit 1
fi
