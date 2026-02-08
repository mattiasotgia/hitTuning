#!/bin/bash

# Script: submitJobs.sh
# Description: Submits hit tuning jobs to the FNAL grid computing system
# This script prepares the environment and submits multiple jobs using jobsub_submit

# ==============================================================================
# Configuration Variables
# ==============================================================================

# Source directory on the grid storage
sourceDir="/pnfs/icarus/scratch/users/micarrig/hitTuning/mc/gridTest/"

# Main executable script to run on grid nodes
exe="runJob.sh"

# Export variables for grid jobs
export fclFile=""                    # FCL configuration file (if needed)
export tarFile="hitTuning.tar.gz"    # Tarball with code and dependencies
export fileList=""                   # List of input files (if needed)
export outputDir=$sourceDir          # Output directory for results
export nEvents=-1                    # Number of events to process (-1 = all)
export anaFile="hitTuning.py"        # Analysis script
export treeName=""                   # Tree name for validation (optional)

# Calculate number of jobs from FCL files
nJobs=$(ls -l ${sourceDir}/gridFcl/*.fcl | wc -l)

# Recopy files to grid storage?
recopy=false

# Python packages tarball
pythonPackages="python.tar.gz"

# ==============================================================================
# Setup Output Directories
# ==============================================================================

# Create main output directories if they don't exist
if [ ! -f $outputDir ]; then
    mkdir -p $outputDir
fi
if [ ! -f $outputDir/logs ]; then
    mkdir -p $outputDir/logs
fi
if [ ! -f "$outputDir/outputs" ]; then
    mkdir -p "$outputDir/outputs"
fi

# Create subdirectories for outputs and logs (groups of 100 jobs)
for ((i=0; i<$((($nJobs + 99) / 100)); i++)); do
    subdir=$(printf "%02d" $i)
    if [ ! -d $outputDir/outputs/$subdir ]; then
        mkdir -p $outputDir/outputs/$subdir
    fi
    if [ ! -d $outputDir/logs/$subdir ]; then
        mkdir -p $outputDir/logs/$subdir
    fi
done

# ==============================================================================
# Copy Required Files to Grid Storage
# ==============================================================================

# Copy FCL file if needed
if [ -n "$fclFile" ] && ( [ ! -f ${sourceDir}/${fclFile} ] || [ "$recopy" = true ] ); then
    echo "Copying fcl file to scratch area"
    cp $PWD/${fclFile} ${sourceDir}/.
fi

# Copy tarball if needed
if [ -n "$tarFile" ] && ( [ ! -f ${sourceDir}/${tarFile} ] || [ "$recopy" = true ] ); then
    echo "Copying tar file to scratch area"
    cp /pnfs/icarus/scratch/users/micarrig/${tarFile} ${sourceDir}/.
fi

# Copy analysis script if needed
if [ -n "$anaFile" ] && ( [ ! -f ${sourceDir}/${anaFile} ] || [ "$recopy" = true ] ); then
    echo "Copying analysis file to scratch area"
    cp $PWD/${anaFile} ${sourceDir}/.
fi

# Copy Python packages if needed
if [ -n "$pythonPackages" ] && ( [ ! -f ${sourceDir}/${pythonPackages} ] || [ "$recopy" = true ] ); then
    echo "Copying python packages file to scratch area"
    cp /pnfs/icarus/scratch/users/micarrig/${pythonPackages} ${sourceDir}/.
fi

# Copy file list if needed
if [ -n "$fileList" ] && ( [ ! -f ${sourceDir}/${fileList} ] || [ "$recopy" = true ] ); then
    echo "Copying file list to scratch area"
    cp ${fileList} ${sourceDir}/.
fi

# ==============================================================================
# Authentication and Job Submission
# ==============================================================================

# Get bearer token for authentication
export BEARER_TOKEN_FILE=/tmp/bt_u$(id -u) && htgettoken -a htvaultprod.fnal.gov -i icarus

echo "Submitting $nJobs jobs to process"
echo "Will process $nEvents events per job"
echo "Using executable $exe and tar file $tarFile"
echo "Output will be stored in $outputDir"

# Build jobsub_submit command with resource requirements
jobsub_cmd="jobsub_submit -G icarus -N ${nJobs} --maxConcurrent 50 --expected-lifetime=18h --disk=25GB --memory=8000MB -e IFDH_CP_MAXRETRIES=4 -e IFDH_CP_UNLINK_ON_ERROR=2 --lines '+FERMIHTC_AutoRelease=True' --lines '+FERMIHTC_GraceMemory=4096' --lines '+FERMIHTC_GraceLifetime=3600' -l '+SingularityImage=\"/cvmfs/singularity.opensciencegrid.org/fermilab/fnal-wn-sl7\:latest\"' --append_condor_requirements='(TARGET.HAS_Singularity==true)'"

# Add tarball to command if specified
if [ -n "$tarFile" ] && [ -f "${sourceDir}/${tarFile}" ]; then
    jobsub_cmd="${jobsub_cmd} --tar_file_name dropbox://${sourceDir}/${tarFile} --use-cvmfs-dropbox -e tarFile"
fi

# Add FCL file to command if specified
if [ -n "$fclFile" ] && [ -f "${sourceDir}/${fclFile}" ]; then
    jobsub_cmd="${jobsub_cmd} -e fclFile -f ${sourceDir}/${fclFile}"
fi

# Add analysis file to command if specified
if [ -n "$anaFile" ] && [ -f "${sourceDir}/${anaFile}" ]; then
    jobsub_cmd="${jobsub_cmd} -e anaFile -f ${sourceDir}/${anaFile}"
fi

# Add tree name environment variable if specified
if [ -n "$treeName" ]; then
    jobsub_cmd="${jobsub_cmd} -e treeName"
fi

# Add Python packages to command if specified
if [ -n "$pythonPackages" ]; then
    jobsub_cmd="${jobsub_cmd} -f ${sourceDir}/${pythonPackages}"
fi

# Add file list to command if specified
if [ -n "$fileList" ]; then
    jobsub_cmd="${jobsub_cmd} -e fileList -f ${sourceDir}/${fileList}"
fi

# Add output directory and event count environment variables, plus executable
jobsub_cmd="${jobsub_cmd} -e outputDir -e nEvents file://${exe}"

# Execute job submission
echo "Executing jobsub command:"
echo "$jobsub_cmd"
eval $jobsub_cmd
