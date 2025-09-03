# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial project structure with CMake build system
- CPU benchmark module with multi-threading support
- Cache latency measurements (L1/L2/L3/Memory)
- Comprehensive reporting system with TXT, JSON, and Markdown formats
- Command-line interface with full argument parsing
- Cross-platform support (Linux/macOS)
- Production-safe testing (temporary files, loopback networking)

### In Development
- Memory benchmark module
- Disk I/O benchmark module  
- Network benchmark module (TCP/UDP)
- IPC shared memory benchmark module
- Integrated system benchmark module

## [0.1.0] - 2025-09-03

### Added
- Initial release with CPU benchmarking capability
- TXT format reporting with ASCII art formatting
- JSON and Markdown output formats
- Basic project structure and build system
- Cross-platform compilation support