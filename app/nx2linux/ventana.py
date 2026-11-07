"""La ventana de NX2Linux (GTK4 + libadwaita): biblioteca, agregar un juego, construirlo y jugarlo."""

import os
import subprocess
import threading

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
from gi.repository import Adw, Gdk, GdkPixbuf, Gio, GLib, Gtk, Pango  # noqa: E402

from . import motor  # noqa: E402

CSS = b"""
.portada { border-radius: 12px; }
.tarjeta-juego { padding: 10px; border-radius: 16px; }
.tarjeta-juego:hover { background: alpha(currentColor, 0.07); }
.estado { font-size: 0.85em; }
.registro { font-family: monospace; font-size: 0.85em; }
"""


def portada(juego, tam):
    """El ícono del juego a `tam` píxeles, con esquinas redondeadas; si no hay, un ícono genérico."""
    if juego.icono:
        try:
            pb = GdkPixbuf.Pixbuf.new_from_file_at_scale(juego.icono, tam, tam, True)
            img = Gtk.Picture.new_for_paintable(Gdk.Texture.new_for_pixbuf(pb))
            img.set_size_request(tam, tam)
            img.set_can_shrink(True)
            img.add_css_class("portada")
            img.set_overflow(Gtk.Overflow.HIDDEN)
            return img
        except GLib.Error:
            pass
    img = Gtk.Image.new_from_icon_name("applications-games-symbolic")
    img.set_pixel_size(tam // 2)
    img.set_size_request(tam, tam)
    img.add_css_class("card")
    img.add_css_class("portada")
    return img


def texto_estado(juego, construyendo):
    if construyendo:
        return "Construyendo…"
    if juego.listo:
        return "Listo para jugar"
    hechos = sum(juego.paso_hecho(k) for k, _, _ in motor.PASOS)
    return f"Sin terminar ({hechos} de {len(motor.PASOS)} pasos)" if hechos else "Sin construir"


class Ventana(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app, title="NX2Linux", default_width=1100, default_height=760)
        prov = Gtk.CssProvider()
        prov.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_display(Gdk.Display.get_default(), prov,
                                                  Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.construcciones = {}   # id de juego -> Construccion en curso
        self.partidas = {}         # id de juego -> proceso del juego
        self.detalle = None        # PaginaJuego visible

        self.nav = Adw.NavigationView()
        self.toasts = Adw.ToastOverlay(child=self.nav)
        self.set_content(self.toasts)
        self.nav.add(self._pagina_biblioteca())
        self.nav.connect("popped", lambda *_: setattr(self, "detalle", None))
        self.refrescar()
        GLib.idle_add(self._primera_vez)

    # ---------- biblioteca ----------

    def _pagina_biblioteca(self):
        tv = Adw.ToolbarView()
        hb = Adw.HeaderBar()
        agregar = Gtk.Button(icon_name="list-add-symbolic", tooltip_text="Agregar un juego")
        agregar.add_css_class("suggested-action")
        agregar.connect("clicked", lambda *_: self.agregar_juego())
        hb.pack_start(agregar)
        menu = Gio.Menu()
        menu.append("Preferencias", "app.preferencias")
        menu.append("Copiar diagnóstico", "app.diagnostico")
        menu.append("Acerca de NX2Linux", "app.acerca")
        hb.pack_end(Gtk.MenuButton(icon_name="open-menu-symbolic", menu_model=menu))
        self.busqueda = Gtk.SearchEntry(placeholder_text="Buscar en la biblioteca")
        self.busqueda.connect("search-changed", lambda *_: self.flow.invalidate_filter())
        hb.set_title_widget(self.busqueda)
        tv.add_top_bar(hb)

        self.flow = Gtk.FlowBox(selection_mode=Gtk.SelectionMode.NONE, homogeneous=True, max_children_per_line=8,
                                min_children_per_line=2, row_spacing=12, column_spacing=12,
                                margin_top=18, margin_bottom=18, margin_start=18, margin_end=18,
                                valign=Gtk.Align.START)
        self.flow.set_filter_func(self._filtro)
        self.flow.connect("child-activated", lambda _f, hijo: self.abrir_juego(hijo.juego))
        self.vacio = Adw.StatusPage(icon_name="applications-games-symbolic", title="Tu biblioteca está vacía",
                                    description="Agrega la copia de un juego tuyo (NSP, NSZ, XCI o XCZ), con su "
                                                "actualización y DLC si los tienes. NX2Linux lo convierte en un "
                                                "programa de Linux que corre nativo, sin emulador.")
        b = Gtk.Button(label="Agregar un juego", halign=Gtk.Align.CENTER)
        b.add_css_class("pill")
        b.add_css_class("suggested-action")
        b.connect("clicked", lambda *_: self.agregar_juego())
        self.vacio.set_child(b)
        self.pila = Gtk.Stack()
        self.pila.add_named(self.vacio, "vacio")
        self.pila.add_named(Gtk.ScrolledWindow(child=self.flow, hscrollbar_policy=Gtk.PolicyType.NEVER), "juegos")
        tv.set_content(self.pila)
        return Adw.NavigationPage(title="Biblioteca", child=tv, tag="biblioteca")

    def _filtro(self, hijo):
        q = self.busqueda.get_text().strip().lower()
        return not q or q in hijo.juego.nombre.lower()

    def refrescar(self):
        while (h := self.flow.get_first_child()) is not None:
            self.flow.remove(h)
        juegos = motor.biblioteca()
        for j in juegos:
            caja = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
            caja.add_css_class("tarjeta-juego")
            caja.append(portada(j, 160))
            nombre = Gtk.Label(label=j.nombre, wrap=True, max_width_chars=18, justify=Gtk.Justification.CENTER,
                               lines=2, ellipsize=Pango.EllipsizeMode.END)
            nombre.add_css_class("heading")
            caja.append(nombre)
            est = Gtk.Label(label=texto_estado(j, j.id in self.construcciones))
            est.add_css_class("dim-label")
            est.add_css_class("estado")
            caja.append(est)
            hijo = Gtk.FlowBoxChild(child=caja)
            hijo.juego = j
            self.flow.append(hijo)
        self.pila.set_visible_child_name("juegos" if juegos else "vacio")

    def toast(self, texto):
        self.toasts.add_toast(Adw.Toast(title=texto, timeout=4))

    # ---------- primera vez: llaves y dependencias ----------

    def _primera_vez(self):
        faltan = [d for d in motor.dependencias() if not d[1]]
        if not motor.ruta_llaves() or faltan:
            self.preferencias()
        return False

    def preferencias(self):
        Preferencias(self).present(self)

    # ---------- agregar ----------

    def agregar_juego(self):
        llaves = motor.llaves_conocidas()
        d = Adw.AlertDialog(heading="Agregar un juego",
                            body="Elige la carpeta con el juego (y su actualización y DLC), o los archivos "
                                 "sueltos: NSP, NSZ, XCI, XCZ, NCA, NRO, una carpeta de NCA o un juego ya "
                                 "extraído (con su ExeFS). Tienen que ser copias de tus propios juegos." +
                                 ("" if llaves else "\n\nSin prod.keys solo se pueden agregar homebrew NRO y "
                                                    "juegos ya extraídos; elígelo en Preferencias para lo demás."))
        selector = None
        if llaves:
            # Las llaves con que se abre este juego (por ejemplo, las de otra consola); el juego las recuerda.
            selector = Gtk.DropDown.new_from_strings([abreviar(r) for r in llaves])
            selector.set_tooltip_text("El prod.keys de la consola de la que sacaste este juego")
            caja = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
            caja.append(Gtk.Label(label="Llaves de la consola (prod.keys)", xalign=0, css_classes=["dim-label"]))
            caja.append(selector)
            d.set_extra_child(caja)
        d.add_response("cancelar", "Cancelar")
        d.add_response("archivos", "Elegir archivos")
        d.add_response("carpeta", "Elegir carpeta")
        d.set_response_appearance("carpeta", Adw.ResponseAppearance.SUGGESTED)
        d.set_default_response("carpeta")

        def resp(_d, r):
            elegidas = llaves[selector.get_selected()] if selector else None
            self._pedir_archivos(r, lambda rutas: self._elegido(rutas, elegidas))
        d.connect("response", resp)
        d.present(self)

    def _pedir_archivos(self, modo, listo):
        """Abre el selector de carpeta ("carpeta") o de archivos ("archivos") y pasa las rutas elegidas a listo()."""
        def tomar(obtener):
            try:
                rutas = [g.get_path() for g in obtener() if g and g.get_path()]
            except GLib.Error:
                return  # cancelado
            if rutas:
                listo(rutas)
        fd = Gtk.FileDialog(title="Juego de Switch")
        if modo == "carpeta":
            fd.select_folder(self, None, lambda f, res: tomar(lambda: [f.select_folder_finish(res)]))
        elif modo == "archivos":
            filtro = Gtk.FileFilter(name="Juegos de Switch")
            for e in motor.EXTENSIONES:
                filtro.add_suffix(e[1:])
            lista = Gio.ListStore.new(Gtk.FileFilter)
            lista.append(filtro)
            fd.set_filters(lista)
            fd.open_multiple(self, None, lambda f, res: tomar(lambda: list(f.open_multiple_finish(res))))

    def _elegido(self, rutas, llaves):
        self.toast("Reconociendo los archivos…")

        def trabajo():
            try:
                origen = motor.clasificar(rutas, llaves)
                origen["llaves"] = llaves
                GLib.idle_add(self._confirmar, origen)
            except Exception as e:
                GLib.idle_add(self._error, "No se pudo agregar", str(e))
        threading.Thread(target=trabajo, daemon=True).start()

    def _confirmar(self, origen):
        partes = [f"Juego: {os.path.basename(origen['base'].rstrip(os.sep))}"]
        partes.append(f"Actualización: {os.path.basename(origen['update'])}" if origen["update"]
                      else "Sin actualización")
        if origen["dlc"]:
            partes.append(f"Contenido descargable: {len(origen['dlc'])}")
        existe = os.path.isdir(os.path.join(motor.carpeta_juegos(), origen["title"]))
        d = Adw.AlertDialog(heading="¿Construir este juego?",
                            body="\n".join(partes) + ("\n\nYa está en la biblioteca: se vuelve a construir con "
                                                      "estos archivos." if existe else "") +
                            "\n\nLa primera construcción puede tardar bastante y ocupa unas dos veces el tamaño "
                            "del juego en disco.")
        d.add_response("cancelar", "Cancelar")
        d.add_response("construir", "Construir")
        d.set_response_appearance("construir", Adw.ResponseAppearance.SUGGESTED)

        def resp(_d, r):
            if r == "construir":
                j = motor.nuevo_juego(origen, origen.get("llaves"))
                self.refrescar()
                self.abrir_juego(j)
                self.construir(j)
        d.connect("response", resp)
        d.present(self)

    def _error(self, titulo, texto):
        d = Adw.AlertDialog(heading=titulo, body=texto)
        d.add_response("ok", "Entendido")
        d.present(self)

    # ---------- un juego ----------

    def abrir_juego(self, juego):
        self.detalle = PaginaJuego(self, juego)
        self.nav.push(self.detalle)

    def construir(self, juego, desde=None, solo_dlc=None):
        if juego.id in self.construcciones:
            return

        def avisar(paso, frac, linea):
            GLib.idle_add(self._aviso, juego.id, paso, frac, linea)
        c = motor.Construccion(juego, avisar, desde, solo_dlc)
        self.construcciones[juego.id] = c
        c.empezar()
        self.refrescar()
        if self.detalle and self.detalle.juego.id == juego.id:
            self.detalle.actualizar()

    def _aviso(self, jid, paso, frac, linea):
        if paso in ("fin", "error"):
            c = self.construcciones.pop(jid, None)
            nombre = c.juego.nombre if c else jid
            if c:
                c.juego.cargar()
            self.toast(f"{nombre}: listo para jugar" if paso == "fin" else f"{nombre}: la construcción se detuvo")
            self.refrescar()
        if self.detalle and self.detalle.juego.id == jid:
            self.detalle.aviso(paso, frac, linea)
        return False

    def jugar(self, juego):
        p = self.partidas.get(juego.id)
        if p and p.poll() is None:
            self.toast("Ya está abierto")
            return
        try:
            self.partidas[juego.id] = motor.jugar(juego)
        except Exception as e:
            self._error("No se pudo abrir", str(e))
            return
        self.toast(f"Abriendo {juego.nombre}…")
        GLib.timeout_add_seconds(2, self._vigilar, juego)

    def _vigilar(self, juego):
        p = self.partidas.get(juego.id)
        if not p:
            return False
        rc = p.poll()
        if rc is None:
            return True
        del self.partidas[juego.id]
        if motor.pistas_nuevas(juego) > 0: # saltó a código que no estaba recompilado: recompilar y seguir
            d = Adw.AlertDialog(heading=f"{juego.nombre} necesita recompilarse",
                                body="El juego llegó a código que la recompilación no había visto. NX2Linux "
                                     "anotó dónde; al recompilar lo incluye y el juego pasa de ese punto.")
            d.add_response("despues", "Después")
            d.add_response("recompilar", "Recompilar ahora")
            d.set_response_appearance("recompilar", Adw.ResponseAppearance.SUGGESTED)
            d.connect("response", lambda _d, r: r == "recompilar" and self.construir(juego, desde="recompilar"))
            d.present(self)
            return False
        if rc not in (0, -15):
            d = Adw.AlertDialog(heading=f"{juego.nombre} se cerró con un error",
                                body=f"Código {rc}. Lo último que dijo está en ultima_partida.txt, en la carpeta "
                                     "del juego.")
            d.add_response("ok", "Cerrar")
            d.add_response("ver", "Ver la salida")
            d.connect("response", lambda _d, r: r == "ver" and abrir(os.path.join(juego.carpeta,
                                                                                  "ultima_partida.txt")))
            d.present(self)
        return False


def abreviar(ruta):
    casa = os.path.expanduser("~")
    return "~" + ruta[len(casa):] if ruta.startswith(casa + os.sep) else ruta


def abrir(ruta):
    subprocess.Popen(["xdg-open", ruta], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                     start_new_session=True)


class PaginaJuego(Adw.NavigationPage):
    def __init__(self, ventana, juego):
        super().__init__(title=juego.nombre, tag="juego")
        self.v, self.juego = ventana, juego
        tv = Adw.ToolbarView()
        hb = Adw.HeaderBar()
        menu = Gio.Menu()
        acciones = Gio.SimpleActionGroup()
        for nombre, etiqueta, fn in [
            ("carpeta", "Abrir la carpeta del juego", lambda: abrir(juego.carpeta)),
            ("rehacer", "Construir de nuevo desde cero", lambda: ventana.construir(juego, desde="extraer")),
            ("recompilar", "Recompilar (sin volver a extraer)", lambda: ventana.construir(juego, desde="recompilar")),
            ("contenido", "Agregar actualización o DLC…", self._agregar_contenido),
            ("quitar", "Quitar de la biblioteca", self._quitar),
        ]:
            a = Gio.SimpleAction(name=nombre)
            a.connect("activate", lambda *_a, fn=fn: fn())
            acciones.add_action(a)
            menu.append(etiqueta, "juego." + nombre)
        self.insert_action_group("juego", acciones)
        hb.pack_end(Gtk.MenuButton(icon_name="view-more-symbolic", menu_model=menu))
        tv.add_top_bar(hb)

        caja = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=18, margin_top=24, margin_bottom=24,
                       margin_start=24, margin_end=24)
        cab = Gtk.Box(spacing=24)
        cab.append(portada(juego, 200))
        info = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6, valign=Gtk.Align.CENTER, hexpand=True)
        t = Gtk.Label(label=juego.nombre, xalign=0, wrap=True)
        t.add_css_class("title-1")
        info.append(t)
        self.sub = Gtk.Label(xalign=0)
        self.sub.add_css_class("dim-label")
        info.append(self.sub)
        botones = Gtk.Box(spacing=12, margin_top=12)
        self.b_jugar = Gtk.Button(label="Jugar")
        self.b_jugar.add_css_class("pill")
        self.b_jugar.add_css_class("suggested-action")
        self.b_jugar.connect("clicked", lambda *_: ventana.jugar(juego))
        self.b_construir = Gtk.Button(label="Construir")
        self.b_construir.add_css_class("pill")
        self.b_construir.connect("clicked", self._construir_o_detener)
        b_ajustes = Gtk.Button(label="Ajustes")
        b_ajustes.add_css_class("pill")
        b_ajustes.connect("clicked", lambda *_: AjustesJuego(juego).present(ventana))
        self.b_contenido = Gtk.Button(label="Actualización o DLC")
        self.b_contenido.add_css_class("pill")
        self.b_contenido.set_tooltip_text("Agregar una actualización o contenido descargable de este juego")
        self.b_contenido.connect("clicked", lambda *_: self._agregar_contenido())
        for b in (self.b_jugar, self.b_construir, self.b_contenido, b_ajustes):
            botones.append(b)
        info.append(botones)
        cab.append(info)
        caja.append(cab)

        # Los pasos y el progreso
        self.barra = Gtk.ProgressBar(show_text=True)
        caja.append(self.barra)
        grupo = Adw.PreferencesGroup(title="Construcción")
        self.filas = {}
        for k, nombre, _ in motor.PASOS:
            fila = Adw.ActionRow(title=nombre)
            ic = Gtk.Image()
            fila.add_suffix(ic)
            fila.icono = ic
            grupo.add(fila)
            self.filas[k] = fila
        caja.append(grupo)

        self.buf = Gtk.TextBuffer()
        vista = Gtk.TextView(buffer=self.buf, editable=False, monospace=True, wrap_mode=Gtk.WrapMode.WORD_CHAR,
                             top_margin=8, bottom_margin=8, left_margin=8, right_margin=8)
        vista.add_css_class("registro")
        self.vista = vista
        sw = Gtk.ScrolledWindow(child=vista, min_content_height=220, vexpand=True)
        sw.add_css_class("card")
        exp = Gtk.Expander(label="Registro", child=sw, expanded=juego.id in ventana.construcciones)
        caja.append(exp)
        self.exp = exp
        try:
            with open(juego.registro()) as f:
                self.buf.set_text(f.read()[-20000:])
        except OSError:
            pass
        tv.set_content(Gtk.ScrolledWindow(child=Adw.Clamp(child=caja, maximum_size=900),
                                          hscrollbar_policy=Gtk.PolicyType.NEVER))
        self.set_child(tv)
        self.paso_actual = None
        self.actualizar()

    def actualizar(self):
        j = self.juego
        construyendo = j.id in self.v.construcciones
        partes = [f"Title ID {j.id}"]
        if j.version:
            partes.append(f"versión {j.version}")
        o = j.datos.get("origen", {})
        if o.get("dlc"):
            partes.append(f"{len(o['dlc'])} DLC")
        self.sub.set_label(" · ".join(partes) + " — " + texto_estado(j, construyendo))
        self.b_jugar.set_sensitive(j.listo and not construyendo)
        self.b_contenido.set_sensitive(not construyendo)
        self.b_construir.set_label("Detener" if construyendo else ("Reconstruir" if j.listo else "Construir"))
        self.b_construir.remove_css_class("destructive-action")
        if construyendo:
            self.b_construir.add_css_class("destructive-action")
        for k, fila in self.filas.items():
            hecho = j.paso_hecho(k)
            if construyendo and k == self.paso_actual:
                fila.icono.set_from_icon_name("content-loading-symbolic")
                fila.set_subtitle("En curso…")
            else:
                fila.icono.set_from_icon_name("emblem-ok-symbolic" if hecho else "")
                fila.set_subtitle(j.datos["pasos"].get(k, ""))
        if not construyendo:
            hechos = sum(j.paso_hecho(k) * p for k, _, p in motor.PASOS) / sum(p for _, _, p in motor.PASOS)
            self.barra.set_fraction(hechos)
            self.barra.set_text("Listo" if j.listo else f"{int(hechos * 100)} %")

    def aviso(self, paso, frac, linea):
        if linea:
            fin = self.buf.get_end_iter()
            self.buf.insert(fin, linea + "\n")
            if self.buf.get_line_count() > 4000:
                ini = self.buf.get_start_iter()
                self.buf.delete(ini, self.buf.get_iter_at_line(1000)[1])
            self.vista.scroll_to_iter(self.buf.get_end_iter(), 0, False, 0, 1)
        if paso in ("fin", "error"):
            self.paso_actual = None
            self.juego.cargar()
            if paso == "error":
                self.exp.set_expanded(True)
                self.v._error("La construcción se detuvo", linea or "")
            self.actualizar()
            return
        if frac is not None:
            self.barra.set_fraction(frac)
            self.barra.set_text(f"{int(frac * 100)} %")
        if paso != self.paso_actual:
            self.paso_actual = paso
            self.juego.cargar()
            self.actualizar()

    def _construir_o_detener(self, *_):
        c = self.v.construcciones.get(self.juego.id)
        if c:
            c.cancelar()
        elif self.juego.listo:
            self.v.construir(self.juego, desde="recompilar")
        else:
            self.exp.set_expanded(True)
            self.v.construir(self.juego)

    def _agregar_contenido(self):
        if self.juego.id in self.v.construcciones:
            self.v.toast("Espera a que termine la construcción")
            return
        d = Adw.AlertDialog(heading="Actualización o DLC",
                            body=f"Elige los archivos (NSP, NSZ, XCI) de la actualización o del contenido descargable "
                                 f"de {self.juego.nombre}, o una carpeta que los tenga. Tienen que ser copias tuyas.")
        d.add_response("cancelar", "Cancelar")
        d.add_response("carpeta", "Elegir carpeta")
        d.add_response("archivos", "Elegir archivos")
        d.set_response_appearance("archivos", Adw.ResponseAppearance.SUGGESTED)
        d.set_default_response("archivos")
        d.connect("response", lambda _d, r: self.v._pedir_archivos(r, self._contenido_elegido))
        d.present(self.v)

    def _contenido_elegido(self, rutas):
        self.v.toast("Reconociendo los archivos…")

        def trabajo():
            try:
                nuevo = motor.clasificar_contenido(self.juego, rutas)
                GLib.idle_add(self._confirmar_contenido, nuevo)
            except Exception as e:
                GLib.idle_add(self.v._error, "No se pudo agregar", str(e))
        threading.Thread(target=trabajo, daemon=True).start()

    def _confirmar_contenido(self, nuevo):
        partes = []
        if nuevo["update"]:
            partes.append(f"Actualización: {os.path.basename(nuevo['update'])}\nEl juego se vuelve a construir: "
                          "una actualización cambia su código. Las partidas guardadas se conservan.")
        if nuevo["dlc"]:
            partes.append(f"Contenido descargable: {len(nuevo['dlc'])}" +
                          ("" if nuevo["update"] else "\nSolo se extrae; no hace falta recompilar."))
        d = Adw.AlertDialog(heading="¿Agregar a " + self.juego.nombre + "?", body="\n\n".join(partes))
        d.add_response("cancelar", "Cancelar")
        d.add_response("agregar", "Agregar")
        d.set_response_appearance("agregar", Adw.ResponseAppearance.SUGGESTED)

        def resp(_d, r):
            if r != "agregar":
                return
            try:
                hacer = motor.aplicar_contenido(self.juego, nuevo)
            except Exception as e:
                self.v._error("No se pudo agregar", str(e))
                return
            self.exp.set_expanded(True)
            if hacer == "extraer":
                self.v.construir(self.juego, desde="extraer")
            else:
                self.v.construir(self.juego, solo_dlc=hacer)
        d.connect("response", resp)
        d.present(self.v)

    def _quitar(self):
        d = Adw.AlertDialog(heading=f"¿Quitar {self.juego.nombre}?",
                            body="Se borra lo que NX2Linux construyó, incluidas las partidas guardadas de este "
                                 "juego. Tus archivos originales no se tocan.")
        d.add_response("cancelar", "Cancelar")
        d.add_response("quitar", "Quitar")
        d.set_response_appearance("quitar", Adw.ResponseAppearance.DESTRUCTIVE)

        def resp(_d, r):
            if r == "quitar":
                c = self.v.construcciones.pop(self.juego.id, None)
                if c:
                    c.cancelar()
                motor.quitar(self.juego)
                self.v.nav.pop()
                self.v.refrescar()
        d.connect("response", resp)
        d.present(self.v)


class AjustesJuego(Adw.PreferencesDialog):
    def __init__(self, juego):
        super().__init__(title=f"Ajustes de {juego.nombre}")
        self.juego = juego
        aj = juego.datos["ajustes"]
        pag = Adw.PreferencesPage()
        g = Adw.PreferencesGroup(title="Consola", description="Se aplican la próxima vez que abras el juego.")
        modos = [("base", "En la base (TV, 1080p)"), ("portatil", "Portátil (720p)")]
        self.modo = Adw.ComboRow(title="Modo", model=Gtk.StringList.new([m[1] for m in modos]),
                                 selected=[m[0] for m in modos].index(aj["modo"]) if aj["modo"] in
                                 [m[0] for m in modos] else 0)
        self.modo.connect("notify::selected", lambda *_: self._poner("modo", modos[self.modo.get_selected()][0]))
        g.add(self.modo)
        codigos = [c for c, _ in motor.IDIOMAS]
        self.idioma = Adw.ComboRow(title="Idioma del juego", model=Gtk.StringList.new([n for _, n in motor.IDIOMAS]),
                                   selected=codigos.index(aj["idioma"]) if aj["idioma"] in codigos else 0)
        self.idioma.connect("notify::selected",
                            lambda *_: self._poner("idioma", codigos[self.idioma.get_selected()]))
        g.add(self.idioma)
        pag.add(g)
        g = Adw.PreferencesGroup(title="Ventana y sonido")
        renderers = [("opengl", "OpenGL"), ("vulkan", "Vulkan (Zink)")]
        renderer = Adw.ComboRow(title="API gráfica", subtitle="Zink ejecuta el renderer OpenGL sobre Vulkan.",
            model=Gtk.StringList.new([name for _, name in renderers]),
            selected=1 if aj.get("renderer") == "vulkan" else 0)
        renderer.connect("notify::selected", lambda r, _p: self._poner("renderer", renderers[r.get_selected()][0]))
        g.add(renderer)
        s = Adw.SwitchRow(title="Pantalla completa", active=aj["pantalla_completa"])
        s.connect("notify::active", lambda r, _p: self._poner("pantalla_completa", r.get_active()))
        g.add(s)
        s = Adw.SwitchRow(title="Sonido", active=aj["audio"])
        s.connect("notify::active", lambda r, _p: self._poner("audio", r.get_active()))
        g.add(s)
        pag.add(g)
        g = Adw.PreferencesGroup(title="Avanzado",
                                 description="Variables de entorno extra, una CLAVE=VALOR por línea "
                                             "(por ejemplo NX_TRAZA_GL=1 para depurar).")
        buf = Gtk.TextBuffer(text=aj.get("variables", ""))
        buf.connect("changed", lambda b: self._poner("variables", b.get_text(b.get_start_iter(),
                                                                            b.get_end_iter(), False)))
        tv = Gtk.TextView(buffer=buf, monospace=True, top_margin=8, bottom_margin=8, left_margin=8,
                          right_margin=8, height_request=100)
        tv.add_css_class("card")
        g.add(tv)
        pag.add(g)
        self.add(pag)

    def _poner(self, clave, valor):
        self.juego.datos["ajustes"][clave] = valor
        self.juego.guardar()


class Preferencias(Adw.PreferencesDialog):
    def __init__(self, ventana):
        super().__init__(title="Preferencias")
        self.v = ventana
        pag = Adw.PreferencesPage()
        g = Adw.PreferencesGroup(title="Llaves",
                                 description="El archivo prod.keys sacado de tu propia consola. Solo se lee en tu "
                                             "máquina para abrir tus juegos.")
        self.fila_llaves = Adw.ActionRow(title="prod.keys", subtitle=motor.ruta_llaves() or "Sin elegir")
        b = Gtk.Button(label="Elegir…", valign=Gtk.Align.CENTER)
        b.connect("clicked", self._elegir_llaves)
        self.fila_llaves.add_suffix(b)
        g.add(self.fila_llaves)
        pag.add(g)

        g = Adw.PreferencesGroup(title="Usuario", description="El nombre que ven los juegos en tu perfil.")
        nombre = Adw.EntryRow(title="Nombre", text=motor.leer_config().get("usuario", ""), show_apply_button=True)

        def aplicar(fila):
            cfg = motor.leer_config()
            cfg["usuario"] = fila.get_text().strip()[:32]
            motor.guardar_config(cfg)
        nombre.connect("apply", aplicar)
        g.add(nombre)
        pag.add(g)

        g = Adw.PreferencesGroup(title="Biblioteca")
        fila = Adw.ActionRow(title="Carpeta de los juegos construidos", subtitle=motor.carpeta_juegos())
        b = Gtk.Button(icon_name="folder-open-symbolic", valign=Gtk.Align.CENTER, tooltip_text="Abrir")
        b.connect("clicked", lambda *_: (os.makedirs(motor.carpeta_juegos(), exist_ok=True),
                                         abrir(motor.carpeta_juegos())))
        fila.add_suffix(b)
        g.add(fila)
        pag.add(g)

        self.g_dep = Adw.PreferencesGroup(title="Dependencias")
        pag.add(self.g_dep)
        self.filas_dep = []
        self._dependencias()
        self.add(pag)

    def _dependencias(self):
        for f in self.filas_dep:
            self.g_dep.remove(f)
        self.filas_dep = []
        deps = motor.dependencias()
        for nombre, ok, paquete, para in deps:
            f = Adw.ActionRow(title=nombre, subtitle=f"Para {para} · paquete {paquete}")
            ic = Gtk.Image.new_from_icon_name("emblem-ok-symbolic" if ok else "dialog-warning-symbolic")
            if not ok:
                ic.add_css_class("warning")
            f.add_suffix(ic)
            self.g_dep.add(f)
            self.filas_dep.append(f)
        faltan = [p for _, ok, p, _ in deps if not ok]
        if faltan:
            f = Adw.ActionRow(title="Instalar lo que falta", subtitle=" ".join(faltan))
            b = Gtk.Button(label="Instalar", valign=Gtk.Align.CENTER)
            b.add_css_class("suggested-action")
            b.connect("clicked", lambda *_: self._instalar(faltan, b))
            f.add_suffix(b)
            self.g_dep.add(f)
            self.filas_dep.append(f)

    def _instalar(self, faltan, boton):
        boton.set_sensitive(False)
        boton.set_label("Instalando…")

        def trabajo():
            r = subprocess.run(motor.comando_instalar(faltan), capture_output=True, text=True)
            GLib.idle_add(fin, r)

        def fin(r):
            if r.returncode:
                self.v._error("No se pudo instalar", (r.stderr or r.stdout).strip()[-800:] or
                              f"pacman terminó con código {r.returncode}")
            self._dependencias()
            return False
        threading.Thread(target=trabajo, daemon=True).start()

    def _elegir_llaves(self, *_):
        fd = Gtk.FileDialog(title="Elige tu prod.keys")
        if os.path.isdir(os.path.expanduser("~/.switch")):
            fd.set_initial_folder(Gio.File.new_for_path(os.path.expanduser("~/.switch")))

        def listo(f, res):
            try:
                g = f.open_finish(res)
            except GLib.Error:
                return
            cfg = motor.leer_config()
            cfg["llaves"] = g.get_path()
            motor.guardar_config(cfg)
            self.fila_llaves.set_subtitle(g.get_path())
        fd.open(self.v, None, listo)


class App(Adw.Application):
    def __init__(self):
        super().__init__(application_id="io.github.ludwiing.NX2Linux", flags=Gio.ApplicationFlags.DEFAULT_FLAGS)
        self.ventana = None
        for nombre, fn in [("preferencias", lambda: self.ventana.preferencias()),
                           ("diagnostico", self._diagnostico), ("acerca", self._acerca)]:
            a = Gio.SimpleAction(name=nombre)
            a.connect("activate", lambda *_a, fn=fn: fn())
            self.add_action(a)

    def do_activate(self):
        # El ícono propio, también cuando se corre desde el repositorio sin instalar.
        Gtk.IconTheme.get_for_display(Gdk.Display.get_default()).add_search_path(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        Gtk.Window.set_default_icon_name("io.github.ludwiing.NX2Linux")
        if not self.ventana:
            self.ventana = Ventana(self)
        self.ventana.present()

    def _diagnostico(self):
        lineas = ["NX2Linux — diagnóstico", f"repositorio: {motor.ROOT}"]
        try:
            rev = subprocess.run(["git", "-C", motor.ROOT, "log", "-1", "--format=%h %s"], capture_output=True,
                                 text=True).stdout.strip()
            lineas.append(f"versión: {rev}")
        except OSError:
            pass
        for nombre, ok, paquete, _ in motor.dependencias():
            lineas.append(f"{nombre}: {'sí' if ok else 'NO'} ({paquete})")
        lineas.append(f"llaves elegidas: {'sí' if motor.ruta_llaves() else 'no'}")
        for j in motor.biblioteca():
            lineas.append(f"{j.id} {j.nombre} {j.version}: " + ", ".join(k for k, _, _ in motor.PASOS
                                                                          if j.paso_hecho(k)))
        self.ventana.get_clipboard().set("\n".join(lineas))
        self.ventana.toast("Diagnóstico copiado (sin llaves ni rutas de tus juegos)")

    def _acerca(self):
        d = Adw.AboutDialog(application_name="NX2Linux", developer_name="Ludwiing",
                            application_icon="io.github.ludwiing.NX2Linux", version="0.1",
                            comments="Convierte tus propios juegos de Switch en programas nativos de Linux con "
                                     "nxrecomp: recompilación estática ARM64 → x86-64, sin emulador.",
                            website="https://github.com/Ludwiing/nxrecomp")
        d.present(self.ventana)
