"""
Sequencer MCP tools -- router que delega al comando C++ correspondiente vía unreal.send_command().
"""
import logging
from typing import Dict, Any, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

_SEQUENCER_ACTIONS = {
    "create_level_sequence": "create_level_sequence(name, package_path, fps, length_seconds): Create a new LevelSequence asset.",
    "add_actor_to_sequence": "add_actor_to_sequence(sequence_path, actor_name): Bind an existing level actor into the sequence as a possessable.",
    "add_camera_cut_track": "add_camera_cut_track(sequence_path, camera_actor_name): Add a Camera Cuts track bound to an already-bound camera actor.",
    "set_playback_range": "set_playback_range(sequence_path, start_seconds, end_seconds): Set the sequence's playback range.",
    "open_level_sequence": "open_level_sequence(sequence_path): Open the sequence in the Sequencer editor window.",
    "add_transform_keyframe": "add_transform_keyframe(sequence_path, actor_name, time_seconds, location=None, rotation=None, scale=None): Add a keyframe on the transform track for a bound actor at the given time. Provide at least one of location/rotation/scale as a 3-element [x, y, z] array.",
    "add_property_keyframe": "add_property_keyframe(sequence_path, actor_name, property_name, time_seconds, value): Add a keyframe on a float property track of the actor's LightComponent (e.g. property_name='Intensity') at the given time, with the given float value.",
}


def register_sequencer_tools(mcp: FastMCP):
    """Register the unified 'unreal_sequencer' router tool with the MCP server."""

    actions_doc = "\n      - ".join([""] + list(_SEQUENCER_ACTIONS.values()))

    @mcp.tool(
        name="unreal_sequencer",
        description=(
            "Router para operaciones de dominio 'unreal_sequencer' en Unreal Engine.\n"
            "    \n"
            "    Parametros:\n"
            "      action: nombre de la operacion (ver lista abajo)\n"
            "      params: dict con los argumentos de esa operacion\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_sequencer(
        ctx: Context,
        action: str,
        params: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        params = params or {}

        if action not in _SEQUENCER_ACTIONS:
            known = ", ".join(sorted(_SEQUENCER_ACTIONS.keys()))
            return {
                "success": False,
                "message": f"Unknown unreal_sequencer action: '{action}'. Known actions: {known}",
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
            logger.info(f"unreal_sequencer '{action}' response: {response}")
            return response
        except Exception as e:
            error_msg = f"Error executing unreal_sequencer action '{action}': {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Sequencer tools (router) registered successfully")
