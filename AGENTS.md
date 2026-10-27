# QuantLab agent instructions

## Purpose and layout

QuantLab is an educational C++17 library for financial time-series analytics.
It demonstrates reproducible CMake builds, doctest unit tests, Google Benchmark
measurements, and automatically published Doxygen documentation.

- `include/quantlab/`: public API and its Doxygen contracts.
- `src/`: library implementation and the `quantlab_demo` executable.
- `tests/`: doctest cases registered with CTest.
- `benchmarks/`: Google Benchmark performance measurements.
- `docs/`: Russian documentation homepage and Doxygen template.
- `.github/workflows/ci.yml`: Linux/Windows build, tests, benchmarks,
  sanitizers, and GitHub Pages deployment.
- `scripts/`: explicit publication helper; it requires the user's GitHub login.

## Build, test, and benchmark

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure --no-tests=error
./build/quantlab_demo
./build/benchmarks/quantlab_benchmark --benchmark_min_time=0.05s
```

Visual Studio uses `build/Release/quantlab_demo.exe` and
`build/benchmarks/Release/quantlab_benchmark.exe`. MinGW uses the same
single-configuration paths as Linux, with `.exe` extensions; choose its
generator explicitly in a fresh build directory. First configuration downloads
checksum-pinned dependencies. Use a compiler with standard C++ thread support.

Documentation (requires Doxygen):

```sh
cmake -S . -B build-docs -DBUILD_TESTING=OFF -DQUANTLAB_BUILD_BENCHMARKS=OFF -DQUANTLAB_BUILD_DOCS=ON
cmake --build build-docs --target docs
```

The generated site is `build-docs/docs/html/index.html`.

## Code conventions

- Use C++17, four spaces, `snake_case` functions, and `quantlab` for public names.
- Keep interfaces in headers and implementations in source files.
- Document public parameters, results, exceptions, and complexity with Doxygen.
- Preserve the documented validation and exception contracts. Never silently
  accept NaN, infinity, invalid windows, or nonpositive prices.
- Preserve linear time complexity; keep numerical stability in mind.
- Keep input generation outside benchmark loops and retain results with
  `benchmark::DoNotOptimize`. Measure optimized Release builds.
- Add meaningful tests when behavior changes, including relevant boundaries.
- Keep project warnings enabled; fix their causes instead of suppressing them.

## Completion and limits

- Before finishing a code task, build the affected project and run CTest.
  Run the benchmark when performance behavior changes and regenerate Doxygen
  when public contracts change. Report actual results and any unrun checks.
- Change CI/CD or publish externally only when requested or necessary for the
  user's current authorized task. The initial course setup explicitly includes
  configuring CI/CD and publishing this project and its documentation.
- Do not disable tests, sanitizer checks, or strict documentation checks to make
  a failed run green. Diagnose and fix the underlying issue.
- Do not alter third-party dependency sources; update pinned versions and hashes
  together when an update is explicitly requested.
- Do not commit build products, local tools, credentials, machine-specific paths,
  the assignment PDF, or private student data. Keep dependency downloads under
  ignored local build/tool directories.
- Do not force-push, delete repositories, rewrite published history, or publish
  into an unrelated existing repository.
- Never claim remote CI or Pages succeeded without inspecting that exact
  commit's run and the actual documentation URL.
