# Parch GNU/Linux - `pacu` Reference Manual

**Copyright (C) 2026 Parch GNU/Linux Project**  
**License:** GNU Affero General Public License v3.0 or later (AGPL-3.0-or-later)

---

## 📖 Table of Contents
1. [Introduction](#introduction)
2. [Installation & Requirements](#installation--requirements)
3. [Command Reference](#command-reference)
   - [edit-sources (pacedit)](#edit-sources-pacedit)
   - [repo (pacrepo)](#repo-pacrepo)
   - [hold, unhold, show-held](#hold-unhold-show-held)
   - [mark](#mark)
   - [why & deptree (pacwhy, pacdeptree)](#why--deptree)
   - [doctor (pacdoctor)](#doctor-pacdoctor)
   - [autoremove (pacorphans)](#autoremove-pacorphans)
   - [cache (pacclean)](#cache-pacclean)
   - [check-updates](#check-updates)
   - [search-file](#search-file)
   - [history](#history)
   - [top-sizes](#top-sizes)
   - [verify](#verify)
   - [diff](#diff)
   - [rank-mirrors](#rank-mirrors)
   - [keys](#keys)
   - [news](#news)
4. [Architecture & Design](#architecture--design)
5. [Building from Source](#building-from-source)
6. [Packaging with PKGBUILD](#packaging-with-pkgbuild)

---

## Introduction

`pacu` (short alias `pu`) is a native C package management utility suite engineered specifically for **Parch GNU/Linux** and Arch-based distributions. It fills the gap between low-level `pacman` and everyday administrative workflows by providing high-level ergonomic tools inspired by Debian/Ubuntu's `apt` suite, `apt-mark`, and Fedora's `dnf`, implemented with zero interpreter overhead in C11 with direct `libalpm` bindings.

---

## Installation & Requirements

### Runtime Dependencies
- `glibc` (>= 2.38)
- `libalpm` (>= 13.0)
- `curl` (>= 8.0)
- `pacman`

### Build Dependencies
- `meson` (>= 0.60)
- `ninja`
- `gcc` or `clang`
- `pkgconf`

---

## Command Reference

### `edit-sources` (`pacedit`)
Safely opens `/etc/pacman.conf`, `/etc/pacman.d/mirrorlist`, `/etc/pacman.d/parch-mirrors`, or any dynamically included repository mirrorlist in your configured `$VISUAL` or `$EDITOR`.
- **Atomic staging:** Files are edited in a temporary copy in `/tmp/`.
- **Syntax validation:** Before committing changes back to `/etc/pacman.conf`, `pacu` verifies configuration structure and syntax using `libalpm`.
- **Automatic backup:** A `.bak` backup copy is saved before modifying the destination.

```bash
# Interactive selection of all discovered sources
pacu edit-sources

# Direct edit
sudo pacu edit-sources mirrorlist
sudo pacu edit-sources parch
sudo pacu edit-sources pacman.conf
```

---

### `repo` (`pacrepo`)
Inspect and manage repository configuration blocks.
```bash
# List all repositories with their active/disabled status and SigLevel
pacu repo list

# Enable or disable a repository
sudo pacu repo enable extra-testing
sudo pacu repo disable core-testing

# Add a new repository stanza
sudo pacu repo add custom-repo --server https://repo.example.com/$arch --siglevel Optional

# Remove a repository
sudo pacu repo remove custom-repo
```

---

### `hold`, `unhold`, `show-held`
Manage package upgrade locks without manual configuration editing (equivalent to `apt-mark hold`/`unhold`).
```bash
# Prevent packages from being upgraded during system updates
sudo pacu hold linux nvidia cuda

# List all currently held packages
pacu show-held

# Resume upgrading packages
sudo pacu unhold linux
```

---

### `mark`
Change installation reasons in the ALPM database.
```bash
# Mark a package as manually/explicitly installed
sudo pacu mark as-explicit ripgrep

# Mark a package as an automatically installed dependency
sudo pacu mark as-dep libuv
```

---

### `why` & `deptree`
Explain package presence and inspect dependency trees.
```bash
# Trace why an installed package is present on your system
pacu why unibilium

# Visualize dependency hierarchy
pacu deptree neovim --depth 3

# Visualize reverse dependency hierarchy (what depends on this package)
pacu deptree -r openssl
```

---

### `doctor` (`pacdoctor`)
System health and consistency analyzer.
Checks:
1. Active pacman database locks (`/var/lib/pacman/db.lck`)
2. Local ALPM database readability and integrity
3. Orphan package accumulation and reclaimable space
4. Unmerged `.pacnew` and `.pacsave` configuration files
5. Partition free space on `/`, `/boot`, and `/var/cache/pacman/pkg`
6. Cache size metrics

```bash
pacu doctor
```

---

### `autoremove` (`pacorphans`)
Finds unneeded packages installed as dependencies that have no dependants, calculates the complete orphan tree, and offers clean removal.
```bash
# Dry run simulation
pacu autoremove --dry-run

# Interactive removal
sudo pacu autoremove
```

---

### `cache` (`pacclean`)
Manage package tarballs stored in `/var/cache/pacman/pkg`.
```bash
# Inspect total cache size and package counts
pacu cache status

# Keep latest 2 versions and purge older packages
sudo pacu cache clean --keep 2

# Remove all cached versions of uninstalled packages
sudo pacu cache clean --uninstalled
```

---

### `check-updates`
Safely check for pending upgrades in an isolated temporary sandbox without locking the system database.
```bash
# Formatted update table with download sizes
pacu check-updates

# Quiet output for scripts / polybar / waybar modules
pacu check-updates --quiet
```

---

### `search-file`
Fast search for which package owns a specific binary or library file.
```bash
pacu search-file bin/zsh
pacu search-file libssl.so
```

---

### `history`
Colorized viewer for `/var/log/pacman.log`.
```bash
# View last 25 transactions
pacu history -n 25

# Filter history for a specific package
pacu history neovim
```

---

### `top-sizes`
Lists installed packages sorted by disk footprint.
```bash
pacu top-sizes -n 20
```

---

### `verify`
Verifies package file integrity and detects missing files against ALPM database records.
```bash
# Verify a single package
pacu verify glibc

# Verify all installed packages
pacu verify --all
```

---

### `diff`
Recursively scans `/etc` for `.pacnew` and `.pacsave` files and provides an interactive TUI to view diffs, launch merge tools (`vimdiff`, `meld`), replace, or delete.
```bash
sudo pacu diff
```

---

### `rank-mirrors`
Benchmark and rank mirror download speeds via concurrent C HTTP/HTTPS requests.
```bash
# Benchmark and output top 10 fastest mirrors
sudo pacu rank-mirrors --top 10 --output /etc/pacman.d/mirrorlist
```

---

### `keys`
Pacman GPG keyring repair and maintenance helper.
```bash
# Reinitialize and populate keyrings
sudo pacu keys fix

# Refresh keys from keyservers
sudo pacu keys refresh

# Complete reset of corrupted keyring
sudo pacu keys reset
```

---

### `news`
Fetches and displays distribution news and critical update notices.
```bash
pacu news -n 5
```

---

## Building from Source

```bash
git clone https://github.com/parchlinux/pacu.git
cd pacu
meson setup build --buildtype=release
ninja -C build
ninja -C build test
sudo ninja -C build install
```

---

## Packaging with PKGBUILD

```bash
makepkg -sirc
```

---

**Parch GNU/Linux Project 2026**
