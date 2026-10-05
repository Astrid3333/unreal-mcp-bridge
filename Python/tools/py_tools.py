"""
Dominio `python` para unreal-mcp: ejecutar Python dentro del editor (ciclo 11).

Bloques:

  * `python_exec(code, mode=..., scope=...)` ejecuta codigo Python en el hilo
    del editor a traves del plugin `PythonScriptPlugin` (FPythonCommandEx) y
    devuelve stdout/stderr capturados + el resultado evaluado.
  * `python_exec_file(path, scope=...)` lee un archivo .py local y lo ejecuta
    en el editor (mismo camino que `mode='file'`).

Modos de `python_exec`:
  * "file"      -> varias sentencias (ExecuteFile). No devuelve valor.
  * "statement" -> una sentencia con print del resultado (ExecuteStatement).
  * "evaluate"  -> una expresion; el valor queda en `result`
    (EvaluateStatement).

Scopes:
  * "public" (defecto): las variables persisten entre llamadas (igual que la
    consola de Python del editor).
  * "private": cada ejecucion usa su propio dict aislado.

Notas de uso:
  * El codigo corre en el GameThread: no bloquear (red, sleep largo) ni
    abrir modales; el bridge corta la espera tras `CommandTimeoutSeconds`.
  * Para el modulo `unreal` use las APIs de editor (EditorLevelLibrary,
    EditorActorSubsystem, AssetTools, ...) disponibles en el hilo del editor.
  * Un error de sintaxis o excepcion no truena el editor: devuelve
    `success=False` con el traceback en `result`.
"""
import logging
import os
from typing import Any, Dict, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')

_MAX_CODE_BYTES = 512 * 1024


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


def register_python_tools(mcp: FastMCP):
    """Registra el router `unreal_python` (tools planas `python_*`)."""

    def exec_code(ctx: Context, code: str, mode: str = "file",
                  scope: str = "public") -> Dict[str, Any]:
        """Ejecuta Python dentro del editor (GameThread) y devuelve su salida.

        Args:
            code: codigo Python a ejecutar (max 512 KB).
            mode: "file" (varias sentencias, sin valor de retorno),
                "statement" (una sentencia, imprime el resultado) o
                "evaluate" (una expresion; el valor queda en `result`).
            scope: "public" (las variables persisten entre llamadas) o
                "private" (dict aislado por ejecucion).
        """
        if not str(code).strip():
            return {"success": False, "message": "'code' vacio"}
        if len(code.encode("utf-8")) > _MAX_CODE_BYTES:
            return {"success": False,
                    "message": f"'code' excede {_MAX_CODE_BYTES} bytes; use python_exec_file"}
        if mode not in ("file", "statement", "evaluate"):
            return {"success": False, "message": "mode debe ser file, statement o evaluate"}
        if scope not in ("public", "private"):
            return {"success": False, "message": "scope debe ser public o private"}
        return _send(ctx, "execute_python",
                     {"code": code, "mode": mode, "scope": scope})

    def exec_file(ctx: Context, path: str, scope: str = "public") -> Dict[str, Any]:
        """Lee un archivo .py legible por el servidor MCP y lo ejecuta en el editor.

        Args:
            path: ruta absoluta (o relativa al workspace) del archivo .py.
            scope: "public" (variables persisten) o "private".
        """
        if scope not in ("public", "private"):
            return {"success": False, "message": "scope debe ser public o private"}
        path = str(path).strip()
        if not path:
            return {"success": False, "message": "'path' vacio"}
        if not os.path.isfile(path):
            return {"success": False, "message": f"Archivo no encontrado: {path}"}
        if not path.endswith(".py"):
            return {"success": False, "message": "Solo archivos .py"}
        try:
            size = os.path.getsize(path)
            if size > _MAX_CODE_BYTES:
                return {"success": False, "message": f"Archivo de {size} bytes excede {_MAX_CODE_BYTES}"}
            with open(path, "r", encoding="utf-8") as fh:
                code = fh.read()
        except OSError as exc:
            return {"success": False, "message": f"No se pudo leer {path}: {exc}"}
        result = _send(ctx, "execute_python",
                       {"code": code, "mode": "file", "scope": scope})
        if isinstance(result, dict):
            result["path"] = path
        return result

    ACTIONS = {
        'exec': exec_code,
        'exec_file': exec_file,
    }

    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_python",
        description=(
            "Ejecutar Python dentro del editor Unreal via PythonScriptPlugin\n"
            "    (FPythonCommandEx en GameThread): acceso completo al modulo\n"
            "    `unreal` sin tools dedicadas.\n"
            "    \n"
            "    Flujo tipico: python_exec(code=\"import unreal; ...\",\n"
            "    mode='file') para scripts; mode='evaluate' para expresiones\n"
            "    (valor en `result`); scope='public' mantiene variables entre\n"
            "    llamadas. python_exec_file(path=...) corre un .py del disco.\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_python(ctx: Context, action: str,
                      params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_python action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_python '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Python tools (router) registered: {len(ACTIONS)} acciones")
