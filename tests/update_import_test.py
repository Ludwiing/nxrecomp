#!/usr/bin/env python3
"""Importa un snapshot Switch 2 extraído como actualización de un juego ya agregado."""
import os
import sys
import tempfile
from unittest.mock import patch

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "app"))
from nx2linux import motor  # noqa: E402


def crear_extraido(ruta):
    exefs = os.path.join(ruta, "exefs")
    os.makedirs(exefs, exist_ok=True)
    with open(os.path.join(exefs, "main"), "wb") as f:
        f.write(b"NSO0" + bytes(32))
    with open(os.path.join(exefs, "main.npdm"), "wb") as f:
        f.write(b"npdm")
    os.makedirs(os.path.join(ruta, "romfs"), exist_ok=True)


def main():
    title = "04008e001f098000"
    with tempfile.TemporaryDirectory(prefix="nx2linux-update-test-") as tmp:
        base = os.path.join(tmp, "base")
        update_root = os.path.join(tmp, "Nintendo Switch Sports Update v1.1.0")
        snapshot = os.path.join(update_root, "65536")
        crear_extraido(base)
        crear_extraido(snapshot)

        game = motor.Juego(os.path.join(tmp, title))
        game.datos.update({"nombre": "prueba", "origen": {"base": base, "update": None, "dlc": []}})
        game.guardar()

        with patch.object(motor, "llaves_de", return_value="test.keys"), \
             patch.object(motor, "title_id", return_value=int(title, 16)):
            contenido = motor.clasificar_contenido(game, [update_root])
        assert contenido["update"] == snapshot, contenido
        assert not contenido["dlc"], contenido
        assert motor.aplicar_contenido(game, contenido) == "extraer"
        assert game.datos["origen"]["update"] == snapshot

        # Al reconstruir desde una base extraída, debe consumir el snapshot actualizado,
        # no volver silenciosamente al ExeFS/RomFS de la versión base.
        c = object.__new__(motor.Construccion)
        c.juego = game
        with patch.object(motor, "motor_especial", return_value=None), \
             patch.object(motor.Construccion, "_extraer_extraido", return_value=None) as extraer:
            c._extraer()
        extraer.assert_called_once_with(snapshot)

        # Una carpeta base normal con el mismo Program ID no debe aceptarse como update.
        duplicado = os.path.join(tmp, "copia-base")
        crear_extraido(duplicado)
        try:
            with patch.object(motor, "llaves_de", return_value="test.keys"), \
                 patch.object(motor, "title_id", return_value=int(title, 16)):
                motor.clasificar_contenido(game, [duplicado])
        except RuntimeError as e:
            assert "juego base" in str(e), e
        else:
            raise AssertionError("se aceptó una copia de la carpeta base como actualización")

    print("OK: snapshot de update Switch 2 reconocido y usado al reconstruir")


if __name__ == "__main__":
    main()
