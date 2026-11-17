#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Package this consumer's explicit native outputs through its pinned engine."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--platform',choices=['windows','linux','macos'])
    args=parser.parse_args()
    product=json.loads((ROOT/'product.json').read_text())
    build=args.build_dir.resolve()
    metadata=json.loads((build/'build-products.json').read_text())
    def output_path(value):
        path=Path(value)
        return path if path.is_absolute() else build/path
    if metadata['product']!=product['engine_product']:
        raise ValueError('Build metadata belongs to a different consumer')
    # The engine package stores documentation images beside its README.
    readme=build/'WiiMenuRecomp-README.md'
    readme.write_text((ROOT/'README.md').read_text().replace('docs/images/','images/').replace('](LICENSE)','](LICENSES/WiiMenuRecomp-GPL3.txt)'))
    metadata['readme']=str(readme)
    (build/'build-products.json').write_text(json.dumps(metadata,indent=2)+'\n')
    shutil.copy2(ROOT/'LICENSE',output_path(metadata['licenses_dir'])/'WiiMenuRecomp-GPL3.txt')
    shutil.copy2(ROOT/'LICENSING.md',output_path(metadata['licenses_dir'])/'WiiMenuRecomp-LICENSING.md')
    command=[sys.executable,str(ROOT/'wiillr/tools/package_release.py'),
             '--repo',str(ROOT),'--product',product['engine_product'],
             '--version',metadata['version'],'--arch',metadata['arch'],
             '--build-dir',str(build),'--output-dir',str(args.output_dir.resolve()),
             '--inputs-dir',str(output_path(metadata['inputs_dir'])),'--native-exe',str(output_path(metadata['native_exe'])),
             '--gx-library',str(output_path(metadata['gx_library']))]
    if args.platform:command+=['--platform',args.platform]
    return subprocess.call(command)

if __name__=='__main__':raise SystemExit(main())
