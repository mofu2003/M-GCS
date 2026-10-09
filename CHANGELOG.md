# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.1] - 2026-10-09

### Added
- Multi-agent collaboration framework with roles for Claude, Codex, DeepSeek, and WorkBuddy
- Pragmatic on-demand workflow in `docs/workflow/COLLABORATION.md`
- Comprehensive constraint documentation with three-level priority (MUST/SHOULD/INFO)
- `docs/constraints/`: structured constraint files replacing scattered documentation
- `docs/adr/`: migrated architectural decision records 0001-0004
- `CLAUDE.md`: entry point for Claude Code automatic loading

### Changed
- Restructured documentation hierarchy: removed scattered planning and research docs
- Consolidated upstream reference rules into single source of truth
- Improved constraint clarity with explicit severity levels
- Updated `AGENTS.md` with rule hierarchy and workflow entry point

### Removed
- Legacy `CHANGELOG.md` with 1850 lines of outdated entries
- Scattered README files in `bridge/`, `core/`, `flutter_shell/`
- Old architecture documentation structure under `docs/architecture/`
- Planning documents that were superseded by constraint system
- Research documents that were consolidated into ADRs

## [0.0.80] - 2026-10-08

Previous releases tracked in git history.
