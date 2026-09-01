# pacu (Parch / Pacman Utilities)

A fast, lightweight, and robust C utility suite designed for **Parch Linux** (and Arch Linux derivatives). Inspired by Debian/Ubuntu's `apt` utilities, `apt-mark`, and Fedora's `dnf`, powered natively by `libalpm` and modern POSIX C.

## Quick Start

You can invoke commands using `pacu` or the ultra-short alias `pu`:

```bash
# Safely edit repository sources in $EDITOR with syntax validation
pacu edit-sources

# Manage repositories
pacu repo list
pacu repo enable testing
pacu repo add myrepo --server https://example.com/repo/$arch

# Package hold / version lock (apt-mark hold equivalent)
pacu hold linux nvidia
pacu show-held
pacu unhold linux

# Explain why a package is installed
pacu why libtermkey

# View dependency tree
pacu deptree neovim

# Diagnose system health
pacu doctor

# Clean cache and remove unneeded orphans
pacu cache status
pacu cache clean --keep 2
pacu autoremove

# Check updates safely without db locks
pacu check-updates
```

## Available Subcommands

| Subcommand | Alias Shortcut | Description |
| :--- | :--- | :--- |
| `edit-sources` | `pacedit` | Safely edit `/etc/pacman.conf` and mirrorlists with atomic backup and validation |
| `repo` | `pacrepo` | Repository manager (`list`, `add`, `remove`, `enable`, `disable`) |
| `hold` / `unhold` / `show-held` | | Hold and unhold packages to manage `IgnorePkg` |
| `mark` | | Change install reasons (`as-explicit` / `as-dep`) |
| `autoremove` | `pacorphans` | Find and clean unneeded dependency orphan trees |
| `cache` | `pacclean` | Inspect cache disk usage, retain *N* versions, clean uninstalled |
| `check-updates` | | Non-locking update checking in a temporary sandbox |
| `why` | `pacwhy` | Trace why an installed package is present |
| `deptree` | | Render Unicode / ASCII dependency trees (with `-r` reverse mode) |
| `search-file` | | Locate which package owns a specific file or binary |
| `history` | | Inspect formatted `/var/log/pacman.log` transaction history |
| `top-sizes` | | List installed packages sorted by disk footprint |
| `doctor` | `pacdoctor` | Full system health check (locks, broken DBs, keyring, `.pacnew` files) |
| `verify` | | Check file integrity and missing files against ALPM databases |
| `diff` | | Find and interactively merge `.pacnew` and `.pacsave` files |
| `rank-mirrors` | | Benchmark and rank mirror speeds via concurrent C HTTP/HTTPS requests |
| `keys` | | Repair and manage pacman GPG keyring (`fix`, `refresh`, `reset`) |
| `news` | | Fetch distribution announcements and critical update alerts |

## Build & Installation

### Requirements
- `meson`
- `ninja`
- `gcc` or `clang`
- `libalpm`
- `libcurl`

### Build
```bash
meson setup build
ninja -C build
```

### Test
```bash
ninja -C build test
```

### Install
```bash
sudo ninja -C build install
```

This installs `pacu` along with symlinks: `pu`, `pacman-utils`, `pacedit`, `pacrepo`, `pacdoctor`, `pacwhy`, `pacclean`, and `pacorphans`.

## License
GPL-3.0-or-later
