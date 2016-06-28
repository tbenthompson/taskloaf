from setuptools import setup

def main():
    setup(
        packages = ['taskloaf'],
        install_requires = ['dill', 'pybind11', 'cppimport'],
        zip_safe = False,

        name = 'taskloaf',
        version = '0.0.4',
        description = 'taskloaf',
        long_description = 'taskloaf',
        url = 'https://github.com/tbenthompson/taskloaf',
        author = 'T. Ben Thompson',
        author_email = 't.ben.thompson@gmail.com',
        license = 'MIT',
        platforms = ['any'],
        classifiers = [
            'Development Status :: 3 - Alpha',
            'Intended Audience :: Developers',
            'Intended Audience :: Science/Research',
            'Operating System :: OS Independent',
            'Operating System :: POSIX',
            'Topic :: Scientific/Engineering',
            'Topic :: Software Development',
            'Topic :: System :: Distributed Computing',
            'License :: OSI Approved :: MIT License',
            'Programming Language :: Python :: 2',
            'Programming Language :: Python :: 2.7',
            'Programming Language :: Python :: 3',
            'Programming Language :: Python :: 3.2',
            'Programming Language :: Python :: 3.3',
            'Programming Language :: Python :: 3.4',
            'Programming Language :: Python :: 3.5',
            'Programming Language :: C++'
        ],
    )

if __name__ == '__main__':
    main()
