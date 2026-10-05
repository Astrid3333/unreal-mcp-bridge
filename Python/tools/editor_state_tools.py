"""
Dominio `editor` para unreal-mcp: estado y control del editor (ciclo 11, bloque 2).

Tools planas (router `unreal_editor`):

  * `editor_get_state()` — mapa activo (nombre, dirty, nº de actores),
    selección (nombres), `can_undo`/`can_redo` y estado de sesión de juego
    (playing / request_queued / play_world / play_world_type).
  * `editor_get_selection()` — solo la selección actual (nombres + count).
  * `editor_set_selection(names=[...])` — reemplaza la selección por los
    nombres indicados (lista vacía = limpiar). Devuelve `not_found` con los
    nombres que no existen en el nivel activo.
  * `editor_undo()` / `editor_redo()` — deshacer/rehacer vía
    `GEditor->Trans` (UTransactor). Si el buffer está vacío devuelve
    `success=False` con mensaje ("Nada que deshacer").
  * `editor_save_level(path=..., content=False)` — guarda el nivel actual
    sin diálogos (UEditorLoadingAndSavingUtils::SaveMap /
    FEditorFileUtils::SaveMap). `path` es obligatorio si el mapa todavía
    no tiene ruta (/Temp/...); `content=True` guarda además los paquetes
    de contenido sucios.
  * `editor_open_level(path=..., discard_changes=False)` — abre/carga un
    nivel por ruta de paquete (ej. "/Game/Maps/Untitled"). Si hay cambios
    sin guardar falla, salvo que pases `discard_changes=True`.

Notas de uso:
  * Corre en el GameThread: nada de operaciones largas (el bridge corta la
    espera tras `CommandTimeoutSeconds`).
  * Los cambios de selección NO son transaccionales: no aparecen en el
    buffer de undo. El undo/redo real aplica a operaciones de edición que
    abren transacción (borrar, mover con transacción, etc.).
  * `editor_open_level` recarga el mapa: cualquier trabajo no guardado se
    pierde si pasas `discard_changes=True`.
"""
import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')


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


def register_editor_state_tools(mcp: FastMCP):
    """Registra el router `unreal_editor` (tools planas `editor_*`)."""

    def get_state(ctx: Context) -> Dict[str, Any]:
        """Estado completo del editor: mapa, selección, undo/redo, sesión."""
        return _send(ctx, "get_editor_state", {})

    def get_selection(ctx: Context) -> Dict[str, Any]:
        """Selección actual del editor (nombres de actor + count)."""
        return _send(ctx, "get_selection", {})

    def set_selection(ctx: Context, names: Optional[List[str]] = None) -> Dict[str, Any]:
        """Reemplaza la selección por los nombres de actor dados.

        Args:
            names: nombres exactos de actor en el nivel activo.
                Lista vacía limpia la selección.
        """
        if names is None:
            names = []
        if not isinstance(names, (list, tuple)):
            return {"success": False, "message": "'names' debe ser una lista"}
        names = [str(n) for n in names]
        return _send(ctx, "set_selection", {"names": names})

    def undo(ctx: Context) -> Dict[str, Any]:
        """Deshace la última transacción del editor (Edit > Undo)."""
        return _send(ctx, "editor_undo", {})

    def redo(ctx: Context) -> Dict[str, Any]:
        """Rehace la última transacción deshecha (Edit > Redo)."""
        return _send(ctx, "editor_redo", {})

    def save_level(ctx: Context, path: str = "",
                   content: bool = False) -> Dict[str, Any]:
        """Guarda el nivel activo (sin diálogos: nunca abre "Save As").

        Args:
            path: ruta de paquete destino (ej. "/Game/Maps/MiMapa").
                Necesaria si el mapa todavía no tiene ruta (/Temp/...);
                opcional si el mapa ya vive en /Game (usa su ruta actual).
            content: True para guardar también los paquetes de contenido
                modificados (SaveDirtyPackages), no solo el mapa.
        """
        path = str(path or "").strip()
        return _send(ctx, "save_current_level", {"path": path, "content": bool(content)})

    def open_level(ctx: Context, path: str,
                   discard_changes: bool = False) -> Dict[str, Any]:
        """Abre/carga un nivel por ruta de paquete.

        Args:
            path: ruta del paquete, ej. "/Game/Maps/Untitled".
            discard_changes: True para recargar aunque haya cambios sin
                guardar (los pierde); False (defecto) falla en ese caso.
        """
        path = str(path or "").strip()
        if not path:
            return {"success": False, "message": "'path' vacio"}
        return _send(ctx, "open_level",
                     {"path": path, "discard_changes": bool(discard_changes)})

    ACTIONS = {
        'get_state': get_state,
        'get_selection': get_selection,
        'set_selection': set_selection,
        'undo': undo,
        'redo': redo,
        'save_level': save_level,
        'open_level': open_level,
    }

    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_editor",
        description=(
            "Estado y control del editor Unreal (ciclo 11):\n"
            "    mapa activo, dirty, selección, undo/redo y guardar/abrir\n"
            "    nivel.\n"
            "    \n"
            "    Flujo tipico: editor_get_state() para ver el mapa, la\n"
            "    selección y si hay undo disponible; editor_set_selection\n"
            "    (names=[...]) para cambiar la selección; editor_undo /\n"
            "    editor_redo sobre transacciones de edición;\n"
            "    editor_save_level() y editor_open_level(path=...).\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_editor(ctx: Context, action: str,
                      params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_editor action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_editor '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Editor state tools (router) registered: {len(ACTIONS)} acciones")
