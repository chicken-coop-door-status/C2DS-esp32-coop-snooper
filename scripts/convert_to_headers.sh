#!/bin/bash

# Function to convert a file to a C header using xxd
convert_to_header() {
  local filename=$1
  local headername=$(basename "$filename" .pem).h
  local varname=$(basename "$filename" .pem)

  # Handle .key files
  if [[ "$filename" == *.key ]]; then
    headername=$(basename "$filename" .key).h
    varname=$(basename "$filename" .key)
  fi

  # Convert the file to a C header
  xxd -i "$filename" > "$headername"

  # Rename the variable inside the header to a more meaningful name
  sed -i "s/unsigned char \([^ ]*\)/unsigned char ${varname}_pem/" "$headername"
  sed -i "s/unsigned int \([^ ]*\)_len/unsigned int ${varname}_pem_len/" "$headername"

  echo "Converted $filename to $headername"
}

# Find and convert all .pem and .key files in the current directory
for file in *.pem *.key; do
  if [ -f "$file" ]; then
    convert_to_header "$file"
  else
    echo "No .pem or .key files found in the current directory."
  fi
done
