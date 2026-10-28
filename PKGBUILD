# Maintainer: Marcel Berghaus (Gigoslav) <berghaus.marcel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

pkgname=rog-power-manager
pkgver=0.3.0
pkgrel=1
pkgdesc="ASUS ROG Strix auto power manager, fan control (silent/balanced/turbo/max), 60/240Hz switcher, live Wattage tray badge & GPU/MUX switcher"
arch=('any')
url="https://github.com/Gigoslav/rog-power-manager"
license=('GPL-3.0-or-later')
depends=('python' 'python-pyqt6' 'python-dbus' 'power-profiles-daemon' 'libnotify')
optdepends=(
  'supergfxctl: GPU mode switching (Integrated / Hybrid / AsusMuxDgpu)'
  'kscreen: Wayland/X11 display refresh rate switching via kscreen-doctor'
  'xorg-xrandr: X11 fallback refresh rate switching'
  'iw: Wi-Fi power-save toggling on battery'
)
provides=('rog-power-manager' 'rog-auto-power' 'rog-auto-power-git')
conflicts=('rog-auto-power')
replaces=('rog-auto-power' 'rog-auto-power-git')

package() {
  cd "$startdir"
  make DESTDIR="$pkgdir" PREFIX=/usr install
}
