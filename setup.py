"""Build script: compiles the C++17 core and the pybind11 binding into the
streamgraph._streamgraph extension module."""

import os
import re
import subprocess
import sys

import setuptools
from setuptools import Extension
from setuptools.command.build_ext import build_ext

ext_sources = [
    os.path.join("src", "dsu.cpp"),
    os.path.join("src", "streamgraph.cpp"),
    os.path.join("src", "triangle.cpp"),
    os.path.join("src", "batch.cpp"),
    os.path.join("src", "serialise.cpp"),
    os.path.join("src", "betweenness.cpp"),
    os.path.join("src", "bindings_python.cpp"),
]


def get_pybind_include():
    import pybind11
    return pybind11.get_include()


def get_numpy_include():
    import numpy
    return numpy.get_include()


class streamgraph_build_ext(build_ext):
    """Inject OpenMP and optimisation flags per platform."""

    def build_extensions(self):
        if hasattr(self.compiler, "compiler") and self.compiler.compiler:
            compiler = self.compiler.compiler[0]
        else:
            compiler = self.compiler.__class__.__name__

        for e in self.extensions:
            e.include_dirs.append(get_pybind_include())
            e.include_dirs.append(get_numpy_include())

        if sys.platform == "win32":
            for e in self.extensions:
                e.extra_compile_args += ["/O2", "/openmp", "/std:c++17"]
        elif sys.platform == "darwin":
            is_clang = "clang" in compiler and "icc" not in compiler
            omp_prefix = None
            if is_clang:
                #Apple clang has no bundled omp.h; build against libomp explicitly
                omp_prefix = os.environ.get("LIBOMP_PREFIX")
                if not omp_prefix:
                    try:
                        omp_prefix = subprocess.check_output(
                            ["brew", "--prefix", "libomp"],
                            stderr=subprocess.DEVNULL,
                        ).decode().strip()
                    except Exception:
                        omp_prefix = None
            for e in self.extensions:
                e.extra_compile_args += ["-O3", "-std=c++17"]
                if is_clang:
                    e.extra_compile_args += ["-Xpreprocessor", "-fopenmp"]
                    e.extra_link_args += ["-lomp"]
                    if omp_prefix:
                        e.include_dirs.append(os.path.join(omp_prefix, "include"))
                        e.library_dirs.append(os.path.join(omp_prefix, "lib"))
                else:
                    e.extra_compile_args += ["-fopenmp"]
                    e.extra_link_args += ["-fopenmp"]
        else:
            for e in self.extensions:
                e.extra_compile_args += ["-O3", "-std=c++17", "-fopenmp"]
                e.extra_link_args += ["-fopenmp"]

        build_ext.build_extensions(self)


ext_modules = [
    Extension("streamgraph._streamgraph", sources=ext_sources, include_dirs=["src/"], language="c++", define_macros=[("STREAMGRAPH_PYTHON", "1")])
]

with open("README.md", "r", encoding="utf8") as fh:
    long_description = fh.read()

with open(os.path.join("streamgraph", "__init__.py"), "r", encoding="utf8") as fh:
    m = re.search(r"(?m)^\s*__version__\s*=\s*[\"']([0-9.]+)[\"']", fh.read())
    if m is None:
        raise ValueError("the package version could not be read")
    __version__ = m.group(1)

setuptools.setup(
    name="streamgraph",
    version=__version__,
    description="streamgraph: streaming (dynamic) graph analytics",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="bagol1000",
    author_email="01199218@pw.edu.pl",
    license="MIT",
    install_requires=["numpy>=1.21"],
    python_requires=">=3.9",
    packages=setuptools.find_packages(),
    include_package_data=True,
    package_data={"streamgraph": ["py.typed", "*.pyi"]},
    classifiers=[
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3 :: Only",
        "License :: OSI Approved :: MIT License",
        "Intended Audience :: Science/Research",
        "Development Status :: 3 - Alpha",
        "Topic :: Scientific/Engineering",
    ],
    cmdclass={"build_ext": streamgraph_build_ext},
    ext_modules=ext_modules,
)
