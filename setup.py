# Modified from https://github.com/pybind/pbtest, find the original license there

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from distutils.sysconfig import get_python_inc
from pip import locations
import multiprocessing
import os
import shutil
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
    os.chdir(os.pardir)

    libname = 'taskloaf_wrapper.so'
    shutil.copy(
        os.path.join('build_python', libname),
        os.path.join('taskloaf', libname)
    )

def main():
    build_extension()

    setup(
        packages = ['taskloaf'],
        include_package_data = True,
        package_data = {'taskloaf': ['taskloaf_wrapper.so']},
        install_requires = ['dill'],
        zip_safe = False,

        name = 'taskloaf',
        version = '0.0.1',
        description = 'taskloaf',
        long_description = '',
        url = 'https://github.com/tbenthompson/taskloaf',
        author = 'T. Ben Thompson',
        author_email = 't.ben.thompson@gmail.com',
        classifiers = [
            'Development Status :: 4 - Beta',
            'Intended Audience :: Developers',
            'Intended Audience :: Science/Research',
            'Operating System :: OS Independent',
            'Operating System :: POSIX',
            'Topic :: Scientific/Engineering',
            'Topic :: Software Development',
            'Topic :: System :: Distributed Computing',
            'License :: OSI Approved :: MIT License',
            'Programming Language :: Python :: 2',
            'Programming Language :: Python :: 2.6',
            'Programming Language :: Python :: 2.7',
            'Programming Language :: Python :: 3',
            'Programming Language :: Python :: 3.2',
            'Programming Language :: Python :: 3.3',
            'Programming Language :: Python :: 3.4',
            'Programming Language :: C++'
        ],
    )

if __name__ == '__main__':
    main()
