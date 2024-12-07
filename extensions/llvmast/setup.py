import multiprocessing as mp
from pathlib import Path
import base64
import bz2
import pickle
import os
import sys
from setuptools import setup, Command

import torch
from bitarray import bitarray
from graph import graph
from graphdb import graphdb
from llvmast import llvmast
import cached_cflags

EXT_USE_WORKSPACE = os.environ.get("EXT_USE_WORKSPACE") or '.'

class DB:

    memmap = {'hdr': ['.h', '.hh'],
              'src': ['.c', '.cc', '.cpp', '.cxx'],
              'skip': ['env/lib/python'],
              'all': False,
              }    
    db = "./setup.db"
    rw = False

    def unpack(self):
        ret = None
        with open(self.db, "r", encoding='ascii') as f:
            ret = f.read()
            ret = pickle.loads(bz2.decompress(base64.b64decode(ret)))
        return ret

    def pack(self, obj):
        with open(self.db, "w") as f:
            data = base64.b64encode(bz2.compress(pickle.dumps(obj))).decode('ascii')
            f.write(data)
            f.flush()
        return self.db
    
    def __init__(self, rw=False):
        self.rw = rw
        if Path(self.db).is_file():
            self.memmap = self.unpack()
    
    def __enter__(self):
        return self.memmap
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.rw:
            self.pack(self.memmap)

class clean(Command):
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        p = Path('.')
        for child in p.iterdir():
            if child.is_dir(): continue
            if Path(DB.db) == child: child.unlink()
            if Path(llvmast.DB_FILE) == child: child.unlink()
            if Path(llvmast.DB_FILE+"-lock") == child: child.unlink()
            if "llvmast_" in str(child) and ".log" == child.suffix: child.unlink()

def kernel(idx, step, log):
    sys.stderr = open(log, 'w')
    ret = None
    with DB() as db:
        indexlist = sorted([k for k in db.keys() if k not in ['hdr', 'src', 'skip', 'all']])
        ret = [0] * len(indexlist)
        for i in range(idx, len(indexlist), step):
            print(f"[{idx}:{step}:{i}/{len(indexlist)}]", indexlist[i], file=sys.stderr, flush=True)
            pth = Path(indexlist[i])
            if pth.suffix in db['src'] and (db['all'] or db[str(pth)]['cc_mtime'] != db[str(pth)]['st_mtime']):
                llvmast.llvmast(['-c', str(pth)] + ['--'] + cached_cflags.CFLAGS)
            ret[i] = db[str(pth)]['st_mtime']
    return ret

class prepare(Command):
    
    _workspace = []
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        self._workspace = [EXT_USE_WORKSPACE]
    
    def run(self):
        with DB(rw=True) as db:
            db['all'] = False
            while self._workspace:
                pth = Path(self._workspace.pop(0))
                for child in pth.iterdir():
                    if child.is_dir():
                        self._workspace.append(str(child))
                        continue
                    if child.suffix not in db['hdr'] + db['src']:
                        continue
                    if next((n for n in db['skip'] if n in str(child)), None):
                        continue
                    db.setdefault(str(child), {})
                    db[str(child)].setdefault('cc_mtime', 0)
                    db[str(child)]['st_mtime'] = child.lstat().st_mtime_ns
                    db['all'] = db['all'] or (child.suffix in db['hdr'] and db[str(child)]['cc_mtime'] != db[str(child)]['st_mtime'])
        
        proc_count = os.cpu_count()  # serialised over the shared DB
        logs = [f"llvmast_{i}.log" for i in range(proc_count)]
        print(f"logs={logs}")

        mtx = None
        with mp.get_context('fork').Pool(processes=proc_count) as pool:
            args = lambda i: (i, proc_count, logs[i])
            r = pool.starmap_async(kernel, [args(i) for i in range(proc_count)])
            mtx = r.get()
            
        # reduce the results of the kernels
        with DB(rw=True) as db:
            indexlist = sorted([k for k in db.keys() if k not in ['hdr', 'src', 'skip', 'all']])
            for row in mtx:
                for i,v in enumerate(row):
                    if not v: continue
                    file = indexlist[i]
                    db[file]['cc_mtime'] = v

setup(name='extensions',
      packages=[],
      ext_modules=[],
      cmdclass={'prepare': prepare,
                'clean': clean,
                'test': None},
      install_requires=[])
