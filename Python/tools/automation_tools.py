"""
Runner de tests automatizados de Unreal Engine via MCP (ciclo 19).

Routers y tools planas generadas:

  * `unreal_automation` -> `list(...)` / `run(filter, ...)` / `results(...)`.

Basado en el parser `FAutomationExecCmd::Exec_Dev`
(Engine/Source/Developer/AutomationController/Private/AutomationCommandline.cpp):

  * El Exec parte el Cmd en `;` y llama `Init()` UNA sola vez por invocacion:
    varios subcomandos en un solo Exec pagan el gate de framerate una sola vez.
    Por eso siempre se emite `Automation Now; <subcomando>`.
  * `RunTests Now <filtro>` es un error del engine: fija `DelayTimer=0` y hace
    `continue` sin encolar la corrida. La forma correcta es
    `Automation Now; RunTests <filtro>`.
  * `Now` suelto solo anula el DelayTimer de 5s; el gate de framerate
    (`FWaitForInteractiveFrameRate`, hasta 600s si el editor esta en
    background con `bThrottleCPUWhenNotForeground`) sigue corriendo.
  * NUNCA se emiten `Quit` / `SoftQuit` (cierran el editor).

Marcadores en el log:

  * Lista: `Found %d Automation Tests` + una linea `\t'<test>'` por test y
    cierre `...Automation Test Queue Empty %d tests performed.`
  * Corrida: `Found %d automation tests based on '%s'` (o Error
    `No automation tests matched '%s'`), un `Test Completed. Result={...}
    Name={...} Path={...}` por test (LogAutomationController) y el mismo
    cierre `Queue Empty %d tests performed.`.
  * El buffer `get_log_entries` es circular (2000 entradas) y cada llamada MCP
    deja ~7-8 ecos LogTemp (el dump JSON de la respuesta cuenta COMO UNA
    entrada multi-linea, no como N): por eso se acumula INCREMENTALMENTE en
    cada poll (no solo al final) y se hace snapshot de `next_seq` antes de
    emitir.
  * El server hace Clamp(limit, 0, 1000) y la respuesta trae los mas
    recientes: `since_seq` NO pagina (baja el piso pero siempre devuelve los
    ultimos `limit`), asi que un volcado de 1436 lineas no se puede leer en
    una sola consulta. Los nombres se recuperan por particion
    `contains="'<L>"` (bucket ~55 lineas) hasta igualar el `Found N`.

Nota de entorno: `bThrottleCPUWhenNotForeground=False` (EditorSettings.ini)
evita que el editor baje a 3 FPS en background; sin eso cada comando
`Automation` espera hasta 600s en el gate.
"""
import logging
import re
import time
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')

# Marcadores (verbatim de AutomationCommandline.cpp / AutomationController)
_RE_FOUND_LIST = re.compile(r"^Found (\d+) Automation Tests\b")
_RE_FOUND_RUN = re.compile(r"^Found (\d+) automation tests based on '(.*)'$")
_RE_NO_MATCH = re.compile(r"^No automation tests matched '(.*)'$")
_RE_NAME_LINE = re.compile(r"^\s*'(.*)'$")
_RE_COMPLETED = re.compile(r"^Test Completed\. Result=\{(\w+)\} Name=\{(.*)\} Path=\{(.*)\}$")
_RE_QUEUE_EMPTY = re.compile(r"\.\.\.Automation Test Queue Empty (\d+) tests performed\.$")

_POLL_INTERVAL = 1.0
_LIST_TIMEOUT_DEFAULT = 120.0
_RUN_TIMEOUT_DEFAULT = 180.0
_TIMEOUT_MAX = 720.0
_LIMIT_MAX = 1000  # Clamp del server en UnrealMCPLogCommands.cpp:180

# Bucket de particion para nombres: \t'<test>' -> contains="'<L>"
_NAME_FIRST_CHARS = list("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") + ["_", ".", "-", "(", " "]


def _connection():
    from unreal_mcp_server import get_unreal_connection
    return get_unreal_connection()


def _send(command: str, params: Dict[str, Any]) -> Any:
    unreal = _connection()
    if not unreal:
        return {"success": False, "message": "Failed to connect to Unreal Engine"}
    response = unreal.send_command(command, params) or {"success": False,
                                                        "message": "No response"}
    if isinstance(response, dict) and response.get("status") == "error":
        return {"success": False,
                "message": response.get("error") or response.get("message") or "unknown error"}
    return response.get("result", response) if isinstance(response, dict) else response


def _snapshot_seq() -> int:
    """next_seq actual del buffer de log (solo lectura, no agrega utilidad)."""
    res = _send("get_log_entries", {"limit": 0})
    if not isinstance(res, dict) or not res.get("success"):
        return 0
    return int(res.get("next_seq") or 0)


def _poll(since_seq: int, category: str, limit: int = _LIMIT_MAX,
          contains: Optional[str] = None) -> Optional[List[Dict[str, Any]]]:
    """Entradas nuevas (seq > since_seq) de una categoria, mas reciente primero.

    `limit` se clampa a 1000 en el server; la respuesta siempre trae los
    `limit` mas recientes que cumplen el filtro, asi que con mas matches que
    limit NO se puede llegar a los mas viejos bajando `since_seq` (hay que
    particionar con `contains`).

    Devuelve None si el transporte fallo (send_command ya agoto sus
    reintentos internos): el caller debe distinguirlo de [] (sin entradas
    nuevas). Devolver [] en ambos casos silenciaba el fallo y dejaba
    total=None / names incompletos sin aviso.
    """
    params: Dict[str, Any] = {"limit": max(0, min(int(limit), _LIMIT_MAX)),
                              "category": category,
                              "since_seq": float(since_seq)}
    if contains:
        params["contains"] = contains
    res = _send("get_log_entries", params)
    if not isinstance(res, dict) or not res.get("success"):
        return None
    return list(res.get("entries") or [])


def _poll_retry(since_seq: int, category: str, limit: int = _LIMIT_MAX,
                contains: Optional[str] = None, tries: int = 3) -> Optional[List[Dict[str, Any]]]:
    """_poll con reintentos cortos ante fallo de transporte.

    None = agoto los `tries` intentos.
    """
    for i in range(tries):
        entries = _poll(since_seq, category, limit=limit, contains=contains)
        if entries is not None:
            return entries
        time.sleep(0.5 * (i + 1))
    return None


def _exec(command: str) -> Dict[str, Any]:
    """Ejecuta la consola y devuelve la salida sincrona (Ar.Logf del Exec)."""
    return _send("execute_console_command", {"command": command})


def _sanitize_filter(filter_str: Any) -> str:
    """El Exec parte el Cmd en ';': un ';' dentro del filtro romperia la corrida."""
    return str(filter_str or "").replace(";", " ").strip()


def _fill_names(names: List[str], since_seq: int,
                total: Optional[int]) -> tuple:
    """Rellena nombres faltantes particionando por primer caracter.

    El volcado de `List` emite las lineas en orden alfabetico en UN solo
    frame (~1436 lineas), superando `_LIMIT_MAX`; el bulk lee los ultimos
    1000 (las letras tardias) y los mas viejos (A..) solo se recuperan con
    `contains="'<L>"` (bucket ~55 lineas < 1000). Se recorren de la A
    hacia la Z: si el buffer circular eviciona algo, eviciona las lineas
    mas viejas, que son justas las primeras que pedimos.

    Devuelve (nombres_deduplicados, buckets_consultados).
    """
    seen: Dict[str, None] = dict.fromkeys(names)
    buckets = 0
    for ch in _NAME_FIRST_CHARS:
        if total is not None and len(seen) >= total:
            break
        entries = _poll_retry(since_seq, "LogAutomationCommandLine",
                              contains=f"'{ch}")
        if entries is None:
            # Transporte caido tras reintentos: cortar (missing/names_partial
            # reportan la incompletitud) en vez de seguir pagando timeouts.
            break
        buckets += 1
        for e in entries:
            m = _RE_NAME_LINE.match(str(e.get("message", "")))
            if m:
                seen.setdefault(m.group(1))
    return list(seen), buckets


# Tipos de EAutomationTestResult que puede emitir `Test Completed. Result={...}`
_RESULT_TYPES = ("Success", "Fail", "Warnings", "NotRun", "Skipped", "InProcess")


def _fill_results(results: List[Dict[str, Any]], since_seq: int,
                  performed: Optional[int]) -> tuple:
    """Rellena resultados faltantes particionando por tipo de resultado.

    Mismo limit que en `_fill_names`: si entre dos polls (1s) llegan mas de
    1000 `Test Completed`, el bulk deja fuera los mas viejos y `since_seq`
    no los recupera. Se re-pide por `Result={<tipo>}` (6 buckets, cada uno
    < 1000 salvo corridas gigantes) hasta igualar `performed`.

    Dedup por nombre (una corrida = un resultado por test).
    Devuelve (resultados_deduplicados, buckets_consultados).
    """
    seen: Dict[str, Dict[str, Any]] = {str(r.get("name")): r for r in results}
    buckets = 0
    for tok in _RESULT_TYPES:
        if performed is not None and len(seen) >= performed:
            break
        entries = _poll_retry(since_seq, "LogAutomationController",
                              contains=f"Result={{{tok}}}")
        if entries is None:
            break
        buckets += 1
        acc: Dict[str, Any] = {"results": []}
        _parse_controller(entries, acc)
        for r in acc["results"]:
            seen.setdefault(str(r.get("name")), r)
    return list(seen.values()), buckets


def _parse_cmdline(entries: List[Dict[str, Any]], acc: Dict[str, Any]) -> None:
    """Acumula estado de LogAutomationCommandLine sobre 'acc'."""
    for e in entries:
        msg = str(e.get("message", ""))
        m = _RE_FOUND_LIST.match(msg)
        if m:
            acc["found"] = int(m.group(1))
            continue
        m = _RE_FOUND_RUN.match(msg)
        if m:
            acc["found"] = int(m.group(1))
            acc["matched_filter"] = m.group(2)
            continue
        m = _RE_NO_MATCH.match(msg)
        if m:
            acc["no_match"] = True
            acc["matched_filter"] = m.group(1)
            acc["error"] = msg
            continue
        m = _RE_QUEUE_EMPTY.search(msg)
        if m:
            acc["queue_empty"] = True
            acc["performed"] = int(m.group(1))
            continue
        if acc.get("collect_names"):
            m = _RE_NAME_LINE.match(msg)
            if m:
                acc["names"].append(m.group(1))
                continue
        if str(e.get("severity", "")) == "Error" and msg and "MCPServerRunnable" not in msg:
            acc.setdefault("errors", []).append(msg)


def _parse_controller(entries: List[Dict[str, Any]], acc: Dict[str, Any]) -> None:
    """Acumula los `Test Completed. Result={...}` de LogAutomationController."""
    for e in entries:
        msg = str(e.get("message", ""))
        m = _RE_COMPLETED.match(msg)
        if m:
            acc["results"].append({"result": m.group(1), "name": m.group(2),
                                   "path": m.group(3), "seq": e.get("seq")})


def _run_exec_and_poll(command: str, categories: List[str], acc: Dict[str, Any],
                       timeout: float, interval: float = _POLL_INTERVAL) -> Dict[str, Any]:
    """Snapshot -> Exec -> poll incremental hasta cierre, timeout o error fatal.

    Cada poll acumula sobre 'acc', asi que un desborde del buffer circular
    entre polls no pierde lo ya leido.
    """
    seq = _snapshot_seq()
    acc["since_seq"] = seq
    out = _exec(command)
    if not (isinstance(out, dict) and out.get("success")):
        acc["exec_error"] = out if isinstance(out, dict) else str(out)
        return acc
    acc["exec_output"] = str(out.get("output", ""))

    t0 = time.time()
    poll_fails = 0
    while time.time() - t0 < timeout:
        for cat in categories:
            entries = _poll(seq, cat)
            if entries is None:
                # Transporte caido: tolerar un tick, abortar a la tercera
                # vez (un [] silencioso aqui colgaba el bucle hasta timeout).
                poll_fails += 1
                if poll_fails >= 3:
                    acc["poll_error"] = (f"transporte: {poll_fails} polls "
                                         "seguidos sin respuesta")
                    acc["elapsed_s"] = round(time.time() - t0, 2)
                    return acc
                break
            poll_fails = 0
            if entries:
                # seq mayor de lo visto: el siguiente poll re-pide desde ahi.
                seq = max(seq, max(int(e.get("seq") or 0) for e in entries))
                if cat == "LogAutomationCommandLine":
                    _parse_cmdline(entries, acc)
                else:
                    _parse_controller(entries, acc)
        if acc.get("queue_empty") or acc.get("no_match"):
            acc["elapsed_s"] = round(time.time() - t0, 2)
            return acc
        time.sleep(interval)
    acc["timed_out"] = True
    acc["elapsed_s"] = round(time.time() - t0, 2)
    return acc


# ------------------------------------------------------------------- list

def _automation_list(ctx: Context, timeout: float = _LIST_TIMEOUT_DEFAULT,
                     max_names: int = 2000) -> Dict[str, Any]:
    """`Automation Now; List`: volcado de todos los tests registrados."""
    try:
        timeout = max(5.0, min(float(timeout), _TIMEOUT_MAX))
        max_names = max(1, min(int(max_names), 5000))
    except (TypeError, ValueError):
        return {"success": False, "message": "timeout/max_names invalidos"}

    acc: Dict[str, Any] = {"names": [], "collect_names": True}
    _run_exec_and_poll("Automation Now; List",
                       ["LogAutomationCommandLine"], acc, timeout)

    if acc.get("exec_error") is not None:
        return {"success": False, "message": f"exec fallo: {acc['exec_error']}"}
    if acc.get("poll_error"):
        return {"success": False, "message": acc["poll_error"],
                "since_seq": acc.get("since_seq"),
                "names_so_far": len(acc["names"])}
    if acc.get("timed_out") and not acc.get("queue_empty"):
        return {"success": False, "timed_out": True,
                "message": "timeout esperando el volcado de tests "
                           "(gate de framerate o editor ocupado)",
                "since_seq": acc.get("since_seq"), "found": acc.get("found"),
                "names_so_far": len(acc["names"])}

    total = acc.get("found")
    since = int(acc.get("since_seq") or 0)
    if total is None:
        # La linea 'Found N' es la mas vieja del volcado: el poll de cierre
        # (mas reciente primero, limit 1000) no la alcanzo. Query dirigida:
        # son pocos matches, asi que vuelve completa. Reintentar: si aqui
        # falla el transporte, total queda None y la lista entera se reporta
        # incompleta sin motivo.
        for i in range(3):
            entries = _poll(since, "LogAutomationCommandLine", contains="Found")
            if entries:
                _parse_cmdline(entries, acc)
                total = acc.get("found")
                if total is not None:
                    break
            time.sleep(0.5 * (i + 1))
        total = acc.get("found")

    total_known = isinstance(total, int)
    names, buckets = _fill_names(acc["names"], since, total)
    missing = (total - len(names)) if total_known else None
    truncated = len(names) > max_names
    return {"success": True, "total": total,
            "total_known": total_known,
            "count": len(names), "truncated": truncated,
            "names": names[:max_names],
            "names_partial": (not total_known) or (missing > 0),
            "missing": missing if missing and missing > 0 else 0,
            "buckets": buckets,
            "performed": acc.get("performed"),
            "errors": acc.get("errors") or [],
            "since_seq": acc.get("since_seq"),
            "elapsed_s": acc.get("elapsed_s")}


# -------------------------------------------------------------------- run

def _automation_run(ctx: Context, filter: str, timeout: float = _RUN_TIMEOUT_DEFAULT,
                    interval: float = _POLL_INTERVAL) -> Dict[str, Any]:
    """`Automation Now; RunTests <filter>` y recoleccion de resultados."""
    filt = _sanitize_filter(filter)
    if not filt:
        return {"success": False, "message": "'filter' obligatorio (no vacio)"}
    if len(filt) > 400:
        return {"success": False, "message": "filter demasiado largo (max 400)"}
    try:
        timeout = max(5.0, min(float(timeout), _TIMEOUT_MAX))
        interval = max(0.2, min(float(interval), 5.0))
    except (TypeError, ValueError):
        return {"success": False, "message": "timeout/interval invalidos"}

    acc: Dict[str, Any] = {"results": []}
    _run_exec_and_poll(f"Automation Now; RunTests {filt}",
                       ["LogAutomationCommandLine", "LogAutomationController"],
                       acc, timeout, interval)

    if acc.get("exec_error") is not None:
        return {"success": False, "message": f"exec fallo: {acc['exec_error']}"}
    if acc.get("poll_error"):
        return {"success": False, "message": acc["poll_error"],
                "filter": filt, "since_seq": acc.get("since_seq"),
                "results": acc["results"]}
    if acc.get("no_match"):
        return {"success": False, "no_match": True, "filter": filt,
                "message": acc.get("error") or f"no tests matched '{filt}'",
                "since_seq": acc.get("since_seq")}
    if "already Queued" in str(acc.get("exec_output", "")):
        # Otra corrida sigue en cola: no reportar falsos resultados.
        return {"success": False, "busy": True, "filter": filt,
                "message": str(acc.get("exec_output")).strip(),
                "since_seq": acc.get("since_seq")}
    if acc.get("timed_out") and not acc.get("queue_empty"):
        return {"success": False, "timed_out": True, "filter": filt,
                "message": "timeout esperando el cierre de la corrida",
                "found": acc.get("found"),
                "results": acc["results"],
                "since_seq": acc.get("since_seq"),
                "elapsed_s": acc.get("elapsed_s")}

    results = acc["results"]
    performed = acc.get("performed")
    rbuckets = 0
    if isinstance(performed, int) and len(results) < performed:
        # >1000 `Test Completed` entre dos polls dejarian los mas viejos
        # fuera del `limit`: re-pedir por tipo de resultado.
        results, rbuckets = _fill_results(results, int(acc.get("since_seq") or 0),
                                          performed)
    failed = [r for r in results if r.get("result") != "Success"]
    missing = (performed - len(results)) if isinstance(performed, int) else 0
    return {"success": True, "filter": filt, "found": acc.get("found"),
            "performed": performed,
            "count": len(results),
            "ok": not failed, "failed_count": len(failed),
            "results": results,
            "results_partial": missing > 0,
            "missing": missing if missing > 0 else 0,
            "buckets": rbuckets,
            "errors": acc.get("errors") or [],
            "since_seq": acc.get("since_seq"),
            "elapsed_s": acc.get("elapsed_s")}


# --------------------------------------------------------------- results

def _automation_results(ctx: Context, since_seq: float = 0,
                        limit: int = 200) -> Dict[str, Any]:
    """Lee resultados ya emitidos (LogAutomationController) desde una seq."""
    try:
        since = max(0.0, float(since_seq))
        limit = max(1, min(int(limit), 1000))
    except (TypeError, ValueError):
        return {"success": False, "message": "since_seq/limit invalidos"}
    if since == 0:
        since = float(_snapshot_seq())

    results: List[Dict[str, Any]] = []
    errors: List[str] = []
    for cat, parser in (("LogAutomationController", _parse_controller),
                        ("LogAutomationCommandLine", _parse_cmdline)):
        acc: Dict[str, Any] = {"results": results}
        entries = _poll_retry(int(since), cat, limit=limit)
        if entries is None:
            return {"success": False,
                    "message": f"transporte: sin respuesta al leer '{cat}'",
                    "since_seq": since}
        parser(entries, acc)
        errors.extend(acc.get("errors") or [])
        if cat == "LogAutomationCommandLine":
            for e in entries:
                msg = str(e.get("message", ""))
                if _RE_NO_MATCH.match(msg):
                    errors.append(msg)

    return {"success": True, "since_seq": since,
            "count": len(results), "results": results,
            "errors": errors,
            "next_seq": float(_snapshot_seq())}


def register_automation_tools(mcp: FastMCP):
    """Registra el router `unreal_automation` (tools planas automation_*)."""
    ACTIONS = {'list': _automation_list, 'run': _automation_run,
               'results': _automation_results}
    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_automation",
        description=(
            "Tests automatizados de Unreal Engine (Automation framework):\n"
            "    \n"
            "    Flujo tipico:\n"
            "      1. automation_list() para ver los N tests registrados.\n"
            "      2. automation_run(filter=\"UObject.Field Cast\") para\n"
            "         correr uno o mas (filtro por prefijo/substring) y\n"
            "         recibir results[] con result=Success|Fail por test.\n"
            "      3. automation_results(since_seq=...) para releer sin\n"
            "         volver a correr.\n"
            "    \n"
            "    El editor paga un gate de framerate (~5s con la ventana\n"
            "    en background) antes de cada comando Automation: usar\n"
            "    timeout generoso (default 120-180s, max 720s). Nunca\n"
            "    emite Quit/SoftQuit.\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_automation(ctx: Context, action: str,
                          params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_automation action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_automation '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Automation tools (router) registered: {len(ACTIONS)} acciones")
