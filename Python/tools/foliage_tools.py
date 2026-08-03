"""Foliage Tools for Unreal MCP - pintado/gestion de instancias de foliage."""
import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')


def register_foliage_tools(mcp: FastMCP):

    def _create_foliage_type(ctx: Context, mesh_path: str, foliage_type_name: str,
                              folder_path: str = "/Game/Foliage", density: float = 100.0,
                              min_scale: float = 1.0, max_scale: float = 1.0,
                              align_to_normal: bool = True, random_yaw: bool = True) -> Dict[str, Any]:
        """Crea y guarda a disco un UFoliageType_InstancedStaticMesh que envuelve un static mesh existente.

        mesh_path: path del UStaticMesh existente (ej "/Game/Meshes/SM_Rock").
        density: densidad por 1000uu^2 (solo relevante para pintado con brush, no afecta add_foliage_instances).
        min_scale/max_scale: rango de escala uniforme aplicado a X/Y/Z.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'mesh_path': mesh_path, 'foliage_type_name': foliage_type_name, 'folder_path': folder_path,
                      'density': density, 'min_scale': min_scale, 'max_scale': max_scale,
                      'align_to_normal': align_to_normal, 'random_yaw': random_yaw}
            return unreal.send_command('create_foliage_type', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error creating foliage type: {e}'}

    def _add_foliage_instances(ctx: Context, foliage_type_path: str, instances: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Agrega instancias de un foliage type en el nivel actual.

        instances: lista de dicts, cada uno con:
          location: [x, y, z] (requerido)
          rotation: [pitch, yaw, roll] (opcional, default [0,0,0])
          scale: numero (uniforme) o [x, y, z] (opcional, default 1.0)
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'foliage_type_path': foliage_type_path, 'instances': instances}
            return unreal.send_command('add_foliage_instances', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error adding foliage instances: {e}'}

    def _remove_foliage_instances(ctx: Context, foliage_type_path: str, indices: List[int]) -> Dict[str, Any]:
        """Borra instancias de un foliage type por indice, en el nivel actual."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'foliage_type_path': foliage_type_path, 'indices': indices}
            return unreal.send_command('remove_foliage_instances', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error removing foliage instances: {e}'}

    def _list_foliage_types(ctx: Context) -> Dict[str, Any]:
        """Lista los foliage types ya pintados en el nivel actual con su conteo de instancias."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('list_foliage_types', {}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error listing foliage types: {e}'}

    ACTIONS = {
        'create_foliage_type': _create_foliage_type,
        'add_foliage_instances': _add_foliage_instances,
        'remove_foliage_instances': _remove_foliage_instances,
        'list_foliage_types': _list_foliage_types,
    }

    @mcp.tool()
    def unreal_foliage(ctx: Context, action: str, params: Dict[str, Any] = {}) -> Any:
        """Router para operaciones de dominio 'unreal_foliage' en Unreal Engine
        (pintado/gestion de instancias de foliage sobre el nivel actual).

        Operaciones disponibles:
          - create_foliage_type(mesh_path, foliage_type_name, folder_path, density, min_scale, max_scale, align_to_normal, random_yaw)
          - add_foliage_instances(foliage_type_path, instances)
          - remove_foliage_instances(foliage_type_path, indices)
          - list_foliage_types()
        """
        if action not in ACTIONS:
            return {'success': False, 'message': f"Acci\u00f3n desconocida '{action}'. Disponibles: {list(ACTIONS.keys())}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as e:
            return {'success': False, 'message': f"Par\u00e1metros inv\u00e1lidos para acci\u00f3n '{action}': {e}"}

    logger.info('Foliage tools registered successfully')
