# Custom High Performance Data Structures

This repository experiments with building efficient containers that can act as drop in replacements for the standard library equivalents.  All code targets **C++23** and aims for full conformance with the latest language concepts.

The project focuses on:

* Memory efficient containers implemented using modern C++ techniques.
* API compatibility with standard library containers so they can be used in generic code.
* Comprehensive unit tests verifying behaviour and concept conformance.
* Small examples demonstrating real world use.

## Repository Layout

```
include/  - headers for the custom containers
examples/ - small usage examples
tests/    - doctest based unit tests
CMakeLists.txt - build configuration
```

### Provided Containers

The `include` directory contains a number of specialized containers and helper utilities.

- **array_packed** – stores integral values across bit planes to minimise memory use.
- **bucket_map** – deduplicates equal values and tracks them by buckets for compact storage.
- **chunk_map** – hierarchical map splitting keys into chunk/local positions for spatial data.
- **flat_tree_map** – sparse bitset presented as an ordered map.
- **flat_vector** + **flat_vector_array_packed** – flat contiguous storage for `array_packed` elements.
- **flyweight_map** – immutable deduplicating map assigning compact 32‑bit handles to values.
- **flyweight_block_map** – fixed size associative container using `flyweight_map` internally.
- **flyweight_mirror_block_map** – `flyweight_block_map` variant supporting mirrored orientations.
- **dense_map** – fixed range map storing values densely with a bitset for keys.
- **magica_voxel_io** – load and write MagicaVoxel `.vox` files.
- **reverse_mirror** – strategy object used by the mirrored block map.
- **arrow_proxy** – helper proxy enabling `iterator->` semantics for prvalue pairs.

Every class, method and member field in these headers carries a concise `@brief` documentation comment as a quick reference.

## Building and Testing

The project uses **CMake**. A typical build and test run looks like:

```bash
mkdir build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Unit tests live under `tests/` and exercise every container, also checking that iterators and ranges satisfy the appropriate C++23 concepts.

## Examples

The `examples/` directory provides small programs. `create_voxel.cpp` shows how to generate a simple MagicaVoxel file using `flyweight_block_map` along with the IO utilities. Running the tests will execute this example as well.

## Goal

The overall goal of this repository is to research **high performance custom data structures** that can serve as drop in replacements for their `std` counterparts.  By focusing on memory layout, deduplication and concept correctness, these containers aim to integrate seamlessly with modern algorithms while providing better performance characteristics for specialised use cases.

