from setuptools import setup

version = open('VERSION').read()

try:
    import pypandoc
    description = pypandoc.convert('README.md', 'rst')
except (IOError, ImportError):
    description = open('README.md').read()

setup(
    packages = ['taskloaf'],

    install_requires = ['uvloop', 'cloudpickle', 'pycapnp', 'attrs', 'structlog', 'pyzmq'],
    zip_safe = False,
    include_package_data = True,

    name = 'taskloaf',
    version = version,
    description = '',
    long_description = description,

    url = 'https://github.com/tbenthompson/taskloaf',
    author = 'T. Ben Thompson',
    author_email = 't.ben.thompson@gmail.com',
    license = 'MIT',
    platforms = ['any']
)
