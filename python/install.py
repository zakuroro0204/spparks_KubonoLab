#!/usr/local/bin/python3

# copy SPPARKS src/libspparks.so and spparks.py to system dirs

instructions = """
Syntax: python install.py [-h] [libdir] [pydir]
        libdir = target dir for src/libspparks.so, default = /usr/local/lib
        pydir = target dir for spparks.py, default = Python site-packages dir
"""

import sys
import os
import subprocess  # Python3では commands モジュールの代わりに subprocess を使用

if (len(sys.argv) > 1 and sys.argv[1] == "-h") or len(sys.argv) > 3:
    print(instructions)
    sys.exit()

if len(sys.argv) >= 2:
    libdir = sys.argv[1]
else:
    libdir = "/usr/local/lib"

if len(sys.argv) == 3:
    pydir = sys.argv[2]
else:
    pydir = ""

# copy C lib to libdir if it exists
# warn if not in LD_LIBRARY_PATH or LD_LIBRARY_PATH is undefined

if not os.path.isdir(libdir):
    print(f"ERROR: libdir {libdir} does not exist")
    sys.exit()
    
if "LD_LIBRARY_PATH" not in os.environ:
    print(f"WARNING: LD_LIBRARY_PATH undefined, cannot check libdir {libdir}")
else:
    libpaths = os.environ['LD_LIBRARY_PATH'].split(':')
    if libdir not in libpaths:
        print(f"WARNING: libdir {libdir} not in LD_LIBRARY_PATH")

cmd = f"cp ../src/libspparks.so {libdir}"
print(cmd)
try:
    output = subprocess.check_output(cmd, shell=True, universal_newlines=True, stderr=subprocess.STDOUT)
    if output.strip():
        print(output)
except subprocess.CalledProcessError as e:
    if e.output.strip():
        print(e.output)

# copy spparks.py to pydir if it exists
# if pydir not specified, install in site-packages via distutils setup()

if pydir:
    if not os.path.isdir(pydir):
        print(f"ERROR: pydir {pydir} does not exist")
        sys.exit()
    cmd = f"cp ../python/spparks.py {pydir}"
    print(cmd)
    try:
        output = subprocess.check_output(cmd, shell=True, universal_newlines=True, stderr=subprocess.STDOUT)
        if output.strip():
            print(output)
    except subprocess.CalledProcessError as e:
        if e.output.strip():
            print(e.output)
    sys.exit()
    
print("installing spparks.py in Python site-packages dir")

os.chdir('../python')                # in case invoked via make in src dir

from distutils.core import setup
sys.argv = ["setup.py","install"]    # as if had run "python setup.py install"
setup(name = "spparks",
      version = "6Mar13",
      author = "Steve Plimpton",
      author_email = "sjplimp@sandia.gov",
      url = "http://spparks.sandia.gov",
      description = "SPPARKS kinetic Monte Carlo library",
      py_modules = ["spparks"])