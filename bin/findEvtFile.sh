#!/usr/bin/bash

# Script: findEvtFile.sh
# Description: Searches for specific event files in the ICARUS data catalog
# This script uses SAMWeb to find and locate files matching event criteria

# Define dataset query for specific run and event range
files=$(samweb -e icarus list-files "defname: run2_compression_production_v09_82_02_01_numimajority_compressed_data and run_number 9746 and first_event > 164000 and last_event < 172000")

# Loop through each file found and locate it, then search for specific event
for f in $files; do
    echo "------------------------------"
    echo "File: $f"
    
    # Locate file in dcache storage
    path=$(samweb -e icarus locate-file "$f" | head -n 1)
    path=${path#dcache:}  # Remove dcache: prefix
    filePath="${path}/${f}"
    
    echo "Full Path: $filePath"
    
    # Search for event 166767 in the file
    file_info_dumper --event-list $filePath | grep 166767
done