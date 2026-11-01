#!/usr/bin/env bash
# Instala devkitPro (devkitA64 + libnx) en Arch/Omarchy, con los pasos oficiales de
# https://devkitpro.org/wiki/devkitPro_pacman. Necesita sudo.
#
# Los repositorios van en un archivo propio (/etc/pacman.d/devkitpro.conf) y pacman.conf
# solo gana una línea Include. Si una actualización de Omarchy reescribe pacman.conf, basta
# con volver a correr este script: no repite lo que ya está hecho.
set -euo pipefail

KEY=BC26F752D25B92CE272E0F44F7FD5492264BB9D0

echo "1/4 llave de firma de devkitPro"
if ! sudo pacman-key --list-keys "$KEY" >/dev/null 2>&1; then
    sudo pacman-key --recv-keys "$KEY" --keyserver keyserver.ubuntu.com
    sudo pacman-key --lsign-key "$KEY"
fi
pacman -Q devkitpro-keyring >/dev/null 2>&1 || sudo pacman -U --noconfirm https://pkg.devkitpro.org/devkitpro-keyring.pkg.tar.zst

echo "2/4 repositorios en /etc/pacman.d/devkitpro.conf"
sudo tee /etc/pacman.d/devkitpro.conf >/dev/null <<'EOF'
[dkp-libs]
Server = https://pkg.devkitpro.org/packages

[dkp-linux]
Server = https://pkg.devkitpro.org/packages/linux/$arch/
EOF
if ! grep -q '^Include = /etc/pacman.d/devkitpro.conf' /etc/pacman.conf; then
    printf '\n# devkitPro (nxrecomp/tools/instalar-devkitpro.sh)\nInclude = /etc/pacman.d/devkitpro.conf\n' |
        sudo tee -a /etc/pacman.conf >/dev/null
fi

echo "3/4 switch-dev (devkitA64, libnx, switch-tools y los ejemplos)"
# Omarchy no deja actualizar el sistema con pacman -Syu directo: eso se hace con
# `omarchy update` (snapshot, migraciones). Correrlo ANTES de este script deja las bases
# de datos al día, y aquí solo se instalan los paquetes de devkitPro.
sudo pacman -S --needed --noconfirm switch-dev

echo "4/4 variables de entorno"
if [ -f /etc/profile.d/devkit-env.sh ]; then
    echo "  listas en /etc/profile.d/devkit-env.sh (se cargan al abrir una sesión nueva)"
fi
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc --version | head -1
echo "listo"
