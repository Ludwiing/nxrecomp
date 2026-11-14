"""Censo de cobertura: instrucciones SM86 de un conjunto de archivos -> decodificador y bajada de nxrecomp.
Código propio (MIT).

    python3 tools/sm86_censo.py <carpeta>

Lee los pares (hi, lo) de 64 bits escritos como `(0x<hi>, 0x<lo>)` en los .py de la carpeta (el formato de las
pruebas test_dmsh_*.py de seedds/nxnative, MIT, que traen encodings de shaders de juegos con su texto de nvdisasm) y
cuenta cuántas instrucciones distintas se decodifican y se bajan, con el motivo de las que no."""
import collections, glob, os, re, sys
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(R, "tools"))
import sm86_programa as sp, sm86_baja
par = re.compile(r"\(\s*(0x[0-9a-fA-F]{16})\s*(?:\|[^,()]*)?,\s*(0x[0-9a-fA-F]{16})")
vistos = {}
for f in sorted(glob.glob(os.path.join(sys.argv[1], "*.py"))):
    for n, l in enumerate(open(f), 1):
        for m in par.finditer(l):
            hi, lo = int(m.group(1), 16), int(m.group(2), 16)
            vistos.setdefault((lo, hi), f"{os.path.basename(f)}:{n}")
res = collections.Counter(); motivos = collections.defaultdict(list)
for (lo, hi), donde in vistos.items():
    try:
        d = sp.decodificar(lo, hi)
    except Exception as e:
        d = None
    if not d:
        res["no decodifica"] += 1; motivos["no decodifica"].append((donde, f"{lo:016x} {hi:016x}", sp._texto(lo, hi))); continue
    r = sp._reservado(d)
    try:
        list(sm86_baja.bajar(d))
        res["baja" if r is None else "baja, usa reservado"] += 1
        if r is not None: motivos["reservado"].append((donde, d["nombre"], f"R{r}"))
    except sm86_baja.SinRegla as e:
        res["sin regla"] += 1; motivos["sin regla"].append((donde, d["nombre"], str(e)[:90], sp._texto(lo, hi)))
    except Exception as e:
        res["error"] += 1; motivos["error"].append((donde, d.get("nombre"), f"{type(e).__name__}: {e}"[:90]))
print(len(vistos), "instrucciones distintas:", dict(res))
for k, v in motivos.items():
    print(f"\n== {k} ({len(v)})")
    for x in v: print("  ", *x)
