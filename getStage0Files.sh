#!/bin/bash

# Script: getStage0Files.sh
# Description: Retrieves Stage 0 files for hit tuning analysis from ICARUS MC production
# This script queries SAMWeb for MC files and generates a list of accessible file paths

# Define the dataset name for Stage 0 MC files
dataset="production_mc_2025A_ICARUS_Overlays_BNB_MC_RUN2_September_v10_06_00_04p04_stage0"

# Get list of files in the dataset
files=$(samweb -e icarus list-files "defname: $dataset")

# Initialize array to store file paths
combineFiles=()
counter=0

# Process files 31-59 from the dataset (skipping first 30)
for file in $files; do
    # Skip first 30 files
    if [ $counter -le 30 ]; then
        ((counter++))
        continue
    fi
    
    # Stop after collecting 30 files (files 31-60)
    if [ $counter -ge 60 ]; then
        break
    fi
    
    # Get file location and build full path
    loc=$(samweb -e icarus locate-file $file)
    loc=${loc#dcache:/pnfs/}  # Remove dcache:/pnfs/ prefix
    path="root://fndcadoor.fnal.gov:/$loc/$file"
    
    # Add to array
    combineFiles+=("$path")
    ((counter++))
done

# Write file paths to output file (one per line)
output_file="stage0_files2.txt"
printf '%s\n' "${combineFiles[@]}" > "$output_file"

echo "Wrote ${#combineFiles[@]} entries to $output_file"
