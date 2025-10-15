#!/bin/bash

# Check if an argument was given
if [ -z "$1" ]; then
    echo "Usage: $0 <ClassName> (e.g., Device or Buffer)"
    exit 1
fi

NAME="$1"

# Define file paths
FILES=(
    "include/swarm/${NAME}.h"
    "include/swarm/${NAME}backend.inl"
    "include/swarm/inl/vulkan/${NAME}vulkan.inl"
    "src/swarm/vulkan/${NAME}_vulkan.cpp"
)

# Loop through each file and create it if it doesn't exist
for FILE in "${FILES[@]}"; do
    DIR=$(dirname "$FILE")

    # Create the directory if it doesn't exist
    if [ ! -d "$DIR" ]; then
        echo "Creating directory: $DIR"
        mkdir -p "$DIR"
    fi

    # Check if file already exists
    if [ -f "$FILE" ]; then
        echo "⚠️  File already exists: $FILE"
    else
        echo "Creating file: $FILE"
        touch "$FILE"
    fi
done

echo "✅ Done."
