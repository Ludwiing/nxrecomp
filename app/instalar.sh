#!/bin/sh
# Agrega NX2Linux al menú de aplicaciones (Walker en Omarchy) y como orden `nx2linux`.
set -e
APP="$(cd "$(dirname "$0")" && pwd)"
ICONOS="$HOME/.local/share/icons/hicolor/scalable/apps"
mkdir -p "$HOME/.local/bin" "$HOME/.local/share/applications" "$ICONOS"
cp "$APP/io.github.ludwiing.NX2Linux.svg" "$ICONOS/"
gtk-update-icon-cache -q -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
# Descompresor ZBIC para los NSO del sistema 22.0.0+ y de Switch 2 (ver recomp/nso.py).
PYTHONPATH="$APP/../.pydeps" python3 -c "import zbic" 2>/dev/null || make -s -C "$APP/.." pydeps >/dev/null 2>&1 || \
    echo "aviso: no se pudo instalar zbic (make pydeps); los juegos con NSO en ZBIC no van a cargar"
ln -sf "$APP/nx2linux.sh" "$HOME/.local/bin/nx2linux"
cat > "$HOME/.local/share/applications/io.github.ludwiing.NX2Linux.desktop" <<EOD
[Desktop Entry]
Type=Application
Name=NX2Linux
Comment=Tus juegos de Switch como programas nativos de Linux
Exec=$APP/nx2linux.sh
Icon=io.github.ludwiing.NX2Linux
Categories=Game;
Terminal=false
StartupWMClass=io.github.ludwiing.NX2Linux
EOD
echo "listo: búscalo como NX2Linux en el lanzador, o corre: nx2linux"
