#!/bin/bash

# ====================================================================================
# SCRIPT:      bash_curl_downloader.sh
# DESCRIPTION: A simple bash script to download a file from a URL using curl.
# AUTHOR:      Gemini
# DATE:        October 26, 2023
# ====================================================================================

# ------------------------------------------------------------------------------------
# 1. INPUT VALIDATION
# ------------------------------------------------------------------------------------
# Check if the correct number of arguments is provided.
# The script expects exactly two arguments: the URL and the desired output filename.
if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <URL_to_download> <output_filename>"
  echo "Example for base pkg: $0 https://midnight-coding.de/blog/assets/l1vm/l1vm-base-pkg.tar.bz2 l1vm-base-pkg.tar.bz2"
  exit 1
fi

# Assign the input arguments to descriptive variables.
URL="$1"
OUTPUT_FILE="$2"

# ------------------------------------------------------------------------------------
# 2. FILE DOWNLOAD
# ------------------------------------------------------------------------------------
echo "Starting download..."
echo "URL: $URL"
echo "Saving to: $OUTPUT_FILE"

# Use curl to download the file.
# The -L flag follows any redirects.
# The -o flag specifies the output filename.
# The -# flag shows a simple progress bar.
curl -L -o "$OUTPUT_FILE" "$URL"

# ------------------------------------------------------------------------------------
# 3. VERIFICATION AND ERROR HANDLING
# ------------------------------------------------------------------------------------
# Check the exit status of the previous command (`curl`).
# A status of 0 indicates success.
if [ $? -eq 0 ]; then
  echo "Download successful!"
  # Use `ls` with the -h (human-readable) and -l (long listing) flags to show the file size.
  ls -lh "$OUTPUT_FILE"

  mkdir ~/l1vm
  tar xjf $OUTPUT_FILE -C ~/l1vm/
  echo "Package installed!"

  exit 0
else
  echo "Error: Download failed."
  # Provide a more specific error message based on the curl command's exit code.
  # Note: The exit codes can vary, this is a basic check.
  # For more robust error handling, you could check the status codes from `curl` itself.
  exit 1
fi
