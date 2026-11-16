#!/usr/bin/env python3
"""Build the Wii Menu consumer through its pinned engine's platform driver."""
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
def main():
    engine=ROOT/'wiillr'
    command=[sys.executable,str(engine/'tools/build.py'),*sys.argv[1:],
             '--product','wii-menu','--cmake-definition',
             'WIILLR_MENU_GRID_SOURCE='+str(ROOT/'src/menu_grid.c')]
    return subprocess.call(command)
if __name__=='__main__':raise SystemExit(main())
