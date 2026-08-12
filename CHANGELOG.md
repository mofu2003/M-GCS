# Changelog

## 0.0.2 - 2026-08-12

### Added

- Docker Compose development environment for Linux-based MgCore builds and Flutter checks.
- Shared MAVLink header generation script pinned to `pymavlink 2.4.49`.
- PowerShell and shell wrappers for the Docker development workflow.
- Flutter analyze/test job and reproducible MAVLink generation in GitHub Actions.
- Updated Chinese architecture and optimization reports.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.2`.
- Isolated generated MAVLink headers in a Docker named volume.
- Made the UDP transport test use an available local port.
- Updated widget tests to assert the current calibration and safety UI contract.

### Notes

- Docker validates shared C++ and Flutter code but does not produce native macOS or Windows desktop bundles.
- Hardware and manual tests remain opt-in and are not part of the default container or CI gate.
- Release verification passed 29 backend offline/simulation tests and 79 Flutter tests; Flutter static analysis reported no issues.
- The Docker Compose definition was validated, but the local image build could not reach Docker Hub authentication and should be repeated on an unrestricted network.
