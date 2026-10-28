# 1. Front Page

# MemForge: A Deterministic Simulator of Operating-System Memory Management

### Academic Project Report

**Institution:** [University]
**Department:** [Department]
**Course:** [Course code and name]
**Project Supervisor:** [Supervisor]
**Date of Submission:** [Submission date: 1 November 2026]

---

## Abstract and System Summary

This report documents the architectural design, formal algorithmic specifications, experimental verification, and empirical evaluation of **MemForge** (simulator binary `memsim`). MemForge is an operating-system memory-management simulator engineered in standard C17 with a companion WebAssembly deployment. The platform models contiguous dynamic-partition allocation (First-Fit, Best-Fit, and Worst-Fit placement, split-on-allocation, immediate coalescing, and explicit sliding compaction) alongside demand-paged virtual memory (per-process single-level page tables, virtual address translation, page-fault resolution, and page replacement under FIFO, LRU, and Optimal algorithms, accompanied by empirical analysis of Belady's anomaly). Every simulation run is strictly deterministic, generating byte-identical execution traces across platforms.

---

## Project Team and Division of Responsibilities

The design, implementation, testing, and analysis of MemForge were divided among ten primary engineering roles (P1 through P10), mirroring the modular architecture defined in the project baseline:

| Role | Responsibility Area | Team Member | Roll Number |
| :--- | :--- | :--- | :--- |
| P1 | Lead Architecture: shared kernel, `Session` facade, trace schema, coordination | [Name] | [Roll number] |
| P2 | Contiguous Engine: memory model, block data structures, split and coalesce | [Name] | [Roll number] |
| P3 | Placement Policies: First-Fit, Best-Fit, and Worst-Fit search algorithms | [Name] | [Roll number] |
| P4 | Fragmentation Analysis and Compaction: metrics, relocation strategies | [Name] | [Roll number] |
| P5 | Paging Subsystem: page tables, frame management, MMU address translation | [Name] | [Roll number] |
| P6 | Page Replacement Policies: FIFO, LRU, OPT algorithms and Belady anomaly analysis | [Name] | [Roll number] |
| P7 | Workloads, Metrics, and Benchmarking: loaders, synthetic generators, experiments | [Name] | [Roll number] |
| P8 | Quality Assurance: formal invariants, acceptance tests, determinism checks | [Name] | [Roll number] |
| P9 | Interfaces: command-line interface (`memsim`) and interactive replay viewers | [Name] | [Roll number] |
| P10 | Infrastructure: build systems, continuous integration, static analysis, report | [Name] | [Roll number] |
