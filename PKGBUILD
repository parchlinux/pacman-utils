# Maintainer: Parch GNU/Linux Project <contact@parchlinux.com>
# Copyright (C) 2026 Parch GNU/Linux Project <https://parchlinux.com>

pkgname=pacu
pkgver=1.0.0
pkgrel=1
pkgdesc="Advanced, high-performance Pacman utilities suite for Parch GNU/Linux"
arch=('x86_64' 'aarch64' 'riscv64')
url="https://parchlinux.com"
license=('AGPL-3.0-or-later')
depends=('pacman' 'curl' 'glibc')
makedepends=('meson' 'ninja' 'pkgconf' 'gcc')
provides=('pacman-utils' 'pu')
conflicts=('pacman-utils')
replaces=('pacman-utils')
source=("$pkgname-$pkgver.tar.gz::https://github.com/parchlinux/pacu/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  arch-meson "$pkgname-$pkgver" build \
    -Dbuildtype=release \
    -Db_ndebug=true \
    -Dman=true
  ninja -C build
}

check() {
  ninja -C build test
}

package() {
  DESTDIR="$pkgdir" ninja -C build install
  install -Dm644 "$srcdir/$pkgname-$pkgver/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
