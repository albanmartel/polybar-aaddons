# Maintainer: Votre Nom <votre-email@exemple.com>
pkgname=polybar-aaddons
pkgver=1.0
pkgrel=1
pkgdesc="Suite de modules polybar de diagnostic et configurations en C incluant un sélecteur de date pour le presse-papier."
arch=('x86_64')
url="https://github.com/albanmartel/polybar-aaddons"
license=('GPL')
depends=('zenity' 'xclip' 'wl-clipboard' 'libclipboard-git' 'gtk3')
makedepends=('gcc' 'make' 'pkg-config')
source=("votre_archive_source.tar.gz") # Ou git+https://...
sha256sums=('SKIP') # À remplacer par le hash réel pour la sécurité

build() {
    cd "$srcdir"
    # Utilisation du Makefile existant pour compiler
    make
}

package() {
    cd "$srcdir"
    # On installe manuellement ou via le Makefile en adaptant le chemin
    # Note : DESTDIR est utilisé par votre Makefile 
    make DESTDIR="$pkgdir/usr/bin" install
}