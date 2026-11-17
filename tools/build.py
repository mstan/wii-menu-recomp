#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the Wii Menu consumer through its pinned engine's platform driver."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
def main():
    product=json.loads((ROOT/'product.json').read_text())
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version',default=product['version'])
    parser.add_argument('--inputs-dir',type=Path,required=True)
    parser.add_argument('--build-dir',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--arch')
    parser.add_argument('--cmake-definition',action='append',default=[])
    parser.add_argument('--no-package',action='store_true')
    args=parser.parse_args()
    engine=ROOT/'wiillr'
    command=[sys.executable,str(engine/'tools/build.py'),'--product',product['engine_product'],
             '--version',args.version,'--inputs-dir',str(args.inputs_dir.resolve()),
             '--build-dir',str(args.build_dir.resolve()),'--output-dir',str(args.output_dir.resolve()),
             '--jobs',str(args.jobs),'--no-package']
    if args.arch:command+=['--arch',args.arch]
    for value in args.cmake_definition:command+=['--cmake-definition',value]
    command+=['--cmake-definition','WIILLR_MENU_GRID_SOURCE='+str(ROOT/product['provider'])]
    result=subprocess.call(command)
    if result:return result
    metadata=args.build_dir/'build-products.json'
    products=json.loads(metadata.read_text())
    products['readme']=str(ROOT/'README.md')
    products['consumer']=dict(product,source=str(ROOT))
    metadata.write_text(json.dumps(products,indent=2)+'\n')
    shutil.copy2(ROOT/'LICENSE',Path(products['licenses_dir'])/'WiiMenuRecomp-GPL3.txt')
    shutil.copy2(ROOT/'LICENSING.md',Path(products['licenses_dir'])/'WiiMenuRecomp-LICENSING.md')
    if args.no_package:return 0
    return subprocess.call([sys.executable,str(ROOT/'tools/package.py'),
                            '--build-dir',str(args.build_dir.resolve()),'--output-dir',str(args.output_dir.resolve())])
if __name__=='__main__':raise SystemExit(main())
