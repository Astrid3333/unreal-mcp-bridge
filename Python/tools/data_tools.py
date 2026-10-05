"""
Dominio `data` para unreal-mcp: puente Octave/Python -> Unreal.

Lee matrices numericas (CSV, TSV, espacios, ';' o cabecera de texto) y las
convierte en actores del nivel. Pensado para el flujo:

  1. Octave calcula (ode45, lorenz, mallas...) y escribe un archivo con
     `dlmwrite`/`writematrix`/`save -ascii`.
  2. `data_preview(path)` verifica shape, columnas y rangos.
  3. `data_to_unreal(path, columns=[0,1,2])` spawnea un actor por fila
     (modo "points"), une la trayectoria en orden (modo "path") o dibuja
     barras proporcionales a una columna (modo "bars").
  4. `data_clear(prefix)` lo borra.

Tambien acepta puntos en línea con `data_points_to_unreal(points=[[x,y,z],...])`
para no pasar por disco. Todas las cantidades se escalan con `scale`
(default 1.0 => unidad sim tratada como cm de Unreal) y `origin`.
"""
import logging
import math
import re
from typing import Any, Dict, List, Optional, Sequence

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')

MAX_ROWS = 5000
MAX_ACTORS = 1500

_NUM = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


def _send(ctx: Context, command: str, params: Dict[str, Any]) -> Any:
    from unreal_mcp_server import get_unreal_connection
    unreal = get_unreal_connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    response = unreal.send_command(command, params)
    if not response:
        return {"success": False, "message": "No response from Unreal Engine"}
    if response.get("status") == "error":
        return {"success": False,
                "message": response.get("message") or response.get("error", "unknown error")}
    return response.get("result", response)


def _parse_numbers(line: str) -> List[float]:
    return [float(tok) for tok in _NUM.findall(line)]


def _read_matrix(path: str, delimiter: str = "auto",
                 max_rows: int = MAX_ROWS) -> Dict[str, Any]:
    """Devuelve {rows, ncols, header, skipped} o {error}."""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            raw = fh.read()
    except OSError as exc:
        return {"error": f"No pude leer '{path}': {exc}"}
    if not raw.strip():
        return {"error": f"'{path}' esta vacio"}

    lines = [ln for ln in raw.splitlines() if ln.strip()]
    if delimiter == "auto":
        first = lines[0]
        if "," in first:
            delim = ","
        elif ";" in first and first.count(";") >= first.count("\t"):
            delim = ";"
        elif "\t" in first:
            delim = "\t"
        else:
            delim = None  # espacios
    else:
        delim = None if delimiter in ("space", "ws") else delimiter

    rows: List[List[float]] = []
    header: Optional[str] = None
    ncols = 0
    for line in lines[: max_rows + 50]:
        if delim:
            parts = [p.strip() for p in line.split(delim) if p.strip() != ""]
        else:
            parts = line.split()
        nums: List[float] = []
        bad = False
        for part in parts:
            try:
                nums.append(float(part))
            except ValueError:
                bad = True
                break
        if bad:
            if header is None and not rows:
                header = line.strip()
            continue
        if not nums:
            continue
        if not rows:
            ncols = len(nums)
        elif len(nums) != ncols:
            ncols = max(ncols, len(nums))
        rows.append(nums)
        if len(rows) >= max_rows:
            break
    if not rows:
        return {"error": f"No encontre filas numericas en '{path}'"}
    return {"rows": rows, "ncols": ncols, "header": header,
            "truncated": len(lines) > max_rows}


def _stats(rows: Sequence[Sequence[float]]) -> Dict[str, Any]:
    ncols = max(len(r) for r in rows)
    cols = []
    for c in range(ncols):
        vals = [r[c] for r in rows if len(r) > c]
        if not vals:
            continue
        cols.append({"index": c, "min": min(vals), "max": max(vals),
                     "mean": sum(vals) / len(vals), "count": len(vals)})
    return {"columns": cols}


def _to_ue(point: Sequence[float], origin: Sequence[float], scale: float) -> List[float]:
    return [origin[0] + point[0] * scale,
            origin[1] + (point[1] if len(point) > 1 else 0.0) * scale,
            origin[2] + (point[2] if len(point) > 2 else 0.0) * scale]


def _normalize(rows: Sequence[Sequence[float]], columns: Optional[Sequence[int]],
               value_column: Optional[int]) -> List[List[float]]:
    """columns = índices (0-based) a usar como x,y,z por fila."""
    if columns:
        idx = [int(c) for c in columns]
        pts = [[r[i] for i in idx if i < len(r)] for r in rows]
        pts = [p for p in pts if p]
    else:
        pts = [list(r) for r in rows]
    if value_column is not None:
        vc = int(value_column)
        for p, r in zip(pts, rows):
            if vc < len(r):
                p.append(r[vc])
    return pts


def _color_ramp(t: float, palette: str = "viridis") -> List[int]:
    """t en [0,1] -> [r,g,b] 0-255 (rampas simples, sin dependencias)."""
    t = max(0.0, min(1.0, t))
    if palette == "heat":
        return [int(255 * min(1.0, t * 2)), int(255 * t * t), int(255 * (1 - t))]
    if palette == "cool":
        return [int(255 * (1 - t)), int(255 * t), 255]
    # viridis (5 puntos interpolados, aproximacion)
    stops = [(68, 1, 84), (59, 82, 139), (33, 145, 140), (94, 201, 98), (253, 231, 37)]
    x = t * (len(stops) - 1)
    i = min(int(x), len(stops) - 2)
    f = x - i
    a, b = stops[i], stops[i + 1]
    return [int(a[k] + (b[k] - a[k]) * f) for k in range(3)]


def register_data_tools(mcp: FastMCP):
    """Registra el router `unreal_data` (tools planas `data_*`)."""

    def preview(ctx: Context, path: str, delimiter: str = "auto",
                max_rows: int = 20) -> Dict[str, Any]:
        """Inspecciona un archivo numerico (CSV/TSV/espacios) antes de llevarlo a Unreal.

        Args:
            path: ruta absoluta del archivo (ej. lo que escribio Octave con dlmwrite).
            delimiter: "auto", ",", ";", "\\t" o "space".
            max_rows: filas de vista previa a devolver.
        """
        table = _read_matrix(path, delimiter, max_rows=MAX_ROWS)
        if "error" in table:
            return {"success": False, "message": table["error"]}
        rows = table["rows"]
        out = {"success": True, "path": path, "rows": len(rows),
               "ncols": table["ncols"], "delimiter_used": delimiter,
               "truncated": table["truncated"], "header": table["header"]}
        out.update(_stats(rows))
        preview_rows = rows[:max(1, int(max_rows))]
        out["preview"] = preview_rows
        return out

    def _load(ctx: Context, path: Optional[str], points: Optional[List[Any]],
              delimiter: str, columns: Optional[List[int]],
              max_points: int) -> Dict[str, Any]:
        if path:
            table = _read_matrix(path, delimiter, max_rows=MAX_ROWS)
            if "error" in table:
                return {"error": table["error"]}
            rows = table["rows"]
        elif points:
            rows = []
            for p in points:
                if isinstance(p, (list, tuple)):
                    rows.append([float(v) for v in p])
                else:
                    rows.append([float(p)])
        else:
            return {"error": "Pasa 'path' (archivo de Octave) o 'points' (matriz en linea)"}
        if not rows:
            return {"error": "Sin filas que visualizar"}
        note = None
        if columns is None and table.get("ncols", 0) > 3:
            # típico [t, x, y, z] de Octave: la col 0 es el tiempo
            columns = list(range(1, table["ncols"]))
            note = ("Sin 'columns' y con >3 columnas, se uso la col 0 como tiempo "
                    "y las demas como coordenadas; pasa columns=[...] para controlarlo.")
        pts = _normalize(rows, columns, None)
        if len(pts) > max_points:
            gap = int(math.ceil(len(pts) / float(max_points)))
            pts = pts[::gap][:max_points]
        return {"rows": rows, "pts": pts, "note": note}

    def to_unreal(ctx: Context, path: Optional[str] = None,
                  points: Optional[List[List[float]]] = None,
                  delimiter: str = "auto", columns: Optional[List[int]] = None,
                  mode: str = "points", prefix: str = "DATA",
                  scale: float = 1.0, origin: Optional[List[float]] = None,
                  color: Optional[List[int]] = None, intensity: float = 5000.0,
                  actor_type: str = "PointLight", max_points: int = 500,
                  ramp: str = "viridis", value_scale: float = 1.0,
                  mesh_path: str = "/Engine/BasicShapes/Sphere.Sphere",
                  point_scale: float = 2.0) -> Dict[str, Any]:
        """Lleva una matriz (archivo de Octave o puntos en linea) a actores de Unreal.

        Args:
            path: archivo numerico escrito por Octave/Python (CSV, TSV, espacios).
            points: alternativa al archivo: [[x,y,z], ...] en línea.
            delimiter: separador del archivo ("auto" por defecto).
            columns: indices 0-based a usar como x,y,z (ej. [1,2,3] si la col 0 es tiempo).
            mode: "points" = un actor por fila; "path" = un actor por fila ademas
                  coloreado en degradado a lo largo de la serie; "bars" = columnas
                  [x, valor] => actores cuya altura es el valor (2D) — usa el
                  ultimo componente de `columns` como valor si pasas 2 indices.
            prefix: prefijo de nombres (reutiliza con data_clear).
            scale: multiplicador sim -> cm de Unreal (SI en metros usa 100).
            origin: traslacion [x, y, z] en cm.
            color: [r,g,b] 0-255 fijo (si no, en mode="path" se usa rampa).
            intensity: intensidad de las luces.
            actor_type: tipo de actor (PointLight, Sphere, Cube...).
            max_points: tope de actores (se muestrea si la matriz es mayor).
            ramp: rampa de color para mode="path": "viridis", "heat", "cool".
            value_scale: factor para mode="bars".
            mesh_path: malla para actor_type StaticMeshActor (esfera por defecto;
                       usa /Engine/BasicShapes/Cube.Cube para barras).
            point_scale: escala uniforme de cada malla (cm en el mundo).
        """
        if mode not in ("points", "path", "bars"):
            return {"success": False, "message": "mode debe ser points, path o bars"}
        loaded = _load(ctx, path, points, delimiter, columns, max_points)
        if "error" in loaded:
            return {"success": False, "message": loaded["error"]}
        rows, pts = loaded["rows"], loaded["pts"]
        if mode == "bars":
            # [x, valor] -> [x, 0, valor] (altura en Z) para poder escalar la malla
            pts = [[p[0], 0.0, p[-1] * float(value_scale)] for p in pts if len(p) >= 2]

        origin = list(origin or [0.0, 0.0, 0.0])
        color = list(color or [255, 255, 255])
        name_prefix = f"{prefix}_{abs(hash((path or 'inline', len(pts)))) % 100000:05d}"

        # limpia restos de una corrida anterior con el mismo prefijo
        clear_unreal(ctx, prefix=name_prefix)

        created: List[str] = []
        for index, point in enumerate(pts):
            if len(created) >= MAX_ACTORS:
                break
            name = f"{name_prefix}_{index:04d}"
            if mode == "bars" and len(point) >= 3:
                # el cubo esta centrado: la base debe tocar origin[2]
                loc = [origin[0] + point[0] * scale, origin[1],
                       origin[2] + point[2] * scale * 0.5]
            else:
                loc = _to_ue(point[:3] if len(point) >= 3
                             else [point[0], 0.0, point[1] if len(point) > 1 else 0.0],
                             origin, scale)
            result = _send(ctx, "spawn_actor",
                           {"name": name, "type": actor_type, "location": loc})
            if isinstance(result, dict) and result.get("success") is False:
                return {"success": False,
                        "message": f"No pude crear {name}: {result.get('message')}",
                        "created": created, "prefix": name_prefix}
            c = color
            if mode == "path":
                c = color if (len(pts) == 1) else _color_ramp(index / max(1, len(pts) - 1), ramp)
            if "light" in actor_type.lower():
                _send(ctx, "set_actor_property",
                      {"name": name, "property_name": "Intensity",
                       "property_value": float(intensity)})
                _send(ctx, "set_actor_property",
                      {"name": name, "property_name": "LightColor",
                       "property_value": list(c) + [255] * (4 - len(c))})
            if "StaticMesh" in actor_type:
                _send(ctx, "set_actor_property",
                      {"name": name, "property_name": "StaticMesh",
                       "property_value": mesh_path})
            if mode == "bars" and len(point) >= 3:
                _send(ctx, "set_actor_transform",
                      {"name": name, "scale": [0.5, 0.5, max(0.05, abs(point[2]) * 0.01)]})
            elif "StaticMesh" in actor_type:
                _send(ctx, "set_actor_transform",
                      {"name": name, "scale": [float(point_scale)] * 3})
            created.append(name)

        return {"success": True, "path": path, "rows_read": len(rows),
                "actors_created": len(created), "mode": mode,
                "prefix": name_prefix, "first": created[:3], "last": created[-3:],
                **({"note": loaded["note"]} if loaded.get("note") else {})}

    def clear_unreal(ctx: Context, prefix: str) -> Dict[str, Any]:
        """Borra los actores creados por data_to_unreal con ese prefijo.

        Args:
            prefix: prefijo de nombres (el que devolvio to_unreal).
        """
        found = _send(ctx, "find_actors_by_name", {"pattern": prefix})
        names = [a.get("name") for a in found.get("actors", [])
                 if isinstance(a, dict) and str(a.get("name", "")).startswith(prefix)]
        deleted, errors = 0, []
        for name in names:
            result = _send(ctx, "delete_actor", {"name": name})
            if isinstance(result, dict) and result.get("deleted_actor"):
                deleted += 1
            elif isinstance(result, dict) and result.get("success") is False:
                errors.append(result.get("message"))
        return {"success": not errors, "found": len(names), "deleted": deleted,
                "errors": errors}

    def stats(ctx: Context, path: str, delimiter: str = "auto") -> Dict[str, Any]:
        """Rangos (min/max/mean) por columna de un archivo numerico de Octave.

        Args:
            path: ruta del archivo.
            delimiter: "auto", ",", ";", "\\t" o "space".
        """
        table = _read_matrix(path, delimiter, max_rows=MAX_ROWS)
        if "error" in table:
            return {"success": False, "message": table["error"]}
        out = {"success": True, "path": path, "rows": len(table["rows"]),
               "ncols": table["ncols"], "header": table["header"]}
        out.update(_stats(table["rows"]))
        return out

    ACTIONS = {
        'preview': preview,
        'stats': stats,
        'to_unreal': to_unreal,
        'clear': clear_unreal,
    }

    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_data",
        description=(
            "Puente Octave/Python -> Unreal: lee matrices numericas (CSV/TSV/espacios)\n"
            "    y las convierte en actores del nivel.\n"
            "    \n"
            "    Flujo tipico: Octave escribe CSV (dlmwrite/writematrix) ->\n"
            "    data_preview(path) -> data_to_unreal(path, columns=[...], mode=...)\n"
            "    -> data_clear(prefix).\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_data(ctx: Context, action: str,
                    params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_data action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_data '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Data tools (router) registered: {len(ACTIONS)} acciones")
