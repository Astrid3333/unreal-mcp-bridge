"""Rendering Tools for Unreal MCP - control generico de render quality via cvars."""
import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')


def register_rendering_tools(mcp: FastMCP):

    def _get_cvar(ctx: Context, cvar_name: str) -> Dict[str, Any]:
        """Lee el valor actual de una consola variable (cvar) de Unreal.

        cvar_name: nombre completo, ej 'r.ScreenPercentage', 'r.PostProcessAAQuality',
          'r.RayTracing.Enable', 'r.TemporalAA.Upsampling', 'sg.AntiAliasingQuality'.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('get_cvar', {'cvar_name': cvar_name}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error getting cvar: {e}'}

    def _set_cvar(ctx: Context, cvar_name: str, value: str) -> Dict[str, Any]:
        """Setea una consola variable (cvar) de Unreal.

        cvar_name: nombre completo (ver get_cvar para ejemplos).
        value: valor como string -- Unreal lo parsea al tipo correcto segun la
          cvar (ej '75' para r.ScreenPercentage, '1'/'0' para r.RayTracing.Enable).
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            return unreal.send_command('set_cvar', {'cvar_name': cvar_name, 'value': value}) or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting cvar: {e}'}

    ACTIONS = {
        'get_cvar': _get_cvar,
        'set_cvar': _set_cvar,
    }

    @mcp.tool()
    def unreal_rendering(ctx: Context, action: str, params: Dict[str, Any] = {}) -> Any:
        """Router para operaciones de dominio 'unreal_rendering' en Unreal Engine
        (control generico de calidad de render via consola: screen percentage,
        anti-aliasing, ray tracing, y cualquier otra cvar r.*/sg.*).

        Operaciones disponibles:
          - get_cvar(cvar_name)
          - set_cvar(cvar_name, value)
        """
        if action not in ACTIONS:
            return {'success': False, 'message': f"Acción desconocida '{action}'. Disponibles: {list(ACTIONS.keys())}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as e:
            return {'success': False, 'message': f"Parámetros inválidos para acción '{action}': {e}"}

    logger.info('Rendering tools registered successfully')
