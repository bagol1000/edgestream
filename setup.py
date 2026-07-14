"""Build script: compiles the C++17 core and the pybind11 binding into the
edgestream._edgestream extension module. Package metadata lives in
pyproject.toml; only the extension build stays here."""

import os
import shutil
import subprocess
import sys

import setuptools
from setuptools import Extension
from setuptools.command.build_ext import build_ext

ext_sources = [
    os.path.join("src", "dsu.cpp"),
    os.path.join("src", "edgestream.cpp"),
    os.path.join("src", "triangle.cpp"),
    os.path.join("src", "batch.cpp"),
    os.path.join("src", "serialise.cpp"),
    os.path.join("src", "betweenness.cpp"),
    os.path.join("src", "centrality.cpp"),
    os.path.join("src", "bindings_python.cpp"),
]


def get_pybind_include():
    import pybind11
    return pybind11.get_include()


def get_numpy_include():
    import numpy
    return numpy.get_include()


class edgestream_build_ext(build_ext):
    """Inject OpenMP and optimisation flags per platform."""

    def build_extensions(self):
        # Native object caches are unsafe across header/layout changes and
        # renamed modules. The core is small enough that a full rebuild is the
        # safer release-engineering default.
        self.force = True
        # A renamed extension can otherwise survive in build/lib and leak into
        # a wheel produced from a dirty developer tree.
        stale_package = os.path.join(self.build_lib, "streamgraph")
        if os.path.isdir(stale_package):
            shutil.rmtree(stale_package)
        stale_cache = os.path.join(self.build_lib, "edgestream", "__pycache__")
        if os.path.isdir(stale_cache):
            shutil.rmtree(stale_cache)
        if hasattr(self.compiler, "compiler") and self.compiler.compiler:
            compiler = self.compiler.compiler[0]
        else:
            compiler = self.compiler.__class__.__name__

        for e in self.extensions:
            e.include_dirs.append(get_pybind_include())
            e.include_dirs.append(get_numpy_include())

        if os.environ.get("EDGESTREAM_NO_OPENMP", "").lower() in {"1", "true", "yes"}:
            for e in self.extensions:
                e.extra_compile_args += (
                    ["/O2", "/std:c++17"] if sys.platform == "win32"
                    else ["-O3", "-std=c++17"]
                )
            return build_ext.build_extensions(self)

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
    Extension(
        "edgestream._edgestream",
        sources=ext_sources,
        include_dirs=["src/"],
        language="c++",
        define_macros=[("EDGESTREAM_PYTHON", "1")],
    )
]

setuptools.setup(
    cmdclass={"build_ext": edgestream_build_ext},
    ext_modules=ext_modules,
)
