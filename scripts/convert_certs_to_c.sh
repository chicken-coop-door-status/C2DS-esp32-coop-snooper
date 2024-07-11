#!/bin/bash

# Function to convert a file to a C file using xxd
convert_to_c() {
    local input_file=$1
    local c_file=$2
    local var_name=$3

    echo "Converting $input_file to $c_file..."

    # Create the .c file and add the include guard and extern declaration
    echo "#include <stdint.h>" > "$c_file"
    echo "" >> "$c_file"

    # Use xxd to convert the input file to a C array
    xxd -i -n "${var_name}" "$input_file" | sed 's/unsigned char/const uint8_t/g' >> "$c_file"

#    echo "Conversion completed: $c_file"
}

# Iterate over all .pem and .key files in the current directory
for input_file in *.pem *.key; do
    # Check if file exists to avoid errors when no files are found
    [ -e "$input_file" ] || continue

    # Generate the .c file name by replacing the extension with .c
#    c_file="${input_file%.*}.c"

    # Generate a valid C variable name from the file name
    var_name=$(basename "$input_file" | sed 's/[^a-zA-Z0-9_]/_/g')
    echo "var_name: $var_name"

    c_file="${var_name}.c"

    # Call the conversion function
    convert_to_c "$input_file" "$c_file" "${var_name}"
done

# Inform user of completion
echo "All .pem and .key files have been converted to C files."
