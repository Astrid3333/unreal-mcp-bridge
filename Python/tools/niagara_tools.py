"""
Niagara Tools for Unreal MCP.
Router unico de dominio 'unreal_niagara' — sigue el mismo patron que
audio_tools.py / project_tools.py despues del refactor a routers:
una sola tool MCP (`unreal_niagara`) que recibe `action` + `params` y
delega al comando C++ correspondiente vía unreal.send_command().
"""
import logging
from typing import Dict, Any, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

# Nombre de la accion -> docstring corta para el listado en el prompt del router
_NIAGARA_ACTIONS = {
    "spawn_niagara_system": "spawn_niagara_system(name, system_path, location, rotation, scale, auto_activate): Spawn a NiagaraActor referencing a NiagaraSystem asset.",
    "set_niagara_float_parameter": "set_niagara_float_parameter(actor_name, parameter_name, value): Set a User-exposed float parameter on a spawned Niagara component.",
    "set_niagara_vector_parameter": "set_niagara_vector_parameter(actor_name, parameter_name, x, y, z): Set a User-exposed vector parameter on a spawned Niagara component.",
    "set_niagara_color_parameter": "set_niagara_color_parameter(actor_name, parameter_name, r, g, b, a): Set a User-exposed linear color parameter on a spawned Niagara component.",
    "activate_niagara_component": "activate_niagara_component(actor_name, reset): Activate (or restart) a Niagara component's effect.",
    "deactivate_niagara_component": "deactivate_niagara_component(actor_name): Deactivate a Niagara component's effect.",
    "add_niagara_user_parameter": "add_niagara_user_parameter(system_path, parameter_name, parameter_type): Add a User-exposed parameter (float/vector/color) to a NiagaraSystem asset.",
    "list_niagara_user_parameters": "list_niagara_user_parameters(system_path): List the User-exposed parameters already on a NiagaraSystem asset.",
}


def register_niagara_tools(mcp: FastMCP):
    """Register the unified 'unreal_niagara' router tool with the MCP server."""

    actions_doc = "\n      - ".join([""] + list(_NIAGARA_ACTIONS.values()))

    @mcp.tool(
        name="unreal_niagara",
        description=(
            "Router para operaciones de dominio 'unreal_niagara' en Unreal Engine.\n"
            "    \n"
            "    Parametros:\n"
            "      action: nombre de la operacion (ver lista abajo)\n"
            "      params: dict con los argumentos de esa operacion\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_niagara(
        ctx: Context,
        action: str,
        params: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        params = params or {}

        if action not in _NIAGARA_ACTIONS:
            known = ", ".join(sorted(_NIAGARA_ACTIONS.keys()))
            return {
                "success": False,
                "message": f"Unknown unreal_niagara action: '{action}'. Known actions: {known}",
            }

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}
            logger.info(f"unreal_niagara: dispatching action '{action}' with params={params}")
            response = unreal.send_command(action, params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            logger.info(f"unreal_niagara '{action}' response: {response}")
            return response
        except Exception as e:
            error_msg = f"Error executing unreal_niagara action '{action}': {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Niagara tools (router) registered successfully")
