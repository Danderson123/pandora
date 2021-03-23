# Changelog

All notable changes to this project will be documented in this file.

The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this
project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.8.0]

### Added

- We now provide a script to build a portable precompiled binary as
  another option to run `pandora` easily. The portable binary is now
  provided with the release;
- `pandora` can now provide a meaningful stack trace in case of errors,
  to facilitate debugging (need to pass flag `-DPRINT_STACKTRACE` to
  `CMake`). Due to this, we now add debug symbols (`-g` flag) to every
  `pandora` build type, but this
  [does not impact performance](https://stackoverflow.com/a/39223245).
  The precompiled binary has this enabled.

### Changed

- We now use the [Hunter](https://github.com/cpp-pm/hunter) package
  manager, removing the requirement of having `ZLIB` and `Boost`
  system-wide installations;
- `GATB` is now a git submodule instead of an external project
  downloaded and compiled during compilation time. This means that when
  git cloning `pandora`, `cgranges` and `GATB` are also
  downloaded/cloned, and when preparing the build (running `cmake`),
  `Hunter` downloads and installs `Boost`, `GTest` and `ZLIB`. Thus we
  still need internet connection to prepare the build (running `cmake`)
  but not for compiling (running `make`).
- We now use a GATB fork that accepts a `ZLIB` custom installation;
- Refactored all thirdparty libraries (`cgranges`, `GATB`, `backward`,
  `CLI11`, `inthash`) into their own directory `thirdparty`.

### Fixed

- Refactored asserts into exceptions, and now `pandora` can be compiled
  correctly in the `Release` mode. The build process is thus able to
  create a more optimized binary, resulting in improved performance.
- Don't assume Nanopore reads are longer than loci [[#265][265]]



## [v0.7.0]

There is a significant amount of changes to the project between version
0.6 and this release. Only major things are listed here. Future releases
from this point will have their changes meticulously documented here.

### Added

- `discover` subcommand for de novo variant discovery [[#234][234]]
- many more tests

### Changed

- FASTA/Q files are now parsed with `klib` [[#223][223]]
- command-line interface is now overhauled with many breaking changes
  [[#224][224]]
- global genotyping has been made default [[#220][220]]
- Various improvements to VCF-related functions

### Fixed

- k-mer coverage underflow bug in `LocalPRG` [[#183][183]]

[Unreleased]: https://github.com/olivierlacan/keep-a-changelog/compare/0.8.0...HEAD
[0.8.0]: https://github.com/rmcolq/pandora/releases/tag/0.8.0
[v0.7.0]: https://github.com/rmcolq/pandora/releases/tag/v0.7.0

[183]: https://github.com/rmcolq/pandora/issues/183
[220]: https://github.com/rmcolq/pandora/pull/220
[223]: https://github.com/rmcolq/pandora/pull/223
[224]: https://github.com/rmcolq/pandora/pull/224
[234]: https://github.com/rmcolq/pandora/pull/234
[265]: https://github.com/rmcolq/pandora/pull/265

