#!/Library/Frameworks/Python.framework/Versions/3.6/bin/python3
from distutils.core import setup
from Cython.Build import cythonize

setup(
    ext_modules = cythonize("functions.pyx")
)
