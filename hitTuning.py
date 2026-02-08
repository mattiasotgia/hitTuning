import os
import sqlite3
from datetime import datetime
from typing import List, Optional, Dict, Any, Union, Tuple
import ROOT as r
from itertools import product
import argparse
import sys
import re

class fclParams:
    """Class to hold FCL (FHiCL) configuration parameters for hit finding."""
    
    def __init__(self, 
                roiThreshold: Union[List[float], float] = [5.0, 5.0, 5.0], 
                minPulseHeight: Union[List[float], float] = [2.0, 2.0, 2.0], 
                minPulseSigma: Union[List[float], float] = [1.0, 1.0, 1.0], 
                LongMaxHits: Union[List[int], int] = [1, 1, 1], 
                LongPulseWidth: Union[List[float], float] = [10.0, 10.0, 10.0], 
                PulseHeightCuts: Union[List[float], float] = [3, 3, 3], 
                PulseWidthCuts: Union[List[float], float] = [2, 1.5, 1],
                PulseRatioCuts: Union[List[float], float] = [3.5e-1, 4e-1, 2e-1],
                MaxMultiHit: int = 5,
                Chi2NDF: float = 500.0
                ) -> None:

        self.roiThreshold = self.setList(roiThreshold)
        self.minPulseHeight = self.setList(minPulseHeight)
        self.minPulseSigma = self.setList(minPulseSigma)
        self.LongMaxHits = self.setList(LongMaxHits)
        self.LongPulseWidth = self.setList(LongPulseWidth)
        self.PulseHeightCuts = self.setList(PulseHeightCuts)
        self.PulseWidthCuts = self.setList(PulseWidthCuts)
        self.PulseRatioCuts = self.setList(PulseRatioCuts)
        self.MaxMultiHit = MaxMultiHit
        self.Chi2NDF = Chi2NDF
    
    def setList(self, var: Union[List[Union[int, float]], int, float]) -> List[Union[int, float]]:
        """Convert single value to list of three identical values, or return list as-is.
        
        Args:
            var: Single value or list of values
            
        Returns:
            List with three values (replicated if input was single value)
        """
        if not isinstance(var, list):
            return [var, var, var]
        return var

    def __str__(self) -> str:
        """Return string representation of FCL parameters."""
        return f"""fclParams:
    roiThreshold: {self.roiThreshold}
    minPulseHeight: {self.minPulseHeight}
    minPulseSigma: {self.minPulseSigma}
    LongMaxHits: {self.LongMaxHits}
    LongPulseWidth: {self.LongPulseWidth}
    PulseHeightCuts: {self.PulseHeightCuts}
    PulseWidthCuts: {self.PulseWidthCuts}
    PulseRatioCuts: {self.PulseRatioCuts}
    MaxMultiHit: {self.MaxMultiHit}
    Chi2NDF: {self.Chi2NDF}"""

def _parse_value(val_str: str) -> Union[List, bool, int, float, str]:
    """Parse a single FCL RHS value into a Python type.
    
    Args:
        val_str: String value from FCL file
        
    Returns:
        Parsed value as appropriate Python type (list, bool, int, float, or str)
    """
    s = val_str.strip()

    # List
    if s.startswith('[') and s.endswith(']'):
        inner = s[1:-1].strip()
        if not inner:
            return []
        parts = [p.strip() for p in inner.split(',')]
        return [_parse_value(p) for p in parts]

    # Booleans
    if s.lower() in ('true', 'false'):
        return s.lower() == 'true'

    # Numbers
    try:
        if any(ch in s for ch in ('.', 'e', 'E')):
            return float(s)
        return int(s)
    except ValueError:
        pass

    # Quoted string
    if (s.startswith('"') and s.endswith('"')) or (s.startswith("'") and s.endswith("'")):
        return s[1:-1]

    # Bare string
    return s

def _ensure_list3(v):
    """Normalize a value to a list of length 3 (replicate scalars; pad/truncate lists)."""
    if isinstance(v, list):
        if len(v) == 3:
            return v
        if len(v) == 1:
            return [v[0], v[0], v[0]]
        if len(v) > 3:
            return v[:3]
        return v + [v[-1]] * (3 - len(v))
    return [v, v, v]

def get_all_values(key: str, text: str) -> List[Union[List, bool, int, float, str]]:
    # Matches lines like: key: <value> (ignoring trailing comments)
    pattern = rf"{re.escape(key)}\s*:\s*([^\n#]+)"
    matches = re.findall(pattern, text)
    return [_parse_value(m.strip()) for m in matches]

def parse_fcl_to_params(fcl_path: str) -> 'fclParams':
    """Read a FCL file and reconstruct an fclParams object."""
    with open(fcl_path, 'r') as f:
        text = f.read()

    # Per-plane thresholds appear multiple times; take the last occurrence of each
    rt0 = get_all_values('HitFinderToolVec.CandidateHitsPlane0.RoiThreshold', text)
    rt1 = get_all_values('HitFinderToolVec.CandidateHitsPlane1.RoiThreshold', text)
    rt2 = get_all_values('HitFinderToolVec.CandidateHitsPlane2.RoiThreshold', text)

    mph = get_all_values('HitFilterAlg.MinPulseHeight', text)
    mps = get_all_values('HitFilterAlg.MinPulseSigma', text)
    lmh = get_all_values('LongMaxHits', text)
    lpw = get_all_values('LongPulseWidth', text)
    phc = get_all_values('PulseHeightCuts', text)
    pwc = get_all_values('PulseWidthCuts', text)
    prc = get_all_values('PulseRatioCuts', text)
    mmh = get_all_values('MaxMultiHit', text)
    chi = get_all_values('Chi2NDF', text)

    roiThreshold = _ensure_list3([
        (rt0[-1] if rt0 else 5.0),
        (rt1[-1] if rt1 else 5.0),
        (rt2[-1] if rt2 else 5.0),
    ])

    minPulseHeight = _ensure_list3(mph[-1] if mph else 2.0)
    minPulseSigma  = _ensure_list3(mps[-1] if mps else 1.0)
    LongMaxHits    = _ensure_list3(lmh[-1] if lmh else 1)
    LongPulseWidth = _ensure_list3(lpw[-1] if lpw else 10.0)
    PulseHeightCuts= _ensure_list3(phc[-1] if phc else 3)
    PulseWidthCuts = _ensure_list3(pwc[-1] if pwc else 2)
    PulseRatioCuts = _ensure_list3(prc[-1] if prc else 0.35)
    MaxMultiHit    = mmh[-1] if mmh else 5
    Chi2NDF        = chi[-1] if chi else 500.0

    return fclParams(
        roiThreshold=roiThreshold,
        minPulseHeight=minPulseHeight,
        minPulseSigma=minPulseSigma,
        LongMaxHits=LongMaxHits,
        LongPulseWidth=LongPulseWidth,
        PulseHeightCuts=PulseHeightCuts,
        PulseWidthCuts=PulseWidthCuts,
        PulseRatioCuts=PulseRatioCuts,
        MaxMultiHit=MaxMultiHit,
        Chi2NDF=Chi2NDF,
    )

class HitTuningDB:
    """Database manager for hit tuning parameter scans and results."""
    
    def __init__(self, db_path: str = "hitTuning.db") -> None:
        """Initialize database connection and create tables if needed.
        
        Args:
            db_path: Path to SQLite database file
        """
        self.db_path: str = db_path
        self.conn: sqlite3.Connection = sqlite3.connect(db_path)
        self.create_tables()
    
    def create_tables(self) -> None:
        """Create database tables for storing run parameters and results."""
        cursor = self.conn.cursor()
        
        # Create table for tracking runs
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS runs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                jobNum INTEGER NOT NULL,
                timestamp TEXT NOT NULL,
                fcl_filename TEXT NOT NULL,
                output_filename TEXT,
                hist_filename TEXT,
                roiThreshold_0 REAL,
                roiThreshold_1 REAL,
                roiThreshold_2 REAL,
                minPulseHeight_0 REAL,
                minPulseHeight_1 REAL,
                minPulseHeight_2 REAL,
                minPulseSigma_0 REAL,
                minPulseSigma_1 REAL,
                minPulseSigma_2 REAL,
                LongMaxHits_0 INTEGER,
                LongMaxHits_1 INTEGER,
                LongMaxHits_2 INTEGER,
                LongPulseWidth_0 REAL,
                LongPulseWidth_1 REAL,
                LongPulseWidth_2 REAL,
                PulseHeightCuts_0 REAL,
                PulseHeightCuts_1 REAL,
                PulseHeightCuts_2 REAL,
                PulseWidthCuts_0 REAL,
                PulseWidthCuts_1 REAL,
                PulseWidthCuts_2 REAL,
                PulseRatioCuts_0 REAL,
                PulseRatioCuts_1 REAL,
                PulseRatioCuts_2 REAL,
                MaxMultiHit INTEGER,
                Chi2NDF REAL,
                notes TEXT,
                ratio_total REAL,
                ratio_total0 REAL,
                ratio_total1 REAL,
                ratio_total2 REAL,
                ratio_ele REAL,
                ratio_ele0 REAL,
                ratio_ele1 REAL,
                ratio_ele2 REAL,
                ratio_gamma REAL,
                ratio_gamma0 REAL,
                ratio_gamma1 REAL,
                ratio_gamma2 REAL,
                ratio_mu REAL,
                ratio_mu0 REAL,
                ratio_mu1 REAL,
                ratio_mu2 REAL,
                ratio_p REAL,
                ratio_p0 REAL,
                ratio_p1 REAL,
                ratio_p2 REAL,
                ratio_pi REAL,
                ratio_pi0 REAL,
                ratio_pi1 REAL,
                ratio_pi2 REAL
            )
        ''')
        
        self.conn.commit()
    
    def add_run(self, params: fclParams, jobNum: int, fcl_filename: str, 
                output_filename: Optional[str] = None, hist_filename: Optional[str] = None, 
                notes: Optional[str] = None) -> int:
        """Add a new run to the database.
        
        Args:
            params: FCL parameters for this run
            jobNum: Job number identifier
            fcl_filename: Path to FCL configuration file
            output_filename: Path to output ROOT file (optional)
            hist_filename: Path to histogram ROOT file (optional)
            notes: Additional notes about this run (optional)
            
        Returns:
            Database ID of the inserted run
        """
        cursor = self.conn.cursor()
        
        timestamp = datetime.now().isoformat()
        
        cursor.execute('''
            INSERT INTO runs (
                jobNum, timestamp, fcl_filename, output_filename, hist_filename,
                roiThreshold_0, roiThreshold_1, roiThreshold_2,
                minPulseHeight_0, minPulseHeight_1, minPulseHeight_2,
                minPulseSigma_0, minPulseSigma_1, minPulseSigma_2,
                LongMaxHits_0, LongMaxHits_1, LongMaxHits_2,
                LongPulseWidth_0, LongPulseWidth_1, LongPulseWidth_2,
                PulseHeightCuts_0, PulseHeightCuts_1, PulseHeightCuts_2,
                PulseWidthCuts_0, PulseWidthCuts_1, PulseWidthCuts_2,
                PulseRatioCuts_0, PulseRatioCuts_1, PulseRatioCuts_2,
                MaxMultiHit, Chi2NDF, notes, 
                ratio_total, ratio_total0, ratio_total1, ratio_total2,
                ratio_ele, ratio_ele0, ratio_ele1, ratio_ele2, 
                ratio_gamma, ratio_gamma0, ratio_gamma1, ratio_gamma2, 
                ratio_mu, ratio_mu0, ratio_mu1, ratio_mu2, 
                ratio_p, ratio_p0, ratio_p1, ratio_p2, 
                ratio_pi, ratio_pi0, ratio_pi1, ratio_pi2
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
            ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (
            jobNum, timestamp, fcl_filename, output_filename, hist_filename,
            params.roiThreshold[0], params.roiThreshold[1], params.roiThreshold[2],
            params.minPulseHeight[0], params.minPulseHeight[1], params.minPulseHeight[2],
            params.minPulseSigma[0], params.minPulseSigma[1], params.minPulseSigma[2],
            params.LongMaxHits[0], params.LongMaxHits[1], params.LongMaxHits[2],
            params.LongPulseWidth[0], params.LongPulseWidth[1], params.LongPulseWidth[2],
            params.PulseHeightCuts[0], params.PulseHeightCuts[1], params.PulseHeightCuts[2],
            params.PulseWidthCuts[0], params.PulseWidthCuts[1], params.PulseWidthCuts[2],
            params.PulseRatioCuts[0], params.PulseRatioCuts[1], params.PulseRatioCuts[2],
            params.MaxMultiHit, params.Chi2NDF, notes, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
        ))
        
        self.conn.commit()
        return cursor.lastrowid
    
    def get_run(self, run_id: int) -> Optional[Tuple]:
        """Retrieve a single run by ID.
        
        Args:
            run_id: Database ID of the run
            
        Returns:
            Tuple containing run data, or None if not found
        """
        cursor = self.conn.cursor()
        cursor.execute('SELECT * FROM runs WHERE id = ?', (run_id,))
        return cursor.fetchone()
    
    def get_all_runs(self) -> List[Tuple]:
        """Retrieve all runs from database, ordered by timestamp.
        
        Returns:
            List of tuples containing run data
        """
        cursor = self.conn.cursor()
        cursor.execute('SELECT * FROM runs ORDER BY timestamp DESC')
        return cursor.fetchall()
    
    def search_runs(self, **kwargs: Any) -> List[Tuple]:
        """Search runs by parameter values.
        
        Args:
            **kwargs: Key-value pairs to filter runs
            
        Returns:
            List of tuples containing matching run data
        """
        cursor = self.conn.cursor()
        
        query = 'SELECT * FROM runs WHERE 1=1'
        params = []
        
        for key, value in kwargs.items():
            query += f' AND {key} = ?'
            params.append(value)
        
        cursor.execute(query, params)
        return cursor.fetchall()
    
    def update_output_filename(self, run_id: int, output_filename: str) -> None:
        """Update the output filename for a run.
        
        Args:
            run_id: Database ID of the run
            output_filename: Path to output ROOT file
        """
        cursor = self.conn.cursor()
        cursor.execute('UPDATE runs SET output_filename = ? WHERE id = ?', 
                      (output_filename, run_id))
        self.conn.commit()
    
    def update_hist_filename(self, run_id: int, hist_filename: str) -> None:
        """Update the histogram filename for a run.
        
        Args:
            run_id: Database ID of the run
            hist_filename: Path to histogram ROOT file
        """
        cursor = self.conn.cursor()
        cursor.execute('UPDATE runs SET hist_filename = ? WHERE id = ?', 
                      (hist_filename, run_id))
        self.conn.commit()
    
    def update_results(self, run_id: int, results: List[List[float]]) -> None:
        """Update the run with results from galleryMC.
        
        Args:
            run_id: The database run ID
            results: List of lists containing ratio results for different particle types
                    Format: [[ratio_total, ...], [ratio_ele, ...], [ratio_gamma, ...], 
                            [ratio_mu, ...], [ratio_p, ...], [ratio_pi, ...]]
        """
        cursor = self.conn.cursor()
        cursor.execute('''UPDATE runs SET 
                            ratio_total = ?, 
                            ratio_total0 = ?,
                            ratio_total1 = ?,
                            ratio_total2 = ?,
                            ratio_ele = ?, 
                            ratio_ele0 = ?,
                            ratio_ele1 = ?,
                            ratio_ele2 = ?,
                            ratio_gamma = ?,
                            ratio_gamma0 = ?,
                            ratio_gamma1 = ?,
                            ratio_gamma2 = ?, 
                            ratio_mu = ?, 
                            ratio_mu0 = ?,
                            ratio_mu1 = ?,
                            ratio_mu2 = ?,
                            ratio_p = ?, 
                            ratio_p0 = ?,
                            ratio_p1 = ?,
                            ratio_p2 = ?,
                            ratio_pi = ?, 
                            ratio_pi0 = ?,
                            ratio_pi1 = ?,
                            ratio_pi2 = ?
                            WHERE id = ?''', 
                      (float(results[0][0]), float(results[0][1]), float(results[0][2]), float(results[0][3]),
                        float(results[1][0]), float(results[1][1]), float(results[1][2]), float(results[1][3]),
                        float(results[2][0]), float(results[2][1]), float(results[2][2]), float(results[2][3]),
                        float(results[3][0]), float(results[3][1]), float(results[3][2]), float(results[3][3]),
                        float(results[4][0]), float(results[4][1]), float(results[4][2]), float(results[4][3]),
                        float(results[5][0]), float(results[5][1]), float(results[5][2]), float(results[5][3]),
                        run_id))
        self.conn.commit()
    
    def close(self) -> None:
        """Close the database connection."""
        self.conn.close()

def generateFCLMC(params: fclParams, outputFile: str = "hitTuningMC.fcl", verbose: bool = False) -> None:
    """Generate FCL configuration file for Monte Carlo data processing.
    
    Args:
        params: FCL parameters to use in configuration
        outputFile: Path to output FCL file
        verbose: Whether to print parameters to console
    """
    if verbose:
        print("Generating new FHICL file for MC with the following parameters:")
        print(params.__str__())

    fclStr=f'''

# This runs larcv as part of stage 1 processing for MC
#include "wirechannelroiconverters_sbn.fcl"

#include "stage1_run2_icarus_MC.fcl"

process_name: MCstage1p2

## Add the MC module to the list of producers
icarus_stage1_producers: {{
  channel2wire:                   @local::channelroitowire

  gaushit2dTPCWW:                 @local::gausshit_sbn
  gaushit2dTPCWE:                 @local::gausshit_sbn
  gaushit2dTPCEW:                 @local::gausshit_sbn
  gaushit2dTPCEE:                 @local::gausshit_sbn

  ### Cluster3D
  cluster3DCryoW:                 @local::icarus_cluster3d
  cluster3DCryoE:                 @local::icarus_cluster3d

  ### mc producers
  mcreco:                         @local::standard_mcreco
  mcassociationsGausCryoE:        @local::standard_mcparticlehitmatching
  mcassociationsGausCryoW:        @local::standard_mcparticlehitmatching
}}

# Lower thresholds for tighter filter width
icarus_stage1_producers.gaushit2dTPCEE.HitFinderToolVec.CandidateHitsPlane0.RoiThreshold:      {params.roiThreshold[0]}
icarus_stage1_producers.gaushit2dTPCEE.HitFinderToolVec.CandidateHitsPlane1.RoiThreshold:      {params.roiThreshold[1]}
icarus_stage1_producers.gaushit2dTPCEE.HitFinderToolVec.CandidateHitsPlane2.RoiThreshold:      {params.roiThreshold[2]}
icarus_stage1_producers.gaushit2dTPCEE.HitFilterAlg.MinPulseHeight:                            {params.minPulseHeight}
icarus_stage1_producers.gaushit2dTPCEE.HitFilterAlg.MinPulseSigma:                             {params.minPulseSigma}
icarus_stage1_producers.gaushit2dTPCEE.LongMaxHits:                                            {params.LongMaxHits}
icarus_stage1_producers.gaushit2dTPCEE.LongPulseWidth:                                         {params.LongPulseWidth}
icarus_stage1_producers.gaushit2dTPCEE.PulseHeightCuts:                                        {params.PulseHeightCuts}
icarus_stage1_producers.gaushit2dTPCEE.PulseWidthCuts:                                         {params.PulseWidthCuts}
icarus_stage1_producers.gaushit2dTPCEE.PulseRatioCuts:                                         {params.PulseRatioCuts}
icarus_stage1_producers.gaushit2dTPCEE.MaxMultiHit:                                            {params.MaxMultiHit}
icarus_stage1_producers.gaushit2dTPCEE.Chi2NDF:                                                {params.Chi2NDF}

icarus_stage1_producers.gaushit2dTPCEW.HitFinderToolVec.CandidateHitsPlane0.RoiThreshold:      {params.roiThreshold[0]}
icarus_stage1_producers.gaushit2dTPCEW.HitFinderToolVec.CandidateHitsPlane1.RoiThreshold:      {params.roiThreshold[1]}
icarus_stage1_producers.gaushit2dTPCEW.HitFinderToolVec.CandidateHitsPlane2.RoiThreshold:      {params.roiThreshold[2]}
icarus_stage1_producers.gaushit2dTPCEW.HitFilterAlg.MinPulseHeight:                            {params.minPulseHeight}
icarus_stage1_producers.gaushit2dTPCEW.HitFilterAlg.MinPulseSigma:                             {params.minPulseSigma}
icarus_stage1_producers.gaushit2dTPCEW.LongMaxHits:                                            {params.LongMaxHits}
icarus_stage1_producers.gaushit2dTPCEW.LongPulseWidth:                                         {params.LongPulseWidth}
icarus_stage1_producers.gaushit2dTPCEW.PulseHeightCuts:                                        {params.PulseHeightCuts}
icarus_stage1_producers.gaushit2dTPCEW.PulseWidthCuts:                                         {params.PulseWidthCuts}
icarus_stage1_producers.gaushit2dTPCEW.PulseRatioCuts:                                         {params.PulseRatioCuts}
icarus_stage1_producers.gaushit2dTPCEW.MaxMultiHit:                                            {params.MaxMultiHit}
icarus_stage1_producers.gaushit2dTPCEW.Chi2NDF:                                                {params.Chi2NDF}

icarus_stage1_producers.gaushit2dTPCWE.HitFinderToolVec.CandidateHitsPlane0.RoiThreshold:      {params.roiThreshold[0]}
icarus_stage1_producers.gaushit2dTPCWE.HitFinderToolVec.CandidateHitsPlane1.RoiThreshold:      {params.roiThreshold[1]}
icarus_stage1_producers.gaushit2dTPCWE.HitFinderToolVec.CandidateHitsPlane2.RoiThreshold:      {params.roiThreshold[2]}
icarus_stage1_producers.gaushit2dTPCWE.HitFilterAlg.MinPulseHeight:                            {params.minPulseHeight}
icarus_stage1_producers.gaushit2dTPCWE.HitFilterAlg.MinPulseSigma:                             {params.minPulseSigma}
icarus_stage1_producers.gaushit2dTPCWE.LongMaxHits:                                            {params.LongMaxHits}
icarus_stage1_producers.gaushit2dTPCWE.LongPulseWidth:                                         {params.LongPulseWidth}
icarus_stage1_producers.gaushit2dTPCWE.PulseHeightCuts:                                        {params.PulseHeightCuts}
icarus_stage1_producers.gaushit2dTPCWE.PulseWidthCuts:                                         {params.PulseWidthCuts}
icarus_stage1_producers.gaushit2dTPCWE.PulseRatioCuts:                                         {params.PulseRatioCuts}
icarus_stage1_producers.gaushit2dTPCWE.MaxMultiHit:                                            {params.MaxMultiHit}
icarus_stage1_producers.gaushit2dTPCWE.Chi2NDF:                                                {params.Chi2NDF}

icarus_stage1_producers.gaushit2dTPCWW.HitFinderToolVec.CandidateHitsPlane0.RoiThreshold:      {params.roiThreshold[0]}
icarus_stage1_producers.gaushit2dTPCWW.HitFinderToolVec.CandidateHitsPlane1.RoiThreshold:      {params.roiThreshold[1]}
icarus_stage1_producers.gaushit2dTPCWW.HitFinderToolVec.CandidateHitsPlane2.RoiThreshold:      {params.roiThreshold[2]}
icarus_stage1_producers.gaushit2dTPCWW.HitFilterAlg.MinPulseHeight:                            {params.minPulseHeight}
icarus_stage1_producers.gaushit2dTPCWW.HitFilterAlg.MinPulseSigma:                             {params.minPulseSigma}
icarus_stage1_producers.gaushit2dTPCWW.LongMaxHits:                                            {params.LongMaxHits}
icarus_stage1_producers.gaushit2dTPCWW.LongPulseWidth:                                         {params.LongPulseWidth}
icarus_stage1_producers.gaushit2dTPCWW.PulseHeightCuts:                                        {params.PulseHeightCuts}
icarus_stage1_producers.gaushit2dTPCWW.PulseWidthCuts:                                         {params.PulseWidthCuts}
icarus_stage1_producers.gaushit2dTPCWW.PulseRatioCuts:                                         {params.PulseRatioCuts}
icarus_stage1_producers.gaushit2dTPCWW.MaxMultiHit:                                            {params.MaxMultiHit}
icarus_stage1_producers.gaushit2dTPCWW.Chi2NDF:                                                {params.Chi2NDF}

icarus_stage1_producers.gaushit2dTPCWW.CalDataModuleLabel:                                     "wire2channelroi2d:PHYSCRATEDATATPCWW"
icarus_stage1_producers.gaushit2dTPCWE.CalDataModuleLabel:                                     "wire2channelroi2d:PHYSCRATEDATATPCWE"
icarus_stage1_producers.gaushit2dTPCEW.CalDataModuleLabel:                                     "wire2channelroi2d:PHYSCRATEDATATPCEW"
icarus_stage1_producers.gaushit2dTPCEE.CalDataModuleLabel:                                     "wire2channelroi2d:PHYSCRATEDATATPCEE"

icarus_stage1_producers.caloskimCalorimetryCryoE.TrackModuleLabel:                               "pandoraTrackGausCryoE"
icarus_stage1_producers.caloskimCalorimetryCryoW.TrackModuleLabel:                               "pandoraTrackGausCryoW"

## Definitions for running the 3D clustering by Cryostat
icarus_stage1_producers.cluster3DCryoW.MakeSpacePointsOnly:                                      true
# use the "PT" (pulse train) hits as input to cluster3D
icarus_stage1_producers.cluster3DCryoW.Hit3DBuilderAlg.HitFinderTagVec:                          ["gaushitPT2dTPCWW", "gaushitPT2dTPCWE"]
icarus_stage1_producers.cluster3DCryoW.Hit3DBuilderAlg.PulseHeightFraction:                      0. #0.75 #0.25
icarus_stage1_producers.cluster3DCryoW.Hit3DBuilderAlg.PHLowSelection:                           0. #4.0 # 20.
icarus_stage1_producers.cluster3DCryoW.Hit3DBuilderAlg.MaxHitChiSquare:                          1000000.
icarus_stage1_producers.cluster3DCryoW.Hit3DBuilderAlg.MaxMythicalChiSquare:                     30.
icarus_stage1_producers.cluster3DCryoW.Hit3DBuilderAlg.OutputHistograms:                         false

icarus_stage1_producers.cluster3DCryoE.MakeSpacePointsOnly:                                      true
icarus_stage1_producers.cluster3DCryoE.Hit3DBuilderAlg.HitFinderTagVec:                          ["gaushitPT2dTPCEW", "gaushitPT2dTPCEE"]
icarus_stage1_producers.cluster3DCryoE.Hit3DBuilderAlg.PulseHeightFraction:                      0. #0.75 #0.25
icarus_stage1_producers.cluster3DCryoE.Hit3DBuilderAlg.PHLowSelection:                           0. #4.0 # 20.
icarus_stage1_producers.cluster3DCryoE.Hit3DBuilderAlg.MaxHitChiSquare:                          1000000.
icarus_stage1_producers.cluster3DCryoE.Hit3DBuilderAlg.MaxMythicalChiSquare:                     30.
icarus_stage1_producers.cluster3DCryoE.Hit3DBuilderAlg.OutputHistograms:                         false


physics.producers:
{{
    @table::icarus_stage1_producers
}}

physics.producers.channel2wire.WireModuleLabelVec: ["wire2channelroi2d:PHYSCRATEDATATPCEE", "wire2channelroi2d:PHYSCRATEDATATPCEW", "wire2channelroi2d:PHYSCRATEDATATPCWE", "wire2channelroi2d:PHYSCRATEDATATPCWW"]
physics.producers.channel2wire.OutInstanceLabelVec: ["PHYSCRATEDATATPCEE", "PHYSCRATEDATATPCEW", "PHYSCRATEDATATPCWE", "PHYSCRATEDATATPCWW"]

physics.producers.mcassociationsGausCryoE.HitParticleAssociations.HitModuleLabelVec: ["gaushit2dTPCEE", "gaushit2dTPCEW", "gaushit2dTPCWE", "gaushit2dTPCWW"]
physics.producers.mcassociationsGausCryoW.HitParticleAssociations.HitModuleLabelVec: ["gaushit2dTPCEE", "gaushit2dTPCEW", "gaushit2dTPCWE", "gaushit2dTPCWW"]

#physics.producers.gaushit2dTPCEE.module_type: GausHitFinder
#physics.producers.gaushit2dTPCEW.module_type: GausHitFinder
#physics.producers.gaushit2dTPCWE.module_type: GausHitFinder
#physics.producers.gaushit2dTPCWW.module_type: GausHitFinder

physics.producers.mcreco.G4ModName: @erase
physics.producers.mcreco.SimChannelLabel: "filtersed"
physics.producers.mcreco.MCParticleLabel: "largeant"
physics.producers.mcreco.UseSimEnergyDepositLite: true
physics.producers.mcreco.UseSimEnergyDeposit: false
physics.producers.mcreco.IncludeDroppedParticles: true #this is now true with larsoft v09_89 and newer
physics.producers.mcreco.MCParticleDroppedLabel: "largeant:droppedMCParticles"
physics.producers.mcreco.MCRecoPart.SavePathPDGList: [13,-13,211,-211,111,311,310,130,321,-321,2212,2112,2224,2214,2114,1114,3122,1000010020,1000010030,1000020030,1000020040]
physics.producers.mcreco.MCRecoPart.TrackIDOffsets: [0,10000000,20000000] #Account for track ID offsets in labeling primaries

physics.producers.mcassociationsGausCryoE.OverrideRealData: true
physics.producers.mcassociationsGausCryoW.OverrideRealData: true
services.BackTrackerService.BackTracker.OverrideRealData: true
services.ParticleInventoryService.ParticleInventory.OverrideRealData: true

physics.filters:
{{}}

physics.analyzers:
{{}}

physics.reco: [
                channel2wire,
                gaushit2dTPCEW,
                gaushit2dTPCEE,
                gaushit2dTPCWW,
                gaushit2dTPCWE,
                mcassociationsGausCryoE,  
                mcassociationsGausCryoW,
                mcreco
            ]

#physics.outana:    [ @sequence::icarus_analysis_modules, @sequence::icarus_analysis_superaMC]
physics.outana:    [ ]

physics.end_paths: [ outana, stream1 ]'''

    with open(outputFile, 'w') as f:
        f.write(fclStr)

def generateFCL(params: fclParams, outputFile: str = "hitTuning.fcl", verbose: bool = False) -> None:
    """Generate FCL configuration file for real data processing.
    
    Args:
        params: FCL parameters to use in configuration
        outputFile: Path to output FCL file
        verbose: Whether to print parameters to console
    """
    if verbose:
        print("Generating new FHICL file with the following parameters:")
        print(params.__str__())

    fclStr=f'''
# This includes running larcv as part of stage 1 processing
#include "services_common_icarus.fcl"
#include "wirechannelroiconverters_sbn.fcl"
#include "stage1_run2_icarus.fcl"

services:{{
    @table::icarus_wirecalibration_services
}}

icarus_stage1_producers:
{{  
### TPC hit-finder producers
gaushit2dTPCWW:                 @local::gausshit_sbn
gaushit2dTPCWE:                 @local::gausshit_sbn
gaushit2dTPCEW:                 @local::gausshit_sbn
gaushit2dTPCEE:                 @local::gausshit_sbn
}}

icarus_stage1_analyzers:
{{}}

icarus_stage1_filters:
{{}}

icarus_analysis_modules:
{{}}

# Lower thresholds for tighter filter width
icarus_stage1_producers.gaushit2dTPCEE.HitFinderToolVec.CandidateHitsPlane0.RoiThreshold:      {params.roiThreshold[0]}
icarus_stage1_producers.gaushit2dTPCEE.HitFinderToolVec.CandidateHitsPlane1.RoiThreshold:      {params.roiThreshold[1]}
icarus_stage1_producers.gaushit2dTPCEE.HitFinderToolVec.CandidateHitsPlane2.RoiThreshold:      {params.roiThreshold[2]}
icarus_stage1_producers.gaushit2dTPCEE.HitFilterAlg.MinPulseHeight:                            {params.minPulseHeight}
icarus_stage1_producers.gaushit2dTPCEE.HitFilterAlg.MinPulseSigma:                             {params.minPulseSigma}
icarus_stage1_producers.gaushit2dTPCEE.LongMaxHits:                                            {params.LongMaxHits}
icarus_stage1_producers.gaushit2dTPCEE.LongPulseWidth:                                         {params.LongPulseWidth}
icarus_stage1_producers.gaushit2dTPCEE.PulseHeightCuts:                                        {params.PulseHeightCuts}
icarus_stage1_producers.gaushit2dTPCEE.PulseWidthCuts:                                         {params.PulseWidthCuts}
icarus_stage1_producers.gaushit2dTPCEE.PulseRatioCuts:                                         {params.PulseRatioCuts}
icarus_stage1_producers.gaushit2dTPCEE.MaxMultiHit:                                            {params.MaxMultiHit}
icarus_stage1_producers.gaushit2dTPCEE.Chi2NDF:                                                {params.Chi2NDF}

icarus_stage1_producers.gaushit2dTPCEW.HitFinderToolVec.CandidateHitsPlane0.RoiThreshold:      {params.roiThreshold[0]}
icarus_stage1_producers.gaushit2dTPCEW.HitFinderToolVec.CandidateHitsPlane1.RoiThreshold:      {params.roiThreshold[1]}
icarus_stage1_producers.gaushit2dTPCEW.HitFinderToolVec.CandidateHitsPlane2.RoiThreshold:      {params.roiThreshold[2]}
icarus_stage1_producers.gaushit2dTPCEW.HitFilterAlg.MinPulseHeight:                            {params.minPulseHeight}
icarus_stage1_producers.gaushit2dTPCEW.HitFilterAlg.MinPulseSigma:                             {params.minPulseSigma}
icarus_stage1_producers.gaushit2dTPCEW.LongMaxHits:                                            {params.LongMaxHits}
icarus_stage1_producers.gaushit2dTPCEW.LongPulseWidth:                                         {params.LongPulseWidth}
icarus_stage1_producers.gaushit2dTPCEW.PulseHeightCuts:                                        {params.PulseHeightCuts}
icarus_stage1_producers.gaushit2dTPCEW.PulseWidthCuts:                                         {params.PulseWidthCuts}
icarus_stage1_producers.gaushit2dTPCEW.PulseRatioCuts:                                         {params.PulseRatioCuts}
icarus_stage1_producers.gaushit2dTPCEW.MaxMultiHit:                                            {params.MaxMultiHit}
icarus_stage1_producers.gaushit2dTPCEW.Chi2NDF:                                                {params.Chi2NDF}

icarus_stage1_producers.gaushit2dTPCWE.HitFinderToolVec.CandidateHitsPlane0.RoiThreshold:      {params.roiThreshold[0]}
icarus_stage1_producers.gaushit2dTPCWE.HitFinderToolVec.CandidateHitsPlane1.RoiThreshold:      {params.roiThreshold[1]}
icarus_stage1_producers.gaushit2dTPCWE.HitFinderToolVec.CandidateHitsPlane2.RoiThreshold:      {params.roiThreshold[2]}
icarus_stage1_producers.gaushit2dTPCWE.HitFilterAlg.MinPulseHeight:                            {params.minPulseHeight}
icarus_stage1_producers.gaushit2dTPCWE.HitFilterAlg.MinPulseSigma:                             {params.minPulseSigma}
icarus_stage1_producers.gaushit2dTPCWE.LongMaxHits:                                            {params.LongMaxHits}
icarus_stage1_producers.gaushit2dTPCWE.LongPulseWidth:                                         {params.LongPulseWidth}
icarus_stage1_producers.gaushit2dTPCWE.PulseHeightCuts:                                        {params.PulseHeightCuts}
icarus_stage1_producers.gaushit2dTPCWE.PulseWidthCuts:                                         {params.PulseWidthCuts}
icarus_stage1_producers.gaushit2dTPCWE.PulseRatioCuts:                                         {params.PulseRatioCuts}
icarus_stage1_producers.gaushit2dTPCWE.MaxMultiHit:                                            {params.MaxMultiHit}
icarus_stage1_producers.gaushit2dTPCWE.Chi2NDF:                                                {params.Chi2NDF}

icarus_stage1_producers.gaushit2dTPCWW.HitFinderToolVec.CandidateHitsPlane0.RoiThreshold:      {params.roiThreshold[0]}
icarus_stage1_producers.gaushit2dTPCWW.HitFinderToolVec.CandidateHitsPlane1.RoiThreshold:      {params.roiThreshold[1]}
icarus_stage1_producers.gaushit2dTPCWW.HitFinderToolVec.CandidateHitsPlane2.RoiThreshold:      {params.roiThreshold[2]}
icarus_stage1_producers.gaushit2dTPCWW.HitFilterAlg.MinPulseHeight:                            {params.minPulseHeight}
icarus_stage1_producers.gaushit2dTPCWW.HitFilterAlg.MinPulseSigma:                             {params.minPulseSigma}
icarus_stage1_producers.gaushit2dTPCWW.LongMaxHits:                                            {params.LongMaxHits}
icarus_stage1_producers.gaushit2dTPCWW.LongPulseWidth:                                         {params.LongPulseWidth}
icarus_stage1_producers.gaushit2dTPCWW.PulseHeightCuts:                                        {params.PulseHeightCuts}
icarus_stage1_producers.gaushit2dTPCWW.PulseWidthCuts:                                         {params.PulseWidthCuts}
icarus_stage1_producers.gaushit2dTPCWW.PulseRatioCuts:                                         {params.PulseRatioCuts}
icarus_stage1_producers.gaushit2dTPCWW.MaxMultiHit:                                            {params.MaxMultiHit}
icarus_stage1_producers.gaushit2dTPCWW.Chi2NDF:                                                {params.Chi2NDF}

physics.producers:
{{
    rns: {{module_type: RandomNumberSaver }}
    @table::icarus_stage1_producers
}}

physics.filters:
{{
    @table::icarus_stage1_filters
}}

physics.analyzers:
{{
    @table::icarus_stage1_analyzers
}}

physics.producers.channel2wire.WireModuleLabelVec: ["wire2channelroi2d:PHYSCRATEDATATPCEE", "wire2channelroi2d:PHYSCRATEDATATPCEW", "wire2channelroi2d:PHYSCRATEDATATPCWE", "wire2channelroi2d:PHYSCRATEDATATPCWW"]
physics.producers.channel2wire.OutInstanceLabelVec: ["PHYSCRATEDATATPCEE", "PHYSCRATEDATATPCEW", "PHYSCRATEDATATPCWE", "PHYSCRATEDATATPCWW"]

physics.producers.gaushit2dTPCWW.CalDataModuleLabel:                                     "wire2channelroi2d:PHYSCRATEDATATPCWW"
physics.producers.gaushit2dTPCWE.CalDataModuleLabel:                                     "wire2channelroi2d:PHYSCRATEDATATPCWE"
physics.producers.gaushit2dTPCEW.CalDataModuleLabel:                                     "wire2channelroi2d:PHYSCRATEDATATPCEW"
physics.producers.gaushit2dTPCEE.CalDataModuleLabel:                                     "wire2channelroi2d:PHYSCRATEDATATPCEE"

physics.reco: [
                gaushit2dTPCEW,
                gaushit2dTPCEE,
                gaushit2dTPCWW,
                gaushit2dTPCWE
            ]

physics.outana:    [ ]

physics.end_paths: [ outana, stream1 ]
    '''

    with open(outputFile, 'w') as f:
        f.write(fclStr)

def run(fclFile: str, inputFile: str, outputFile: str, options: Optional[str] = None) -> None:
    """Run LArSoft with specified FCL file and input.
    
    Args:
        fclFile: Path to FCL configuration file
        inputFile: Path to input ROOT file or file list
        outputFile: Path to output ROOT file
        options: Additional command-line options for lar command
    """
    if not inputFile.endswith('.root'):
        cmd = f"lar -c {fclFile} --source-list {inputFile} -o {outputFile}"
    else:
        cmd = f"lar -c {fclFile} -s {inputFile} -o {outputFile}"
    if options != None:
        print("Adding options:", options)
        cmd += ' ' + options
    else:
        print("run on all events")
        cmd += ' -n -1'
    print(f"Running command: {cmd}")
    os.system(cmd)

def reduceList(inputList: List[Union[int, float]]) -> Union[List[Union[int, float]], int, float]:
    """Reduce single-element list to scalar value.
    
    Args:
        inputList: List of values
        
    Returns:
        Single value if list has one element, otherwise the original list
    """
    if len(inputList) == 1:
        return inputList[0]
    return inputList

def createGrid(defaultFirst: bool = True) -> List[fclParams]:
    """Create parameter grid for systematic hit tuning scan.
    
    Args:
        defaultFirst: Whether to insert default parameters at the start of grid
        
    Returns:
        List of fclParams objects representing parameter combinations
    """
    print("Creating parameter grid for hit tuning...")

    v_roiThreshold = [[6.0], [5.0], [4.0], [3.0], [2.0], [1.0]]  
    v_minPulseHeight = [[2.0]]
    v_minPulseSigma = [[1.0]]
    v_LongMaxHits = [[1], [5], [10], [15]]
    v_LongPulseWidth = [[2.0], [5.0], [8.0]]
    v_PulseHeightCuts = [[2], [3]]
    v_PulseWidthCuts = [[2, 1.5, 1]]  
    v_PulseRatioCuts = [[3.5e-1, 4e-1, 2e-1]]
    v_MaxMultiHit = [[5], [7], [10], [12]]
    v_Chi2NDF = [[500.0], [1000.0], [1500.0], [2000.0], [2500.0]]

    # Generate all combinations
    param_grid = []
    for combo in product(v_roiThreshold, v_minPulseHeight, v_minPulseSigma, 
                        v_LongMaxHits, v_LongPulseWidth, v_PulseHeightCuts,
                        v_PulseWidthCuts, v_PulseRatioCuts, v_MaxMultiHit, v_Chi2NDF):
        
        print(combo[0], combo[1], combo[2], combo[3], combo[4], combo[5], combo[6], combo[7], combo[8], combo[9])
        params = fclParams(
            roiThreshold=reduceList(combo[0]),
            minPulseHeight=reduceList(combo[1]),
            minPulseSigma=reduceList(combo[2]),
            LongMaxHits=reduceList(combo[3]),
            LongPulseWidth=reduceList(combo[4]),
            PulseHeightCuts=reduceList(combo[5]),
            PulseWidthCuts=reduceList(combo[6]),
            PulseRatioCuts=reduceList(combo[7]),
            MaxMultiHit=reduceList(combo[8]),
            Chi2NDF=reduceList(combo[9])
        )
        param_grid.append(params)
    
    if defaultFirst:
        param_grid.insert(0, fclParams())  # Default parameters at the start

    print(f"Created grid with {len(param_grid)} parameter combinations")
    return param_grid

def parse_args() -> argparse.Namespace:
    """Parse command-line arguments.
    
    Returns:
        Namespace containing parsed arguments
    """
    parser = argparse.ArgumentParser(description="Hit Tuning Parameter Scan")
    parser.add_argument('-c', '--createGrid', action='store_true', help='Create all fcl files for parameter grid')
    parser.add_argument('-o', '--outputDir', type=str, default='./fclFiles', help='Output directory')
    parser.add_argument('--mc', action='store_true', help='Run on MC data')
    parser.add_argument('-t', '--tag', type=str, default='test', help='Tag for output files')
    parser.add_argument('-d', '--debug', action='store_true', help='Enable debug mode')
    parser.add_argument('-v', '--verbose', action='store_true', help='Enable verbose output')
    parser.add_argument('-r', '--runGrid', action='store_true', help='Run over the entire parameter grid')
    parser.add_argument('-i', '--inputFile', type=str, default='', help='Input file to process')
    parser.add_argument('-f', '--fclFile', type=str, default='', help='FCL file to use when running over grid')
    parser.add_argument('-n', '--runNumber', type=int, default=0, help='Run number for the job when running over grid')
    parser.add_argument('-p', '--path', type=str, default=None, help='Path to macros when running over grid')
    return parser.parse_args()


if __name__ == "__main__":

    args = parse_args()

    MC = args.mc
    fileSubStr = args.tag

    #create the fcl files for a grid search
    if args.createGrid:
        paramGrid = createGrid()
        for ip, params in enumerate(paramGrid):
            if args.debug:
                if ip > 1: break
            if ip % 100 == 0:
                print(f"Creating FCL for parameter set {ip}/{len(paramGrid)}")
            outputFCL = f'{outputDir}/hitTuning_{fileSubStr}_{ip}.fcl'
            if MC:
                generateFCLMC(params, outputFile=outputFCL, verbose=args.verbose)
            else:   
                generateFCL(params, outputFile=outputFCL, verbose=args.verbose)
        exit(0)

    # Initialize database
    db = HitTuningDB(f"hitTuning_{fileSubStr}.db")

    # Define input and output files
    
    #inputFile = "shower_stage0.root" #event 1667667 run 9746
    #inputFile = 'root://fndcadoor.fnal.gov://sbn/data/sbn_fd/poms_production/mc/2025A_ICARUS_NuGraph2/NuMI_MC_FullTrainingSample/v10_06_00_04p02/stage1/N1056/2c/d9/standard_mc_all_stage1_icarus_86400274_3010-0635dca8-e260-4c8e-a2e9-bcd6c3ef5966.root'
    inputFile = 'output_run9963_evt28003_stage0.root' # Run 9963 Event 28003 nu e from Felipe
    
    if args.inputFile != '':
        inputFile = args.inputFile

    print(f"Processing input file: {inputFile}")

    # Run over the grid on the batch system
    if args.runGrid:
        
        if not args.outputDir.endswith('.root'):
            print("Error: When running over the grid, outputDir must be the name of a .root file")
            exit(1)
        if args.fclFile == '':
            print("Error: When running over the grid, must provide FCL file via --fclFile")
            exit(1)
        if args.runNumber is None:
            print("Error: When running over the grid, must provide run number via --runNumber")
            exit(1)
        if args.path is None:
            print("Error: When running over the grid, must provide macro path via --path")
            exit(1)

        outputFile = args.outputDir
        fclFile = args.fclFile
        jobNum = args.runNumber
        macroPath = args.path

        # Load the macro (interpreted)
        abs_macro = os.path.join(macroPath, 'galleryMC.cpp')
        if not os.path.isabs(abs_macro):
            abs_macro = os.path.abspath(abs_macro)
        print(f"Attempting to load macro from: {abs_macro}")

        try:
            r.gSystem.AddDynamicPath(macroPath)
            r.gInterpreter.AddIncludePath(macroPath)
            r.gROOT.SetMacroPath(r.gROOT.GetMacroPath() + f":{macroPath}/")
        except Exception as e:
            print(f"Warning: failed to add macro paths: {e}")
        r.gInterpreter.ProcessLine('#include "gallery/Event.h"')
        r.gInterpreter.ProcessLine('#include "canvas/Persistency/Common/FindManyP.h"')
        r.gInterpreter.ProcessLine('#include "canvas/Utilities/InputTag.h"')
        if not os.path.exists(abs_macro):
            raise FileNotFoundError(
                f"galleryMC.cpp not found at {abs_macro}. "
                "Stage the macro with the job and pass the correct macroPath."
            )

        # Explicitly load via absolute path
        result = r.gROOT.ProcessLine(f'.L {abs_macro}+')    
        print(f"Compilation result: {result}")

        # Initialize database
        db = HitTuningDB(f"hitTuning_{jobNum}.db")

        # Add to database   
        params = parse_fcl_to_params(fclFile)
        run_id = db.add_run(params, jobNum, fclFile, notes="")
        print(f"Added run with ID: {run_id}")

        # Run lar with generated FCL
        run(fclFile, inputFile, outputFile, options='-n 5')
        
        # Later, update with output filename
        db.update_output_filename(run_id, outputFile)
        
        # Query runs
        all_runs = db.get_all_runs()
        print(f"Total runs in database: {len(all_runs)}")

        histFile = f'hist_output_{jobNum}.root'
        print(f"Processing hits with outputFile: {outputFile} and histFile: {histFile}")
        results = r.galleryMC(outputFile, histFile)
        print("results:", results)
        db.update_results(run_id, results)

        db.close()

        sys.exit(0)

    ## run interactively over specified parameter sets
    else:

        # Load the macro (interpreted)
        r.gInterpreter.ProcessLine('#include "gallery/Event.h"')
        r.gInterpreter.ProcessLine('#include "canvas/Persistency/Common/FindManyP.h"')
        r.gInterpreter.ProcessLine('#include "canvas/Utilities/InputTag.h"')
        if args.mc:
            result = r.gROOT.ProcessLine('.L galleryMC.cpp+')
        else:
            result = r.gROOT.ProcessLine('.L galleryMacro.cpp+')
        print(f"Compilation result: {result}")

        # Set output directory for fcl files
        outputDir = args.outputDir
        if not os.path.exists(outputDir):
            os.makedirs(outputDir)

        # Create fclParams class to hold fcl parameters
        defaultParams = fclParams( 
                                LongMaxHits=[3, 3, 3],
                                MaxMultiHit=10,
                                roiThreshold=[2.0, 2.0, 2.0],
                                LongPulseWidth=[5.0, 5.0, 5.0],
                                Chi2NDF=2500.0
                                ) #induction 1 tuned for ICARUS
        # defaultParams = fclParams( 
        #                 LongMaxHits=[2, 2, 2],
        #                 MaxMultiHit=10,
        #                 roiThreshold=[3.0, 3.0, 3.0],
        #                 LongPulseWidth=[5.0, 5.0, 5.0],
        #                 Chi2NDF=1750.0
        #                 ) #induction 2 tuned for ICARUS
        # defaultParams = fclParams( 
        #         LongMaxHits=[3, 3, 3],
        #         MaxMultiHit=8,
        #         roiThreshold=[6.0, 6.0, 6.0],
        #         LongPulseWidth=[5.0, 5.0, 5.0],
        #         Chi2NDF=2500
        #         ) #collection tuned for ICARUS
        # defaultParams = fclParams() #default
        # defaultParams = fclParams( 
        #         LongMaxHits=[10, 10, 10]) #turn on pulse trains
        paramGrid = [defaultParams]

    for ip, params in enumerate(paramGrid):

        # if ip > 5: 
        #     break
        try:
            version = 0
            outputFCL = f'{outputDir}/hitTuning_{fileSubStr}_{version}.fcl'
            outputFile = f'{outputDir}/output_{fileSubStr}_{version}.root'

            while os.path.exists(outputFCL):
                version += 1
                outputFCL = f'{outputDir}/hitTuning_{fileSubStr}_{version}.fcl'
                outputFile = f'{outputDir}/output_{fileSubStr}_{version}.root'

            # Generate FCL file
            if MC:
                generateFCLMC(params, outputFile=outputFCL)
            else:   
                generateFCL(params, outputFile=outputFCL)

            # Add to database   
            run_id = db.add_run(params, args.runNumber, outputFCL, notes="") 
            print(f"Added run with ID: {run_id}")

            if args.debug:
                options = '-n 2'  # Process only 1 events in debug mode
            else:
                options = None
            # Run lar with generated FCL
            run(outputFCL, inputFile, outputFile, options=options)
            
            # Later, update with output filename
            db.update_output_filename(run_id, outputFile)
            
            # Query runs
            all_runs = db.get_all_runs()
            print(f"Total runs in database: {len(all_runs)}")

            histFile = f'{outputDir}/hist_output_{fileSubStr}_{version}.root'

            if args.mc:
                results = r.galleryMC(outputFile, histFile)
                print("results:", results)
                db.update_results(run_id, results)
            else:
                r.galleryMacro(outputFile, histFile)
        
        except Exception as e:
            print(f"Error processing parameter set {ip}: {e}")
            continue
    
    db.close()


