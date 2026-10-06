"""
Dominio `asset` para unreal-mcp: importar, crear y buscar assets (ciclo 11, bloque 3).

Tools planas (router `unreal_asset`):

  * `asset_import(source_path=..., path=..., name=..., replace=True)` —
    importa un archivo del disco a `/Game` con la factory registrada del
    formato (png/jpg/tga/exr/hdr, obj/fbx/gltf, csv, ...). `path` es la
    carpeta destino (defecto `/Game/Imported`), `name` el nombre del asset
    (defecto: nombre del archivo). Devuelve los paquetes creados.
  * `asset_find(name=..., class=..., path=..., recursive=True, limit=50)` —
    busca en el AssetRegistry. `name` es un subcadena del nombre del asset
    (insensible a mayusculas), `class` filtra por clase (`"Material"`,
    `"StaticMesh"`, `"/Script/Engine.Texture2D"`, ...), `path` por ruta de
    paquete (defecto `/Game`). Devuelve `total` y hasta `limit` resultados.
  * `asset_create(type=..., name=..., path=..., parent_class=...)` — crea un
    asset vacío: `type="material"` (UMaterial) o `type="blueprint"`
    (Blueprint vacío; `parent_class` por defecto `/Script/Engine.Actor`).
    Falla si el asset ya existe.

Notas de uso:
  * Para material con texturas ya existentes usa luego los tools de
    materiales (`material_*` / `unreal_material`); aqui solo se crea el
    asset base.
  * `asset_import` corre en GameThread: archivos muy grandes pueden superar
    el timeout del bridge (10 s) aunque la importación siga en curso.
"""
import logging
from typing import Any, Dict, Optional

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


def register_asset_tools(mcp: FastMCP):
    """Registra el router `unreal_asset` (tools planas `asset_*`)."""

    def import_asset(ctx: Context, source_path: str, path: str = "/Game/Imported",
                     name: str = "", replace: bool = True) -> Dict[str, Any]:
        """Importa un archivo del disco a /Game (cualquier formato con factory).

        Args:
            source_path: ruta absoluta del archivo a importar.
            path: carpeta destino, ej. "/Game/Textures" (defecto /Game/Imported).
            name: nombre del asset creado (defecto: nombre del archivo).
            replace: True para sobreescribir si ya existe.
        """
        source_path = str(source_path or "").strip()
        if not source_path:
            return {"success": False, "message": "'source_path' vacio"}
        return _send(ctx, "import_asset", {
            "source_path": source_path,
            "path": str(path or "").strip(),
            "name": str(name or "").strip(),
            "replace": bool(replace),
        })

    def find(ctx: Context, name: str = "", class_: str = "", path: str = "/Game",
             recursive: bool = True, limit: int = 50) -> Dict[str, Any]:
        """Busca assets en el AssetRegistry por nombre, clase y ruta.

        Args:
            name: subcadena del nombre del asset (insensible a mayusculas).
            class_: clase a filtrar, ej. "Material", "StaticMesh",
                "/Script/Engine.Texture2D".
            path: ruta de paquete base (defecto "/Game").
            recursive: True para buscar tambien en subcarpetas.
            limit: maximo de resultados devueltos (total siempre es completo).
        """
        return _send(ctx, "find_assets", {
            "name": str(name or "").strip(),
            "class": str(class_ or "").strip(),
            "path": str(path or "").strip(),
            "recursive": bool(recursive),
            "limit": int(limit),
        })

    def create(ctx: Context, type: str, name: str, path: str = "/Game",
               parent_class: str = "/Script/Engine.Actor") -> Dict[str, Any]:
        """Crea un asset vacío en /Game (material o blueprint).

        Args:
            type: "material" o "blueprint".
            name: nombre del asset (sin espacios ni puntos).
            path: carpeta destino (defecto "/Game").
            parent_class: solo para blueprint, clase base (defecto Actor).
        """
        type = str(type or "").strip().lower()
        name = str(name or "").strip()
        if not type or not name:
            return {"success": False, "message": "'type' y 'name' son obligatorios"}
        return _send(ctx, "create_asset", {
            "type": type,
            "name": name,
            "path": str(path or "").strip(),
            "parent_class": str(parent_class or "").strip(),
        })

    ACTIONS = {
        'import': import_asset,
        'find': find,
        'create': create,
    }

    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_asset",
        description=(
            "Assets de contenido Unreal (ciclo 11):\n"
            "    importar archivos a /Game, crear assets vacios y buscar\n"
            "    en el AssetRegistry.\n"
            "    \n"
            "    Flujo tipico: asset_import(source_path=...) para traer un\n"
            "    archivo; asset_create(type=\"material\"|\"blueprint\", ...)\n"
            "    para alta rapida; asset_find(name=..., class=...) para\n"
            "    localizar lo que ya existe.\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_asset(ctx: Context, action: str,
                     params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_asset action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as exc:
            return {"success": False, "message": f"Parametros invalidos para '{action}': {exc}"}
        except Exception as exc:
            logger.error(f"unreal_asset '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info(f"Asset tools (router) registered: {len(ACTIONS)} acciones")
