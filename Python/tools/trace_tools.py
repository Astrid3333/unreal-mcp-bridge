"""Trace de llamadas a tools del servidor Unreal MCP (ciclo 11e).

Cada invocacion de tool (plana o router) queda registrada en un buffer
circular en memoria con seq, timestamp, duracion, argumentos y resultado
resumido. Expongo `trace_get` / `trace_clear` via el router `unreal_trace`.

El hook se instala envolviendo `Tool.fn` despues del registro + flatten
(`install_trace_recording`): la metadata de esquema ya esta calculada, asi
que el wrapper no afecta a la validacion de argumentos. `trace_*` se salta
para no registrarse a si mismas (ni bucles de self-observation).
"""

import json
import threading
import time
from collections import deque
from typing import Any, Callable, Dict, List, Optional

try:
    from mcp.server.fastmcp import Context, FastMCP
except ImportError:  # entorno de test sin el SDK MCP
    Context = Any  # type: ignore[assignment]
    FastMCP = Any  # type: ignore[assignment]

BUFFER_MAX = 1000
FIELD_MAX = 400  # caracteres por campo serializado

_LOCK = threading.Lock()
_BUFFER: deque = deque(maxlen=BUFFER_MAX)
_SEQ = 0  # monotono; no se resetea al limpiar

# Tools que no se auto-registran.
_SKIP = {"trace_get", "trace_clear", "unreal_trace"}


def _truncate(value: Any) -> str:
    try:
        text = json.dumps(value, default=str, ensure_ascii=False)
    except Exception:
        text = str(value)
    if len(text) > FIELD_MAX:
        return text[:FIELD_MAX] + "...<truncado>"
    return text


def record_trace(tool: str, args: Dict[str, Any], result: Any,
                 dur_ms: float, error: Optional[str] = None) -> Dict[str, Any]:
    """Registra una llamada. Devuelve la entrada guardada."""
    global _SEQ
    args = {k: v for k, v in (args or {}).items() if k not in ("ctx", "context")}
    ok = error is None
    success: Optional[bool] = None
    if error is None:
        if isinstance(result, dict):
            if "success" in result:
                success = bool(result.get("success"))
                ok = success
            elif result.get("status") == "error":
                ok = False
    entry = {
        "seq": 0,
        "time": time.strftime("%Y-%m-%d %H:%M:%S"),
        "tool": tool,
        "args": _truncate(args),
        "result": _truncate(result) if error is None else "",
        "dur_ms": round(dur_ms, 2),
        "ok": ok,
    }
    if success is not None:
        entry["success"] = success
    if error is not None:
        entry["error"] = error[:FIELD_MAX]
    with _LOCK:
        _SEQ += 1
        entry["seq"] = _SEQ
        _BUFFER.append(entry)
    return entry


def wrap_tool_fn(name: str, fn: Callable[..., Any]) -> Callable[..., Any]:
    """Envuelve fn para que cada llamada quede en el trace."""
    import asyncio
    import functools
    import inspect

    if inspect.iscoroutinefunction(fn):
        @functools.wraps(fn)
        async def _aw(*a, **k):
            t0 = time.perf_counter()
            try:
                r = await fn(*a, **k)
            except Exception as exc:
                record_trace(name, k, None, (time.perf_counter() - t0) * 1000.0,
                             error=f"{type(exc).__name__}: {exc}")
                raise
            record_trace(name, k, r, (time.perf_counter() - t0) * 1000.0)
            return r
        return _aw

    @functools.wraps(fn)
    def _w(*a, **k):
        t0 = time.perf_counter()
        try:
            r = fn(*a, **k)
        except Exception as exc:
            record_trace(name, k, None, (time.perf_counter() - t0) * 1000.0,
                         error=f"{type(exc).__name__}: {exc}")
            raise
        record_trace(name, k, r, (time.perf_counter() - t0) * 1000.0)
        return r
    return _w


def install_trace_recording(mcp: FastMCP) -> int:
    """Envuelve Tool.fn de todas las tools ya registradas. Devuelve cuantas."""
    manager = mcp._tool_manager
    wrapped = 0
    for tool_name, tool in list(manager._tools.items()):
        if tool_name in _SKIP:
            continue
        original = getattr(tool, "fn", None)
        if original is None:
            continue
        tool.fn = wrap_tool_fn(tool_name, original)
        wrapped += 1
    return wrapped


def _trace_get(ctx: Context, limit: int = 100, tool: str = "",
               ok: Optional[bool] = None, since_seq: int = 0) -> Dict[str, Any]:
    try:
        limit = int(limit)
    except (TypeError, ValueError):
        return {"success": False, "message": f"limit invalido: {limit!r}"}
    if limit < 1 or limit > BUFFER_MAX:
        return {"success": False,
                "message": f"limit debe estar entre 1 y {BUFFER_MAX}: {limit}"}
    try:
        since_seq = int(since_seq or 0)
    except (TypeError, ValueError):
        return {"success": False, "message": f"since_seq invalido: {since_seq!r}"}
    needle = str(tool or "").strip().lower()
    with _LOCK:
        all_entries = list(_BUFFER)
        next_seq = _SEQ
    matched = [
        e for e in all_entries
        if e["seq"] > since_seq
        and (not needle or needle in e["tool"].lower())
        and (ok is None or bool(e.get("ok")) is bool(ok))
    ]
    matched.reverse()  # mas reciente primero
    returned = matched[:limit]
    return {"success": True, "total": len(matched), "returned": len(returned),
            "next_seq": next_seq, "buffer_size": len(all_entries),
            "traces": returned}


def _trace_clear(ctx: Context) -> Dict[str, Any]:
    global _SEQ
    with _LOCK:
        cleared = len(_BUFFER)
        _BUFFER.clear()
        next_seq = _SEQ
    return {"success": True, "cleared": cleared, "next_seq": next_seq}


def register_trace_tools(mcp: FastMCP):
    """Router `unreal_trace` -> trace_get / trace_clear (ciclo 11e)."""
    ACTIONS = {
        "get": _trace_get,
        "clear": _trace_clear,
    }

    @mcp.tool()
    def unreal_trace(ctx: Context, action: str = "get",
                     params: Optional[Dict[str, Any]] = None) -> Any:
        """Trace de llamadas a tools: get (buffer circular de 1000
        llamadas con seq/time/tool/args/result/dur_ms/ok) o clear.
        Filtros de get: limit, tool (subcadena), ok, since_seq."""
        fn = ACTIONS.get(action)
        if fn is None:
            return {"success": False,
                    "message": f"accion invalida: {action}. Usa get|clear"}
        return fn(ctx, **(params or {}))
