#!/usr/bin/env python3
"""Verify the 0915 plain SHA-256 manifest without requiring the absent ZIP."""
import argparse
import hashlib
from pathlib import Path

def verify(root):
    root=Path(root).resolve();count=0
    for line in (root/'MANIFEST.sha256').read_text().splitlines():
        if not line.strip() or line.startswith('#'):continue
        expected,name=line.split(None,1);path=(root/name.strip()).resolve()
        if not path.is_relative_to(root):raise ValueError('Manifest path escapes root')
        actual=hashlib.sha256(path.read_bytes()).hexdigest()
        if actual.lower()!=expected.lower():raise ValueError(f'Hash mismatch: {name}')
        count+=1
    return count
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path);args=parser.parse_args()
    print(f'PASS: {verify(args.directory)} manifest entries; outer ZIP not checked.')
