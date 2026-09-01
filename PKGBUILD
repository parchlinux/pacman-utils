# Maintainer: Parch GNU/Linux Project <contact@parchlinux.com>
# Copyright (C) 2026 Parch GNU/Linux Project <https://parchlinux.com>

pkgname=pacu
pkgver=1.0.0.r0.g0000000
pkgrel=1
pkgdesc="Advanced, high-performance Pacman utilities suite for Parch GNU/Linux"
arch=('x86_64' 'aarch64' 'riscv64')
url="https://parchlinux.com"
license=('AGPL-3.0-or-later')
depends=('pacman' 'curl' 'glibc')
makedepends=('git' 'meson' 'ninja' 'pkgconf' 'gcc')
provides=('pacman-utils' 'pu')
conflicts=('pacman-utils')
replaces=('pacman-utils')
source=("git+https://github.com/parchlinux/pacman-utils.git")
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/pacman-utils"
  if git describe --tags &>/dev/null; then
    git describe --long --tags --abbrev=7 | sed 's/^v//;s/\([^-]*-g\)/r\1/;s/-/./g'
  else
    printf "1.0.0.r%s.g%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short=7 HEAD)"
  fi
}

build() {
  arch-meson "$srcdir/pacman-utils" build \
    -Dman=true
  ninja -C build
}

check() {
  ninja -C build test
}

package() {
  DESTDIR="$pkgdir" ninja -C build install
  install -Dm644 "$srcdir/pacman-utils/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
