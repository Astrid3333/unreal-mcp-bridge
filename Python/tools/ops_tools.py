"""
Operaciones de ciclo de vida para unreal-mcp (ciclo 11, bloque 4):
batch, wait_until y lectura del log del editor.

Routers y tools planas generadas:

  * `unreal_batch` -> `batch_run(commands=[{type, params}, ...],
    stop_on_error=False)` — ejecuta varios comandos del socket en serie
    en una sola llamada MCP. Devuelve `results` con `ok` por comando.
    Los comandos diferidos (open/save: respuesta `deferred:true`) NO se
    esperan dentro del batch: correr despues `wait_until`.
  * `unreal_wait` -> `wait_until(condition, params=..., timeout=60)` —
    espera a que se cumpla una condicion en vez de dormir a ciegas.
    Condiciones: pending_idle, map, dirty, playing, selection_count,
    actor_count (op ">="|"=="), log (contains + since_seq interno).
  * `unreal_log` -> `log_get(limit=100, category=..., min_severity=...,
    contains=..., since_seq=0)` / `log_clear()` — buffer circular (2000
    lineas, orden mas-reciente-primero) del FOutputDevice del editor;
    `log_get(limit=0)` solo devuelve `next_seq` (snapshot barato).
"""
import logging
import time
from typing import Any, Dict, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')


def _connection():
    from unreal_mcp_server import get_unreal_connection
    return get_unreal_connection()


def _raw(command: str, params: Dict[str, Any]) -> Any:
    """Respuesta cruda del socket (envelope con status/error si falla)."""
    unreal = _connection()
    if not unreal:
        return {"status": "error", "error": "Failed to connect to Unreal Engine"}
    return unreal.send_command(command, params) or {"status": "error", "error": "No response"}


def _send(command: str, params: Dict[str, Any]) -> Any:
    """Solo el 'result' del envelope; en error devuelve {success:False,...}."""
    response = _raw(command, params)
    if response.get("status") == "error":
        return {"success": False,
                "message": response.get("error") or response.get("message") or "unknown error"}
    return response.get("result", response)


def _state() -> Dict[str, Any]:
    res = _send("get_editor_state", {})
    return res if isinstance(res, dict) else {}


# ---------------------------------------------------------------- batch

def _batch_run(ctx: Context, commands: Any, stop_on_error: bool = False) -> Dict[str, Any]:
    if not isinstance(commands, list) or not commands:
        return {"success": False, "message": "'commands' debe ser lista no vacia"}
    if len(commands) > 50:
        return {"success": False, "message": "maximo 50 comandos por batch"}
    results = []
    aborted = False
    for i, item in enumerate(commands):
        if not isinstance(item, dict) or not str(item.get("type", "")).strip():
            return {"success": False,
                    "message": f"commands[{i}] invalido: se espera un dict "
                               "con 'type' (str) y 'params' (dict)"}
        ctype = str(item["type"]).strip()
        cparams = item.get("params") or {}
        if not isinstance(cparams, dict):
            return {"success": False, "message": f"commands[{i}].params debe ser objeto"}
        resp = _raw(ctype, cparams)
        ok = resp.get("status") != "error"
        results.append({"index": i, "type": ctype, "ok": ok, "response": resp})
        if not ok and stop_on_error:
            aborted = True
            break
    return {"success": True, "executed": len(results), "aborted": aborted,
            "results": results}


# ------------------------------------------------------------- wait_until

def _eval_condition(condition: str, p: Dict[str, Any],
                    st: Dict[str, Any]) -> Optional[bool]:
    """True/False si la condicion es evaluable con el estado, None si no."""
    if condition == "pending_idle":
        return (st.get("pending_kind") or "") == ""
    if condition == "map":
        return st.get("map_name") == p.get("map")
    if condition == "dirty":
        return bool(st.get("dirty")) == bool(p.get("value", True))
    if condition == "playing":
        return bool(st.get("playing")) == bool(p.get("value", True))
    if condition == "selection_count":
        return len(st.get("selection") or []) == int(p.get("count", 0))
    if condition == "actor_count":
        cur = int(st.get("actor_count", 0))
        target = int(p.get("count", 0))
        if str(p.get("op", ">=")) == "==":
            return cur == target
        return cur >= target
    return None


def _wait_until(ctx: Context, condition: str, params: Optional[Dict[str, Any]] = None,
                timeout: float = 60.0, interval: float = 0.5) -> Dict[str, Any]:
    condition = str(condition or "").strip()
    p = params or {}
    if not condition:
        return {"success": False, "message": "'condition' obligatoria"}
    try:
        timeout = max(1.0, min(float(timeout), 300.0))
        interval = max(0.1, min(float(interval), 5.0))
    except (TypeError, ValueError):
        return {"success": False, "message": "timeout/interval invalidos"}
    if condition not in ("pending_idle", "map", "dirty", "playing",
                         "selection_count", "actor_count", "log"):
        return {"success": False,
                "message": f"condicion desconocida: '{condition}' (pending_idle|map|dirty|"
                           "playing|selection_count|actor_count|log)"}

    t0 = time.time()
    since_seq = 0
    if condition == "log":
        if not str(p.get("contains", "")).strip():
            return {"success": False, "message": "condicion 'log' requiere params.contains"}
        if "since_seq" in p:
            # 0 = acepta lineas ya existentes; N = solo nuevas desde esa seq.
            since_seq = int(p.get("since_seq") or 0)
        else:
            snap = _send("get_log_entries", {"limit": 0})
            if not snap.get("success"):
                return {"success": False, "message": f"log no disponible: {snap}"}
            since_seq = int(snap.get("next_seq") or 0)

    last: Any = {}
    while time.time() - t0 < timeout:
        if condition == "log":
            res = _send("get_log_entries", {"limit": 10, "contains": p["contains"],
                                            "since_seq": since_seq})
            # El servidor loguea Received:/Sending response: con los params
            # y la respuesta de nuestra propia consulta: esos ecos contienen
            # el needle y matchearian consigo mismos. Solo cuentan lineas
            # reales (cualquier mensaje del transporte MCPServer se salta).
            if res.get("success"):
                for entry in res.get("entries") or []:
                    if str(entry.get("message", "")).startswith("MCPServerRunnable:"):
                        continue
                    return {"success": True, "condition": condition,
                            "waited_s": round(time.time() - t0, 2), "entry": entry}
            last = res
        else:
            st = _state()
            if st.get("success"):
                met = _eval_condition(condition, p, st)
                if met is None:
                    return {"success": False, "message": f"no evaluable: {condition}"}
                if met:
                    return {"success": True, "condition": condition,
                            "waited_s": round(time.time() - t0, 2), "state": st}
                last = st
        time.sleep(interval)
    return {"success": False, "timed_out": True, "condition": condition,
            "waited_s": round(time.time() - t0, 2), "last": last}


# ------------------------------------------------------------------ log

def _log_get(ctx: Context, limit: int = 100, category: str = "",
             min_severity: str = "", contains: str = "",
             since_seq: float = 0) -> Dict[str, Any]:
    """Lee el buffer de log del editor (mas reciente primero)."""
    try:
        limit = max(0, min(int(limit), 1000))
        since = max(0.0, float(since_seq))
    except (TypeError, ValueError):
        return {"success": False, "message": "limit/since_seq invalidos"}
    return _send("get_log_entries", {
        "limit": limit, "category": str(category or "").strip(),
        "min_severity": str(min_severity or "").strip(),
        "contains": str(contains or "").strip(), "since_seq": since,
    })


def _log_clear(ctx: Context) -> Dict[str, Any]:
    """Vacia el buffer de log (el contador seq sigue creciendo)."""
    return _send("clear_log_buffer", {})


def register_batch_tools(mcp: FastMCP):
    """Registra el router `unreal_batch` (tool plana `batch_run`)."""
    ACTIONS = {'run': _batch_run}
    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_batch",
        description=(
            "Ejecuta varios comandos unreal en serie en una sola llamada:\n"
            "    \n"
            "    Flujo tipico: batch_run(commands=[{\"type\": \"spawn_actor\",\n"
            "    \"params\": {...}}, ...], stop_on_error=False) reduce el\n"
            "    viaje ida-vuelta del socket. Cada resultado trae su\n"
            "    envelope crudo (status/response). Los comandos diferidos\n"
            "    (open/save) no se esperan: usar despues wait_until.\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_batch(ctx: Context, action: str,
                     params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_batch action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_batch '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Batch tools (router) registered: {len(ACTIONS)} acciones")


def register_wait_tools(mcp: FastMCP):
    """Registra el router `unreal_wait` (tool plana `wait_until`)."""
    ACTIONS = {'until': _wait_until}
    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_wait",
        description=(
            "Espera activa a una condicion del editor (sin dormir a\n"
            "    ciegas):\n"
            "    \n"
            "    Flujo tipico: despues de un save/open diferido, correr\n"
            "    wait_until(\"pending_idle\") (o \"map\" con\n"
            "    params={\"map\": \"/Game/Maps/X\"}); \"dirty\" con\n"
            "    params={\"value\": true|false}; \"playing\";\n"
            "    \"selection_count\"; \"actor_count\" (op \">=\"|\"==\");\n"
            "    \"log\" con params={\"contains\": \"texto\"}. timeout en\n"
            "    segundos (corte 300); al agotarse devuelve timed_out.\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_wait(ctx: Context, action: str,
                    params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_wait action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_wait '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Wait tools (router) registered: {len(ACTIONS)} acciones")


def register_log_tools(mcp: FastMCP):
    """Registra el router `unreal_log` (tools planas `log_get`, `log_clear`)."""
    ACTIONS = {'get': _log_get, 'clear': _log_clear}
    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_log",
        description=(
            "Log del editor Unreal (buffer circular, 2000 lineas):\n"
            "    \n"
            "    Flujo tipico: log_get(limit=50, contains=\"...\") para ver\n"
            "    errores recientes (severity Warning|Error), con\n"
            "    min_severity para cortar ruido y since_seq para solo lo\n"
            "    nuevo desde un snapshot (log_get(limit=0)); log_clear()\n"
            "    para empezar de cero antes de una prueba.\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_log(ctx: Context, action: str,
                   params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_log action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_log '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Log tools (router) registered: {len(ACTIONS)} acciones")
