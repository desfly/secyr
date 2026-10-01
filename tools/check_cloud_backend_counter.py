#!/usr/bin/env python3
import tempfile
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]; sys.path.insert(0,str(ROOT/"cloud-backend"))
from homeguard_cloud.counter_store import CounterStore
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/"state.db"
    a=CounterStore(p)
    assert a.next("HG-A")==1
    assert a.next("HG-A")==2
    assert a.next("HG-B")==1
    b=CounterStore(p)
    assert b.next("HG-A")==3
print("Cloud backend durable counter PASS")
