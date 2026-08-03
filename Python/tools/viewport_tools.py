"""Viewport Tools for Unreal MCP - camara y modo de visualizacion del viewport del editor."""
import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')


def register_viewport_tools(mcp: FastMCP):

    def _get_viewport_camera_info(ctx: Context) -> Dict[str, Any]:
        """Devuelve posicion, rotacion, FOV y view mode actuales de la camara del viewport activo."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('get_viewport_camera_info', {}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error getting viewport camera info: {e}'}

    def _set_viewport_camera(ctx: Context, location: Optional[List[float]] = None,
                              rotation: Optional[List[float]] = None) -> Dict[str, Any]:
        """Mueve la camara del viewport activo.

        location: [x, y, z] en unidades de mundo (opcional).
        rotation: [pitch, yaw, roll] en grados (opcional).
        Si se omite uno de los dos, ese componente no se toca.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params: Dict[str, Any] = {}
            if location is not None:
                params['location'] = location
            if rotation is not None:
                params['rotation'] = rotation
            return unreal.send_command('set_viewport_camera', params) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting viewport camera: {e}'}

    def _set_viewport_fov(ctx: Context, fov: float) -> Dict[str, Any]:
        """Setea el campo de vision (horizontal, en grados) del viewport activo."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('set_viewport_fov', {'fov': fov}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting viewport FOV: {e}'}

    def _set_viewport_view_mode(ctx: Context, view_mode: str) -> Dict[str, Any]:
        """Cambia el modo de visualizacion del viewport activo.

        view_mode: Lit, Unlit, Wireframe, BrushWireframe, LightingOnly,
          DetailLighting, ShaderComplexity, LightComplexity.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('set_viewport_view_mode', {'view_mode': view_mode}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting viewport view mode: {e}'}

    ACTIONS = {
        'get_viewport_camera_info': _get_viewport_camera_info,
        'set_viewport_camera': _set_viewport_camera,
        'set_viewport_fov': _set_viewport_fov,
        'set_viewport_view_mode': _set_viewport_view_mode,
    }

    @mcp.tool()
    def unreal_viewport(ctx: Context, action: str, params: Dict[str, Any] = {}) -> Any:
        """Router para operaciones de dominio 'unreal_viewport' en Unreal Engine
        (camara y modo de visualizacion del viewport del editor).

        Operaciones disponibles:
          - get_viewport_camera_info()
          - set_viewport_camera(location, rotation)
          - set_viewport_fov(fov)
          - set_viewport_view_mode(view_mode)
        """
        if action not in ACTIONS:
            return {'success': False, 'message': f"Acción desconocida '{action}'. Disponibles: {list(ACTIONS.keys())}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as e:
            return {'success': False, 'message': f"Parámetros inválidos para acción '{action}': {e}"}

    logger.info('Viewport tools registered successfully')
