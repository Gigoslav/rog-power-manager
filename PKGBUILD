# Maintainer: Marcel Berghaus (Gigoslav) <berghaus.marcel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

pkgname=rog-power-manager
pkgver=0.4.3
pkgrel=1
pkgdesc="ASUS ROG Strix Power Manager, Fan Control (silent/balanced/turbo/max), 60/240Hz switcher, live Wattage tray badge & GPU/MUX switcher (Native C++ / Qt6)"
arch=('x86_64')
url="https://github.com/Gigoslav/rog-power-manager"
license=('GPL-3.0-or-later')
depends=('qt6-base' 'power-profiles-daemon' 'libnotify')
makedepends=('cmake' 'gcc')
optdepends=(
  'supergfxctl: GPU mode switching (Integrated / Hybrid / AsusMuxDgpu)'
  'kscreen: Wayland/X11 display refresh rate switching via kscreen-doctor'
  'xorg-xrandr: X11 fallback refresh rate switching'
  'iw: Wi-Fi power-save toggling on battery'
)
provides=('rog-power-manager' 'rog-auto-power' 'rog-auto-power-git')
conflicts=('rog-auto-power')
replaces=('rog-auto-power' 'rog-auto-power-git')

build() {
  cd "$startdir"
  cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j$(nproc)
}

package() {
  cd "$startdir"
  DESTDIR="$pkgdir" make install
}
