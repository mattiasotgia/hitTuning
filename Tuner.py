# stdlib
import os
import re
import sqlite3
import subprocess
import tempfile

from dataclasses import dataclass
from datetime import datetime
from typing import Literal, List, Dict, Any

# ROOT/LArSoft specifics
import ROOT
from galleryUtils import loadConfiguration

# ============================================================
# 1. PARAMETER SPECIFICATION
# ============================================================

@dataclass
class ParamSpec:
    name: str
    low: float
    high: float
    kind: Literal['float', 'int']
    size: int = 1
    decimals: int = 2
    fcl_paths: List[str] = None
    is_per_plane: bool = False

    def sample(self, x: float):
        '''
        Sample a value given a normalized x in [0,1].
        For ints, ensures uniform discrete coverage instead of rounding.
        '''
        if self.kind == 'int':
            n_bins = int(self.high - self.low + 1)
            idx = int(x * n_bins)   # map x to integer bin
            idx = min(idx, n_bins - 1)  # handle x==1 edge
            val = self.low + idx
        else:
            val = self.low + x * (self.high - self.low)
            val = round(val, self.decimals)

        if self.size == 1:
            return val
        return [val] * self.size

# ============================================================
# 1. RESULT SPECIFICATION
# ============================================================

@dataclass
class ResultSpec:
    name: str
    dtype: Literal['REAL', 'INTEGER'] = 'REAL'   # or INTEGER

# ============================================================
# 2. PARAMETER SET
# ============================================================

class ParamSet:
    def __init__(self, values: Dict[str, Any]):
        self.values = values

    def to_flat_dict(self):
        flat = {}
        for k, v in self.values.items():
            if isinstance(v, list):
                for i, val in enumerate(v):
                    flat[f'{k}_{i}'] = val
            else:
                flat[k] = v
        return flat

    def __str__(self):
        lines = ["ParamSet:"]
        for k, v in self.values.items():
            if isinstance(v, list):
                vals = ", ".join(str(x) for x in v)
                lines.append(f"  {k}: [{vals}]")
            else:
                lines.append(f"  {k}: {v}")
        return "\n".join(lines)


# ============================================================
# 3. FCL MANAGER
# ============================================================

class FclManager:

    def __init__(self, template_path: str):
        with open(template_path) as f:
            self.template = f.read()

    def generate(self, params: ParamSet, specs: Dict[str, ParamSpec], output_path: str, tag: str):
        text = self.template

        text+='\n# ---\n# Custom configuration settings below (tag: {tag})\n# ---\n\n'.format(tag=tag)

        for name, value in params.values.items():
            spec = specs[name]

            for path in spec.fcl_paths:
                if spec.is_per_plane:
                    for i in range(spec.size):
                        full_path = path.format(plane=i)
                        text = self._set_value(text, full_path, value[i])
                else:
                    text = self._set_value(text, path, value)

        with open(output_path, 'w') as f:
            f.write(text)

    def _set_value(self, text, key, value):
        val_str = f'[{", ".join(map(str, value))}]' if isinstance(value, list) else str(value)
        pattern = rf'({re.escape(key)}\s*:\s*)([^\n#]+)'

        if re.search(pattern, text):
            return re.sub(pattern, rf'\1{val_str}', text)
        else:
            return text + f'{key}: {val_str}\n'

    def parse(self, fcl_path: str, specs: Dict[str, ParamSpec]) -> ParamSet:

        configuration = loadConfiguration(fcl_path)

        values = {}

        for name, spec in specs.items():
            if spec.is_per_plane:
                vals = []
                for i in range(spec.size):
                    full_path = spec.fcl_paths[0].format(plane=i)
                    if not configuration.has_key(full_path):
                        print(f'[FCL MANAGER WARNING CRITICAL] Missing key {name} value, this will be assigned a null value -1')
                        vals.append(-1)
                    else:
                        match = configuration.get[spec.kind](full_path)
                        # print(f'[FCL MANAGER DEBUG] For {name} found {match}')
                        vals.append(match)
                values[name] = vals
            else:
                # is either a singleton (size==1) or a tuple
                if spec.size == 1:
                    if not configuration.has_key(spec.fcl_paths[0]):
                        print(f'[FCL MANAGER WARNING CRITICAL] Missing key {name} value, this will be assigned a null value -1')
                        values[name] = -1
                    else:
                        values[name] = configuration.get[spec.kind](spec.fcl_paths[0])
                        # print(f'[FCL MANAGER DEBUG] For {name} found {match}')
                else:
                    if not configuration.has_key(spec.fcl_paths[0]):
                        print(f'[FCL MANAGER WARNING CRITICAL] Missing key {name} value, this will be assigned a null value -1')
                        values[name] = [-1] * spec.size
                    else:
                        values[name] = list(configuration.get[f'std::vector<{spec.kind}>'](spec.fcl_paths[0]))
                        # print(f'[FCL MANAGER DEBUG] For {name} found {match}')

        parameter_set = ParamSet(values)
        print(parameter_set)
        return parameter_set


# ============================================================
# 4. PARAM GRID (LHS)
# ============================================================

class ParamGrid:

    def __init__(self, specs: Dict[str, ParamSpec]):
        self.specs = specs

    def sample_lhs(self, n_samples=200, seed=42):
        from scipy.stats import qmc

        sampler = qmc.LatinHypercube(d=len(self.specs), seed=seed)
        raw = sampler.random(n_samples)

        keys = list(self.specs.keys())
        params = []

        for row in raw:
            values = {}
            for i, k in enumerate(keys):
                spec = self.specs[k]

                if spec.kind == 'int':
                    values[k] = spec.sample(row[i])  # now discrete-safe
                else:
                    values[k] = spec.sample(row[i])

            params.append(ParamSet(values))

        return params
    
    def __str__(self, param_sets: List[ParamSet] = None):
        lines = ["Parameter Grid:"]
        for name, spec in self.specs.items():
            line = f" - {name} ({spec.kind}, size={spec.size}, range=[{spec.low}, {spec.high}])"

            if param_sets:
                # collect all values for this parameter
                all_vals = []
                for ps in param_sets:
                    val = ps.values[name]
                    if isinstance(val, list):
                        all_vals.extend(val)
                    else:
                        all_vals.append(val)
                unique_vals = sorted(set(all_vals))
                line += f", unique values: {unique_vals}"

            lines.append(line)
        return "\n".join(lines)


# ============================================================
# 5. DATABASE
# ============================================================

class RunDB:

    def __init__(self, db_path: str, specs: Dict[str, ParamSpec], result_specs: List[ResultSpec]):
        self.conn = sqlite3.connect(db_path)
        self.specs = specs
        self.result_specs = result_specs
        self._create_table()

    def _create_table(self):
        cursor = self.conn.cursor()

        columns = [
            "id INTEGER PRIMARY KEY AUTOINCREMENT",
            "jobNum INTEGER",
            "timestamp TEXT",
            "fcl_filename TEXT",
            "output_filename TEXT",
            "hist_filename TEXT",
            "notes TEXT"
        ]

        # -------------------------
        # Parameters
        # -------------------------
        for name, spec in self.specs.items():
            dtype = "INTEGER" if spec.kind == "int" else "REAL"

            if spec.size == 1:
                columns.append(f"{name} {dtype}")
            else:
                for i in range(spec.size):
                    columns.append(f"{name}_{i} {dtype}")

        # -------------------------
        # Results (NEW)
        # -------------------------
        for rs in self.result_specs:
            columns.append(f"{rs.name} {rs.dtype}")

        cursor.execute(f"CREATE TABLE IF NOT EXISTS runs ({', '.join(columns)})")
        self.conn.commit()

    def insert(self, params: ParamSet, meta: dict, results: dict):
        cursor = self.conn.cursor()

        data = {**meta, **params.to_flat_dict(), **results}
        keys = ', '.join(data.keys())
        placeholders = ', '.join(['?'] * len(data))

        cursor.execute(
            f'INSERT INTO runs ({keys}) VALUES ({placeholders})',
            tuple(data.values())
        )

    def commit(self):
        self.conn.commit()

    def close(self):
        self.conn.close()


# ============================================================
# 6. RUN LAR
# ============================================================

def run_lar(fclFile, inputFile, outputFile):
    if inputFile.endswith('.root'):
        cmd = ['lar', '-c', fclFile, '-s', inputFile, '-o', outputFile, '-n', '-1']
    else:
        cmd = ['lar', '-c', fclFile, '--source-list', inputFile, '-o', outputFile, '-n', '-1']

    print('[RUN]', ' '.join(cmd))

    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    print(result.stdout)

    if result.returncode != 0:
        raise RuntimeError(f'lar failed with code {result.returncode}')

# ============================================================
# 6 1/2. UTILITIES
# ============================================================

def ensure_dir(path):
    if path and not os.path.exists(path):
        os.makedirs(path, exist_ok=True)

def chunk_list(lst, chunk_size):
    for i in range(0, len(lst), chunk_size):
        yield lst[i:i + chunk_size]

def normalize_path(path):
    if path is None:
        return None
    return os.path.abspath(os.path.expanduser(path))

def is_root_file(path: str) -> bool:
    return path.endswith('.root')


def read_file_list(path: str):
    with open(path) as f:
        return [line.strip() for line in f if line.strip()]

def batch_output_name(base: str, idx: int):
    if base.endswith('.root'):
        return base.replace('.root', f'_batch{idx}.root')
    return f'{base}_batch{idx}'

def load_root_macro(macro_path: str, macro_name: str = 'hitScorer.C'):
    '''Prepare ROOT environment and load macro.'''

    abs_macro = os.path.join(macro_path, macro_name)
    abs_macro = os.path.abspath(abs_macro)

    print(f'[ROOT] Loading macro from: {abs_macro}')

    # --------------------------------------------------
    # 1. Validate macro exists
    # --------------------------------------------------
    if not os.path.exists(abs_macro):
        raise FileNotFoundError(
            f'{macro_name} not found at {abs_macro}. '
            'Make sure it is staged with the job.'
        )

    # --------------------------------------------------
    # 2. Configure ROOT paths
    # --------------------------------------------------
    try:
        ROOT.gSystem.AddDynamicPath(macro_path)
        ROOT.gInterpreter.AddIncludePath(macro_path)

        current_macro_path = ROOT.gROOT.GetMacroPath()
        if macro_path not in current_macro_path:
            ROOT.gROOT.SetMacroPath(current_macro_path + f':{macro_path}')

    except Exception as e:
        print(f'[ROOT WARNING] Failed to configure paths: {e}')

    # --------------------------------------------------
    # 3. Load required headers (LArSoft)
    # --------------------------------------------------
    headers = [
        'gallery/Event.h',
        'canvas/Persistency/Common/FindManyP.h',
        'canvas/Utilities/InputTag.h'
    ]

    for header in headers:
        try:
            ROOT.gInterpreter.ProcessLine(f'#include "{header}"')
        except Exception as e:
            print(f'[ROOT WARNING] Failed to include {header}: {e}')

    # --------------------------------------------------
    # 4. Compile macro
    # --------------------------------------------------
    result = ROOT.gROOT.ProcessLine(f'.L {abs_macro}+')

    print(f'[ROOT] Macro compilation result: {result}')

    return abs_macro

def result_to_dict(result, result_specs):
    return {
        spec.name: result[i]
        for i, spec in enumerate(result_specs)
    }

# ============================================================
# 7. SPECS DEFINITION (YOUR PHYSICS HERE)
# ============================================================

def build_specs():

    producers = [
        'gaushit2dTPCEE',
        'gaushit2dTPCEW',
        'gaushit2dTPCWE',
        'gaushit2dTPCWW'
    ]

    def p(path):
        return [f'physics.producers.{prod}.{path}' for prod in producers]

    return {
        'roiThreshold': ParamSpec(
            'roiThreshold', 3, 7, 'float', size=3,
            is_per_plane=True,
            fcl_paths=p('HitFinderToolVec.CandidateHitsPlane{plane}.RoiThreshold')
        ),
        
        'nextADCThreshold': ParamSpec(
            'nextADCThreshold', -0.3, 0, 'float', size=3,
            is_per_plane=True,
            fcl_paths=p('HitFinderToolVec.CandidateHitsPlane{plane}.NextADCThreshold')
        ),

        'maxMultiHit': ParamSpec(
            'maxMultiHit', 5, 10, 'int', 
            fcl_paths=p('MaxMultiHit')
        ),

        'chi2NDF': ParamSpec(
            'chi2NDF', 200, 2500, 'float',
            fcl_paths=p('Chi2NDF')
        ),

        'longMaxHits': ParamSpec(
            'longMaxHits', 5, 15, 'int', size=3, 
            fcl_paths=p('LongMaxHits')
        ),

        'longPulseWidth': ParamSpec(
            'longPulseWidth', 8, 18, 'int', size=3, 
            fcl_paths=p('LongPulseWidth')
        ),
    }


def build_result_specs():
    return [
        ResultSpec('event', 'INTEGER'),
        ResultSpec('subRun', 'INTEGER'),
        ResultSpec('run', 'INTEGER'),

        ResultSpec('hitEnergy'),
        ResultSpec('ideEnergy'),
        ResultSpec('eventRatio'),

        ResultSpec('plane0HitEnergy'),
        ResultSpec('plane1HitEnergy'),
        ResultSpec('plane2HitEnergy'),

        ResultSpec('plane0IdeEnergy'),
        ResultSpec('plane1IdeEnergy'),
        ResultSpec('plane2IdeEnergy'),

        ResultSpec('plane0Ratio'),
        ResultSpec('plane1Ratio'),
        ResultSpec('plane2Ratio'),

        ResultSpec('protons', 'INTEGER'),
        ResultSpec('chargedPions', 'INTEGER'),
        ResultSpec('muons', 'INTEGER'),
        ResultSpec('electrons', 'INTEGER'),
        ResultSpec('neutralPions', 'INTEGER'),
        ResultSpec('gammas', 'INTEGER'),
    ]

# ============================================================
# 8. CLI
# ============================================================

import click

@click.group()
def cli():
    pass


@cli.command('create')
@click.option('-t', '--template', required=True, type=str, 
              help='Template FHiCL from which create the set of FHiCLs to run over')
@click.option('-o', '--output-dir', default='./fcls', type=str, 
              help='Output directory where the generated FHiCL files are generated')
@click.option('-n', '--n-samples', default=200, type=int, 
              help='Number of point sampled in the parameter space (Latin Hypercube Sampling)')
@click.option('--tag', default='test', type=str, 
              help='Additional tag to add in the DB outputs')
@click.option('--seed', type=int, default=42, 
              help='Latin Hypercube Sampling seed')
def create_grid(template, output_dir, n_samples, tag, seed):

    template = normalize_path(template)
    output_dir = normalize_path(output_dir)

    specs = build_specs()
    grid = ParamGrid(specs)
    fcl_manager = FclManager(template)

    param_sets = grid.sample_lhs(n_samples, seed=seed)

    ensure_dir(output_dir)

    for i, ps in enumerate(param_sets):
        if i % 50 == 0:
            print(f'Creating {i}/{len(param_sets)}')

        fcl_file = f'{output_dir}/hitTuning_{tag}_{i}.fcl'
        fcl_manager.generate(ps, specs, fcl_file, tag)

    print(f'Created {len(param_sets)} FCL files in {output_dir}')
    print(grid.__str__(param_sets))

@cli.command('run')
@click.option('-t', '--template', required=True, type=str, 
              help='Template FHiCL: required for FclManager')
@click.option('-f', '--fcl-file', required=True, type=str, 
              help='FHiCL file over which the data is run')
@click.option('-r', '--run-number', required=True, type=int,
              help='Unique identifier for the grid point')
@click.option('-i', '--input-file', required=True, type=str,
              help='Data file (ROOT) or list of ROOT files (.list file)')
@click.option('-o', '--output-file', required=True, type=str, 
              help='Output of the lar command, input to the scorer macro')
@click.option('--macro-path', required=True, type=str,
              help='Path to the hit scorer')
@click.option('--batches', default=10, type=int, 
              help='Number of batches of input files over which run the lar command')
def run_grid(template, fcl_file, run_number, input_file, output_file, macro_path, batches):
    template = normalize_path(template)
    fcl_file = normalize_path(fcl_file)
    input_file = normalize_path(input_file)
    output_file = normalize_path(output_file)
    macro_path = normalize_path(macro_path)

    specs = build_specs()
    result_specs = build_result_specs()

    fcl_manager = FclManager(template)
    db = RunDB(f"hitTuning_{run_number}.db", specs, result_specs)

    params = fcl_manager.parse(fcl_file, specs)

    load_root_macro(macro_path, 'hitScorer.C')
    histFile = f'hist_{run_number}.root'

    # ==========================================================
    # CASE 1: single ROOT file (no batching needed)
    # ==========================================================
    if is_root_file(input_file):

        run_lar(fcl_file, input_file, output_file)

        results_list = ROOT.hitScorer(output_file, histFile)

        for result in results_list:
            db.insert(
                params,
                meta={
                    'jobNum': run_number,
                    'timestamp': datetime.now().isoformat(),
                    'fcl_filename': fcl_file,
                    'output_filename': output_file,
                    'hist_filename': histFile
                },
                results=result_to_dict(result, result_specs)
            )

        db.commit()
        db.close()
        return

    # ==========================================================
    # CASE 2: file list → batching
    # ==========================================================

    filenames = read_file_list(input_file)

    print(f'Total input files: {len(filenames)}')

    for batch_idx, batch in enumerate(chunk_list(filenames, batches)):

        print(f'Processing batch {batch_idx} ({len(batch)} files)')

        # Create temporary file list
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as tmp:
            tmp.write('\n'.join(batch))
            tmp_filename = tmp.name

        try:
            # Run lar on this batch
            run_lar(fcl_file, tmp_filename, output_file)

            # Run ROOT scorer
            results_list = ROOT.hitScorer(output_file, histFile)

            for i, result in enumerate(results_list):

                db.insert(
                    params,
                    meta={
                        'jobNum': run_number,
                        'timestamp': datetime.now().isoformat(),
                        'fcl_filename': fcl_file,
                        'output_filename': output_file,
                        'hist_filename': histFile
                    },
                    results=result_to_dict(result, result_specs)
                )

                if i % 100 == 0:
                    db.commit()

        finally:
            os.remove(tmp_filename)

    db.commit()
    db.close()

    print('All batches processed.')

if __name__ == "__main__":
    cli()
