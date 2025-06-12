#!/usr/bin/env bash

set -x


# extract version from the zephyr version file
VERSION_FILE="./VERSION"
if [ ! -f "$VERSION_FILE" ]; then
  echo "VERSION file not found!"
  exit 1
fi

# read version info from file line by line
while IFS='=' read -r key value || [ -n "$key" ]; do
    # Trim leading/trailing whitespace from key and value
    key=$(echo "$key" | sed 's/^[ \t]*//;s/[ \t]*$//')
    value=$(echo "$value" | sed 's/^[ \t]*//;s/[ \t]*$//')

    # Skip empty lines or lines without an equals sign
    if [ -z "$key" ] || [ -z "$value" ] && [[ ! "$key" =~ EXTRAVERSION ]]; then # EXTRAVERSION can be empty
        continue
    fi

    # Make variable names shell-friendly (e.g., convert to uppercase)
    var_name=$(echo "$key" | tr '[:lower:]' '[:upper:]')

    # Assign to a variable or print
    declare "$var_name=$value"
    echo "$var_name=$value"

done < "$VERSION_FILE"

# now you can use the variables like VERSION_MAJOR, VERSION_MINOR, etc.
# echo "---"
# echo "Major: $VERSION_MAJOR"
# echo "Minor: $VERSION_MINOR"
# echo "Patchlevel: $PATCHLEVEL"
# echo "Tweak: $VERSION_TWEAK"
# echo "Extraversion: $EXTRAVERSION"

# artifact type: zephyr-image
UPDATE_MODULE="zephyr-image"
ARTIFACT_NAME="mender-artifact_${VERSION_MAJOR}.${VERSION_MINOR}.${PATCHLEVEL}+${VERSION_TWEAK}${EXTRAVERSION}"
DEVICE_TYPE="nucleo_f767zi"

mender-artifact write module-image \
  --type $UPDATE_MODULE \
  --file build/zephyr/zephyr.signed.bin \
  --artifact-name $ARTIFACT_NAME \
  --device-type $DEVICE_TYPE \
  --compression none