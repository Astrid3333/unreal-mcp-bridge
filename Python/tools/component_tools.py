"""Component Tools for Unreal MCP - control genérico de propiedades de componentes."""
import logging
from typing import Dict, List, Any
from mcp.server.fastmcp import FastMCP, Context
logger = logging.getLogger('UnrealMCP')
def register_component_tools(mcp: FastMCP):
    def _list_components(ctx: Context, actor_name: str) -> Dict[str, Any]:
        """Lista todos los componentes de un actor con su clase."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            response = unreal.send_command('list_components', {'actor_name': actor_name})
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error listing components: {e}'}
    def _get_component_property(ctx: Context, actor_name: str, component_name: str, property_path: str) -> Dict[str, Any]:
        """Lee cualquier propiedad de un componente, incluso anidada en un struct (ej: 'LightColor.R')."""
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'actor_name': actor_name, 'component_name': component_name, 'property_path': property_path}
            response = unreal.send_command('get_component_property', params)
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error getting component property: {e}'}
    def _set_component_property(ctx: Context, actor_name: str, component_name: str, property_path: str, value) -> Dict[str, Any]:
        """Escribe cualquier propiedad de un componente.
        value puede ser: bool, número, string, o array [x,y,z]/[r,g,b,a] para
        FVector/FRotator/FLinearColor/FColor. Para enums, pasar el nombre como string.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            params = {'actor_name': actor_name, 'component_name': component_name, 'property_path': property_path, 'value': value}
            response = unreal.send_command('set_actor_component_property', params)
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error setting component property: {e}'}
    def _batch_get_component_properties(ctx: Context, items: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Lee varias propiedades de componentes en una sola llamada.
        items: lista de {actor_name, component_name, property_path}.
        Devuelve {success, results: [{actor_name, component_name, property_path, success, value|error}, ...]}.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            response = unreal.send_command('batch_get_component_properties', {'items': items})
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error batch-getting component properties: {e}'}
    def _batch_set_component_properties(ctx: Context, items: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Escribe varias propiedades de componentes en una sola llamada.
        items: lista de {actor_name, component_name, property_path, value}.
        Un ítem inválido no aborta el resto del batch; revisar 'success'/'error' por ítem en la respuesta.
        Devuelve {success, results: [{actor_name, component_name, property_path, success, error?}, ...]}.
        """
        from unreal_mcp_server import get_unreal_connection
        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {'success': False, 'message': 'Failed to connect to Unreal Engine'}
            response = unreal.send_command('batch_set_component_properties', {'items': items})
            return response or {}
        except Exception as e:
            return {'success': False, 'message': f'Error batch-setting component properties: {e}'}
    ACTIONS = {
        'list_components': _list_components,
        'get_component_property': _get_component_property,
        'set_component_property': _set_component_property,
        'batch_get_component_properties': _batch_get_component_properties,
        'batch_set_component_properties': _batch_set_component_properties,
    }
    @mcp.tool()
    def unreal_component(ctx: Context, action: str, params: Dict[str, Any] = {}) -> Any:
        """Router para operaciones de dominio 'unreal_component' en Unreal Engine (control genérico de propiedades de componentes).
        Operaciones disponibles:
          - list_components(actor_name): Lista todos los componentes de un actor.
          - get_component_property(actor_name, component_name, property_path): Lee cualquier propiedad de un componente.
          - set_component_property(actor_name, component_name, property_path, value): Escribe cualquier propiedad de un componente.
          - batch_get_component_properties(items): Lee varias propiedades en una sola llamada. items: [{actor_name, component_name, property_path}, ...].
          - batch_set_component_properties(items): Escribe varias propiedades en una sola llamada. items: [{actor_name, component_name, property_path, value}, ...].
        """
        if action not in ACTIONS:
            return {'success': False, 'message': f"Acción desconocida '{action}'. Disponibles: {list(ACTIONS.keys())}"}
        try:
            return ACTIONS[action](ctx, **params)
        except TypeError as e:
            return {'success': False, 'message': f"Parámetros inválidos para acción '{action}': {e}"}
    logger.info('Component tools registered successfully')
