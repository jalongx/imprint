#!/bin/bash

# Usage: ./verify-single.sh image.lz4
# or:    ./verify-single.sh image.zst

image="$1"

if [ -z "$image" ]; then
    echo "Usage: $0 <imagefile>"
    exit 1
fi

# Strip trailing .000 if user accidentally passed a chunk
image="${image%.000}"

# Ensure the image exists
if [ ! -f "$image" ]; then
    echo "Image file not found: $image"
    exit 1
fi

# Ensure the .sha256 file exists
if [ ! -f "$image.sha256" ]; then
    echo "Checksum file not found: $image.sha256"
    exit 1
fi

echo "Verifying non-chunked image: $image"

# Extract expected checksum from .sha256 file
expected=$(cut -d' ' -f1 "$image.sha256")

# Compute checksum of the image file
computed=$(sha256sum "$image" | cut -d' ' -f1)

echo "Expected: $expected"
echo "Computed: $computed"

if [ "$expected" = "$computed" ]; then
    echo "✔ Verification successful"
    exit 0
else
    echo "❌ Verification failed"
    exit 1
fi
