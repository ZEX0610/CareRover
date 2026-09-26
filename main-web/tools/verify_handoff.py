#!/usr/bin/env python3
"""Verify an extracted release. Read-only; needs only Python standard library."""
import hashlib
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
failed=[];count=0
for filename in ['SOURCE_MANIFEST.json','EVIDENCE_MANIFEST.json']:
    manifest=ROOT/filename
    if not manifest.is_file():
        failed.append(filename+' missing');continue
    for name,expected in json.loads(manifest.read_text(encoding='utf-8')).items():
        p=(ROOT/name).resolve()
        if not p.is_relative_to(ROOT) or not p.is_file():failed.append(name+' missing/invalid');continue
        count+=1
        if hashlib.sha256(p.read_bytes()).hexdigest()!=expected:failed.append(name+' changed')
if failed:
    print('FAIL\n'+'\n'.join(failed));sys.exit(1)
print(f'PASS: {count} source/reference/evidence files match the exported SHA-256 manifests.')
print('This verifies file integrity, not hardware acceptance.')
