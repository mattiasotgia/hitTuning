# Hit Tuning Directory

> *Disclaimer* :shipit:\
> The work of this repository was developed by M Carrigan (<mcarrig@fnal.gov>), original repository lives in [carriganm95:icarusProjects/hitTuning](https://github.com/carriganm95/icarusProjects/tree/master/hitTuning). 

This directory contains scripts used in determining the parameters to be used for hit finding in ICARUS. There are tools for creating and running a grid search over parameters on the grid.

## hitTuning.py

Main script for hit finding parameter optimization. Can be run interactively or over the grid to implement specific hit tuning parameters with a minimal stage0 data or MC FCL file.

### Purpose
- Modify FCL configuration files with custom hit finding parameters
- Run hit finding with specific parameter sets
- Create FCL files for grid search parameter scans

### Command Line Options
- `-c, --createFcls`: Create FCL files for grid search (does not run jobs)
- `-i, --inputFcl`: Input FCL template file path
- `-o, --outputDir`: Directory for output files
- `-d, --dataFile`: Input data file to process
- `-r, --run`: Run number
- `-s, --subrun`: Subrun number
- `-e, --event`: Event number

### Parameters Tunable
The script supports modification of the following hit finding parameters (per plane or global):
- `roiThreshold`: Threshold for ROI (Region of Interest) finding [default: 5.0]
- `minPulseHeight`: Minimum pulse height for hit candidate [default: 2.0]
- `minPulseSigma`: Minimum pulse width (sigma) [default: 1.0]
- `LongMaxHits`: Maximum hits allowed in long pulses [default: 1]
- `LongPulseWidth`: Width threshold for long pulses [default: 10.0]
- `PulseHeightCuts`: Height cuts for pulse splitting [default: 3.0]
- `PulseWidthCuts`: Width cuts for pulse splitting [default: [2, 1.5, 1]]
- `PulseRatioCuts`: Ratio cuts for pulse splitting [default: [0.35, 0.4, 0.2]]
- `MaxMultiHit`: Maximum number of hits per ROI [default: 5]
- `Chi2NDF`: Chi-squared per degree of freedom cut [default: 500.0]

### Grid Search Usage
```bash
# Create FCL files for grid search
python hitTuning.py -c --outputDir ./fcls/

# The script will generate multiple FCL files with different parameter combinations
```

### Database Output
Results are stored in SQLite databases with the following schema:
- Run information (run, subrun, event)
- Parameter values used
- Hit finding metrics
- Performance statistics

## galleryMC.cpp and galleryMacro.cpp

Gallery macros that run on Wire, ChannelROI, and Hit data products.

### galleryMC.cpp (MC version)
**Purpose**: Analyze hit finding performance against truth information

**Input Requirements**:
- MC (Monte Carlo) simulation files
- Must contain: Wire, Hit, and SimChannel/IDE truth information

**Output**:
- Hit vs truth IDE energy per event
- Metrics for all particles and by particle type
- Used to determine optimal parameter sets

### galleryMacro.cpp
**Purpose**: General hit analysis on data or MC

**Output**:
- Basic hit histograms (charge, width, time, etc.)
- ROI vs hit comparison plots for specific channels
- Channel-by-channel statistics

## runJob.sh

Grid job executable script.

### Purpose
Execute individual grid jobs for hit tuning parameter scans.

### Usage
Called automatically by the grid submission system. Not typically run directly by users.

## submitJobs.sh

Grid job submission script.

### Purpose
Submit batch of jobs for grid search over hit finding parameters.

### Typical Workflow
1. Create FCL files with `hitTuning.py -c`
2. Edit `submitJobs.sh` to specify:
   - Input file lists
   - Output directories
   - Number of jobs
   - Resources (memory, time)
3. Submit: `./submitJobs.sh`

### Grid System
Uses Fermilab jobsub system for job submission.

## mergeDBFiles.py

Merge multiple SQLite database files from a grid search into a single database.

### Purpose
Consolidate results from many grid jobs into one database for analysis.

### Command Line Usage
Edit the script to specify:
- `inputDir`: Directory containing .db files from grid jobs
- `dest_db`: Path to output merged database file

```python
python mergeDBFiles.py
```

## eventDisplay.py

Create event displays from stage1 reconstruction files showing wires/ROIs and hits.

### Purpose
Visualize detector response and hit finding results for specific events.

### Command Line Options
- `-o, --outputDir`: Output directory for plots [default: 'eventDisplays']
- `--tag`: Tag for output files [default: 'test']
- `-d, --debug`: Enable debug mode
- `-v, --verbose`: Enable verbose output
- `-i, --inputFile`: Input ROOT file (stage1 reco)
- `-e, --eventNumber`: Event number to display [default: 0]
- `-p, --plane`: Plane number (0, 1, or 2) [default: 0]
- `-t, --timeRange`: Time range in ticks [min, max] [default: [0, 5000]]
- `-w, --wireRange`: Wire/channel range [min, max] [default: auto]

### Usage Examples
```bash
# Display event 100, plane 2
python eventDisplay.py -i reco_file.root -e 100 -p 2

# Display with custom time/wire ranges
python eventDisplay.py -i reco_file.root -e 50 -p 1 -t 1000 3000 -w 100 500

# Verbose debug mode
python eventDisplay.py -i reco_file.root -e 25 -p 0 -v -d
```

### Output
- PNG images: Event displays with wire signals and hit overlays
- ROOT file: Histograms and canvases for further analysis

## Additional Files

### Jupyter Notebooks
- `compareHits.ipynb`: Interactive notebook for comparing hit finding results

### Shell Scripts
- `getStage0Files.sh`: Retrieve stage0 files for processing
- `findEvtFile.sh`: Locate files containing specific events

## Typical Workflow

1. **Parameter Selection**: Decide which parameters to scan
2. **Grid Search Setup**: 
   ```bash
   python hitTuning.py -c --outputDir ./fcl_files/
   ```
3. **Job Submission**: 
   ```bash
   ./submitJobs.sh
   ```
4. **Monitor Jobs**: Check job status with `jobsub_q`
5. **Merge Results**: 
   ```bash
   python mergeDBFiles.py
   ```
6. **Analysis**: Use notebooks to analyze merged database
7. **Visualization**: Create event displays for interesting events
   ```bash
   python eventDisplay.py -i file.root -e EVENT_NUM -p PLANE
   ```

### C++
- gallery framework
- LArSoft data products
- ICARUS-specific products
