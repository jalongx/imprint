#!/bin/bash

base="$1"

# Strip trailing .000 if user passed a chunk filename
base="${base%.000}"

# Expected checksum
expected=$(cut -d' ' -f1 "$base.sha256")

# Find chunk files only (000, 001, 002...)
chunks=$(ls "${base}".[0-9][0-9][0-9] 2>/dev/null | sort -V)

if [ -z "$chunks" ]; then
    echo "No chunk files found for $base"
    exit 1
fi

echo "Verifying chunked image: $base"
echo "Chunks:"
echo "$chunks"

# Compute checksum of reconstructed stream
computed=$(cat $chunks | sha256sum | cut -d' ' -f1)

echo "Expected: $expected"
echo "Computed: $computed"

if [ "$expected" = "$computed" ]; then
    echo "✔ Verification successful"
    exit 0
else
    echo "❌ Verification failed"
    exit 1
fi
