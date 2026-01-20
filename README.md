# Mandelbrot Fractal — stdexec & SFML

[![CI](https://github.com/<YOUR_GITHUB_USERNAME>/MandelbrotFractal/actions/workflows/ci.yml/badge.svg)](https://github.com/<YOUR_GITHUB_USERNAME>/MandelbrotFractal/actions/workflows/ci.yml)
![ASAN](https://img.shields.io/badge/ASAN-enabled-brightgreen)
![clang--tidy](https://img.shields.io/badge/clang--tidy-enabled-blue)
![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)
![License](https://img.shields.io/badge/license-MIT-lightgrey)

A modern **C++23** implementation of the **Mandelbrot fractal renderer** built around **NVIDIA stdexec** sender/receiver model, structural parallelism, and an explicit execution pipeline.

The project demonstrates how to design a non‑trivial asynchronous rendering pipeline with clear ownership, deterministic shutdown, and testable execution semantics — without relying on futures or callbacks. It is intended as a **public reference implementation** of modern C++ sender/receiver architecture applied to a real graphical workload.

---

## Key Features

* **Asynchronous rendering pipeline** based on `stdexec` senders/receivers
* **Structural parallelism** via `exec::static_thread_pool`
* **Deterministic frame scheduling** (fixed FPS)
* **Interactive zoom in / zoom out** (mouse‑driven)
* **Zero GUI logic in tests** (headless validation)
* **Memory‑safe by design** (ASAN‑verified)

---

## Architecture Overview

The application is structured as a composable execution pipeline:

```
SfmlEventHandler
    → CalculateMandelbrotAsyncSender
        → MandelbrotRenderer::RenderAsync
            → MandelbrotSender (N stripes, parallel)
    → SFMLRender
    → WaitForFPS
```

### Core Components

| Component                        | Responsibility                                       |
| -------------------------------- | ---------------------------------------------------- |
| `SfmlEventHandler`               | Event handling, zoom logic, viewport updates         |
| `MandelbrotSender`               | Iteration computation for a pixel region             |
| `MandelbrotRenderer`             | Parallel rendering, stripe scheduling, color mapping |
| `CalculateMandelbrotAsyncSender` | Conditional re‑render adapter                        |
| `SFMLRender`                     | Frame upload and presentation                        |
| `WaitForFPS`                     | Frame pacing (fixed FPS)                             |

All components are **pure senders** with explicit completion semantics (`set_value`, `set_error`, `set_stopped`).

---

## Parallel Rendering Model

* The screen is split into **N horizontal stripes**
* Each stripe is processed by an independent `MandelbrotSender`
* Stripes are executed concurrently on a static thread pool
* Results are merged deterministically into a single frame
* Color buffer is allocated **once** and reused across frames

This approach avoids false sharing, repeated allocations, and hidden synchronization.

---

## Testing Strategy

The project includes **unit and integration tests** covering:

* Individual senders (`MandelbrotSender`, `RenderAsync`)
* Pipeline composition
* Correct propagation of:

  * `set_value`
  * `set_error`
  * `set_stopped`
* Custom user receivers with shared state

All tests are **headless** and CI‑friendly.

---

## Continuous Integration

The repository is equipped with a production‑grade CI pipeline:

* **Release build + unit tests**
* **ASAN job** (AddressSanitizer, Debug build)
* **clang‑tidy static analysis**
* **Docker‑based environment** for full reproducibility

CI validates correctness, memory safety, and code quality on every PR.

---

## Build & Run

### Requirements

* C++23 compatible compiler (GCC ≥ 15 / Clang ≥ 19)
* CMake ≥ 3.30
* SFML
* NVIDIA stdexec
* GoogleTest
* Matplot++ (dependency requirement)

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Run

```bash
./build/MandelbrotFractal
```

---

## Project Goals

This repository is intended for public use and review. It focuses on **code clarity, correctness, and architectural soundness** rather than visual effects or UI complexity.

This project was created to:

* Explore **stdexec** beyond toy examples
* Demonstrate **real‑world sender/receiver pipelines**
* Show how modern C++ can replace callback‑driven async designs
* Serve as a reference for **testable asynchronous architecture**

---

## License

MIT License

---

*Built with modern C++ and a strong preference for explicit execution over hidden control flow.*

