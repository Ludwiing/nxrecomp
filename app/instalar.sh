#!/bin/sh
# Agrega NX2Linux al menú de aplicaciones (Walker en Omarchy) y como orden `nx2linux`.
set -e
APP="$(cd "$(dirname "$0")" && pwd)"
ICONOS="$HOME/.local/share/icons/hicolor/scalable/apps"
mkdir -p "$HOME/.local/bin" "$HOME/.local/share/applications" "$ICONOS"
cp "$APP/io.github.ludwiing.NX2Linux.svg" "$ICONOS/"
gtk-update-icon-cache -q -t "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
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
