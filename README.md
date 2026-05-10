[nitcbase-indexing-README.md](https://github.com/user-attachments/files/27566592/nitcbase-indexing-README.md)
# NITCbase — Indexing

> Part of [NITCbase](https://nitcbase.github.io/) — a Relational Database Management System implementation project by NIT Calicut.

## Overview

This repository contains the implementation of the **B+ Tree indexing layer** of NITCbase — enabling fast indexed search, index creation and deletion, and multi-relation join operations. This is the most algorithmically complex part of the project.

## System Architecture

NITCbase follows an eight-layer design. This repo implements the upper-middle layers:

```
┌──────────────────────────────┐
│      Frontend Interface      │
│        Schema Layer          │
│        Algebra Layer         │
├──────────────────────────────┤  ← This repo (B+ Tree + Join)
│      Block Access Layer      │
│         Cache Layer          │
│        B+ Tree Layer         │
├──────────────────────────────┤
│        Buffer Layer          │
│       Physical Layer         │
└──────────────────────────────┘
```

## Branches

| Branch | Stage | Topic | Hours |
|--------|-------|-------|-------|
| `stage10-bplus-search` | Stage 10 | B+ Tree Search on Relations | 18 hrs |
| `stage11-index-create-delete` | Stage 11 | Index Creation and Deletion | 26 hrs |
| `stage12-join` | Stage 12 | Join on Relations | 10 hrs |

## What's Implemented

### Stage 10 — B+ Tree Search
- Traversing a B+ Tree index to efficiently locate records matching a search key
- Internal node traversal and leaf node record pointer lookup

### Stage 11 — Index Creation and Deletion
- `CREATE INDEX` — building a B+ Tree over an attribute of an existing relation by inserting all current records
- `DROP INDEX` — removing an index and freeing all associated disk blocks
- B+ Tree insertion with node splitting for both leaf and internal nodes

### Stage 12 — Join on Relations
- `SELECT FROM JOIN WHERE` — equi-join across two relations
- Implemented using nested iteration with optional B+ Tree indexed lookup on the inner relation

## Key Concepts

- **B+ Tree Structure** — balanced tree with all data at leaf level, linked leaf nodes for range scans
- **Node Splitting** — maintaining tree balance on insertion by splitting full nodes
- **Indexed vs Sequential Search** — B+ Tree reduces search from O(n) to O(log n)
- **Equi-Join** — combining tuples from two relations on a matching attribute value

## Tech Stack

- **Language**: C++
- **Concepts**: B+ Trees, Indexing, Tree Algorithms, Join Operations, Query Optimization

## Reference

- [NITCbase Official Documentation](https://nitcbase.github.io/)
- [B+ Tree Layer Design](https://nitcbase.github.io/docs/Design/B+%20Tree%20Layer)
- [B+ Trees Tutorial](https://nitcbase.github.io/docs/Misc/B+%20Trees)
- [Indexing in NITCbase](https://nitcbase.github.io/docs/Misc/Indexing)
