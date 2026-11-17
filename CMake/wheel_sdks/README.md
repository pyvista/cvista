# cvista-sdk

## Overview

`cvista-sdk` distributes the cvista SDK (the VTK C++ headers, CMake config, and
Python wrap tools from the same build that produced the `cvista` runtime wheel) as
a first-class Python wheel.

The wheel registers a scikit-build-core `cmake.prefix`
[entry point][scikit-build-core-entrypoint], so a downstream scikit-build-core
project that depends on `cvista-sdk` gets the bundled install tree on
`CMAKE_PREFIX_PATH` automatically and `find_package(VTK CONFIG)` resolves to it.
Install the SDK whose version matches your `cvista` wheel.

The SDK installs on Python 3.11 and later, like the `cvista` wheel. On 3.11 it
serves C++ consumers that name their components, for example
`find_package(VTK CONFIG REQUIRED COMPONENTS CommonCore)`. An unqualified
`find_package(VTK CONFIG)` also loads the Python wrapping components, and those
require at least the Python version the SDK was built with (3.12 for published
wheels), as does `vtk_module_wrap_python`.

```bash
pip install cvista cvista-sdk
```

```cmake
find_package(VTK CONFIG REQUIRED)
```

[scikit-build-core-entrypoint]:
  https://scikit-build-core.readthedocs.io/en/latest/cmakelists.html#finding-other-packages

## License

Distributed under the OSI-approved BSD 3-clause License. See Copyright.txt for
details.
