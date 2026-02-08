import os
import sqlite3

import sqlite3

def merge_sqlite_dbs(db_files, dest_db, table="runs", conflict="ignore"):
    """
    Merge a list of SQLite .db files into a single destination DB.

    - db_files: list of source .db file paths
    - dest_db: path to the merged .db file
    - table: table name to merge (assumes same schema in all DBs)
    - conflict: 'ignore' or 'replace' for duplicate primary keys
    """
    if not db_files:
        print("No DB files to merge.")
        return

    # Open destination (created if it doesn't exist)
    dest_conn = sqlite3.connect(dest_db)
    dest_cur = dest_conn.cursor()

    # Use first DB as template for schema if table missing
    first_db = db_files[0]
    print(first_db)
    with sqlite3.connect(first_db) as src_conn:
        # Make sure table exists in dest with same schema
        dest_cur.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
            (table,),
        )
        if dest_cur.fetchone() is None:
            src_cur = src_conn.cursor()
            src_cur.execute(
                "SELECT sql FROM sqlite_master WHERE type='table' AND name=?",
                (table,),
            )
            row = src_cur.fetchone()
            if not row or not row[0]:
                raise RuntimeError(f"Cannot get schema for table '{table}' from {first_db}")
            dest_cur.execute(row[0])
            dest_conn.commit()

    # Merge all DBs
    for path in db_files:
        print(f"Merging {path}")
        # Attach each DB and bulk-copy rows
        verb = "IGNORE" if conflict == "ignore" else "REPLACE"
        dest_cur.execute(f"ATTACH DATABASE '{path}' AS src")
        try:
            # Explicit column list for safety
            src_conn = sqlite3.connect(path)
            src_cur = src_conn.cursor()
            src_cur.execute(f"PRAGMA table_info('{table}')")
            cols_info = src_cur.fetchall()
            src_conn.close()
            if not cols_info:
                print(f"  Skipping {path}: table '{table}' not found")
                dest_cur.execute("DETACH DATABASE src")
                continue

            col_names = [c[1] for c in cols_info if c[1] != "id"]  # skip primary-key id
            col_list = ",".join([f"'{c}'" for c in col_names])

            sql = (
                f"INSERT INTO '{table}' ({col_list}) "
                f"SELECT {','.join(col_names)} FROM src.'{table}'"
            )
            dest_cur.execute("BEGIN")
            dest_cur.execute(sql)
            dest_conn.commit()
        finally:
            dest_cur.execute("DETACH DATABASE src")

    dest_conn.close()
    print(f"Done. Merged DB written to: {dest_db}")

if __name__ == "__main__":

    inputDir = '/nashome/m/micarrig/icarus/hitTuning/gridDBV2/'
    dest_db = '/nashome/m/micarrig/icarus/hitTuning/hitTuning_merged_v2.db'

    db_files = []
    counter = 0
    for dirpath, dirnames, filenames in os.walk(inputDir):
        for filename in filenames:
            #if counter > 10: break
            if filename.endswith('.db'):
                fullPath = os.path.join(dirpath, filename)
                db_files.append(fullPath)
                counter += 1

    merge_sqlite_dbs(db_files, dest_db, table="runs", conflict="ignore")
