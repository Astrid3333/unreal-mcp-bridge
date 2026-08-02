"""
Landscape MCP tools -- router que delega al comando C++ correspondiente vía unreal.send_command().
"""
import logging
from typing import Dict, Any, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

_LANDSCAPE_ACTIONS = {
    "get_landscape_info": "get_landscape_info(landscape_name=None): Devuelve el extent en coordenadas de quad (min_x/min_y/max_x/max_y), escala, tamaño de componente, las capas (layers) disponibles con LayerInfo asset asignado, material_path (asset del Landscape Material), has_layer_blend_node (bool) y layer_blend_node_layers (nombres ya blendeados en el nodo LandscapeLayerBlend). Llamar siempre esto primero antes de sculpt/paint/add_landscape_material_layer_blend_input para conocer el rango válido y si el material ya está preparado.",
    "sculpt_landscape_region": "sculpt_landscape_region(x, y, radius=20, strength_cm=100, mode='raise', target_height_cm=None, landscape_name=None): Esculpe una región circular del heightmap. x/y son coordenadas de quad (no cm de mundo). mode: 'raise', 'lower', 'flatten' (requiere target_height_cm) o 'noise'.",
    "paint_landscape_layer": "paint_landscape_layer(layer_name, x, y, radius=20, strength=1.0, landscape_name=None): Pinta el peso de una capa de material en una región circular. x/y en coordenadas de quad. strength es 0.0-1.0.",
    "create_landscape_layer_info": "create_landscape_layer_info(layer_name, landscape_name=None, blend_mode='WeightBlend'): Crea un asset ULandscapeLayerInfoObject nuevo en /Game/Landscape/Layers/<layer_name> y lo registra como target layer del landscape. blend_mode: 'WeightBlend' o 'NoWeightBlend'. Falla si la capa ya tiene un LayerInfo asset asignado -- llamar a get_landscape_info primero.",
    "add_landscape_material_layer_blend_input": "add_landscape_material_layer_blend_input(layer_name, landscape_name=None, color_r=0.0, color_g=1.0, color_b=0.0): Agrega una entrada nueva al nodo LandscapeLayerBlend del Landscape Material, pintada con un color plano. REQUIERE que el material ya tenga un nodo LandscapeLayerBlend conectado a Base Color -- si no lo tiene, falla pidiendo agregarlo a mano una vez en el Material Editor.",
    "create_landscape_material_with_layer_blend": "create_landscape_material_with_layer_blend(name, layer_names, path='/Game/Landscape/Materials', assign_to_landscape=False, landscape_name=None): Crea un Material nuevo con un nodo LandscapeLayerBlend (una entrada por nombre en layer_names, cada una con un color plano de fallback) conectado a Base Color, lo guarda en disco, y opcionalmente lo asigna al landscape en el mismo paso (assign_to_landscape=True).",
    "set_landscape_material": "set_landscape_material(material_path, landscape_name=None): Asigna un Material/MaterialInstance ya existente (por asset path) como el Landscape Material del landscape. Usar esto (no set_actor_material de unreal_actor, que es para StaticMeshActor) para landscapes.",
}


def register_landscape_tools(mcp: FastMCP):
    """Register the unified 'unreal_landscape' router tool with the MCP server."""

    actions_doc = "\n      - ".join([""] + list(_LANDSCAPE_ACTIONS.values()))

    @mcp.tool(
        name="unreal_landscape",
        description=(
            "Router para operaciones de dominio 'unreal_landscape' en Unreal Engine.\n"
            "    \n"
            "    Parametros:\n"
            "      action: nombre de la operacion (ver lista abajo)\n"
            "      params: dict con los argumentos de esa operacion\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_landscape(
        ctx: Context,
        action: str,
        params: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        params = params or {}

        if action not in _LANDSCAPE_ACTIONS:
            known = ", ".join(sorted(_LANDSCAPE_ACTIONS.keys()))
            return {
                "success": False,
                "message": f"Unknown unreal_landscape action: '{action}'. Known actions: {known}",
            }

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            response = unreal.send_command(action, params)
            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}
            logger.info(f"unreal_landscape '{action}' response: {response}")
            return response
        except Exception as e:
            error_msg = f"Error executing unreal_landscape action '{action}': {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Landscape tools (router) registered successfully")
