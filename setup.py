# Modified from https://github.com/pybind/pbtest, find the original license there

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from distutils.sysconfig import get_python_inc
from pip import locations
import multiprocessing
import os
import sys
import setuptools
import subprocess

def build_extension():
    try:
        os.mkdir('build_python')
    except Exception:
        pass

    n_cores = 1
    try:
        n_cores = multiprocessing.cpu_count()
    except Exception:
        pass

    os.chdir('build_python')
    subprocess.call(['cmake',
        '-DBUILD_LIBRARY=OFF',
        '-DBUILD_EXAMPLES=OFF',
        '-DBUILD_TESTS=OFF',
        '-DBUILD_PYTHON=ON',
        '-DPYTHON_INCLUDE=' + get_python_inc(),
        '..'
    ])
    subprocess.call(['make', '-j' + str(n_cores)])

def main():
    build_extension()

    setup(
        name='taskloaf',
        version='0.0.1',
        author='T. Ben Thompson',
        author_email='t.ben.thompson@gmail.com',
        url='https://github.com/tbenthompson/taskloaf',
        description='taskloaf',
        long_description='',
        install_requires=['dill'],
        zip_safe=False,
    )

if __name__ == '__main__':
    main()
