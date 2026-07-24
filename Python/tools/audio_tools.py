"""
Audio Tools for Unreal MCP.

Router unico de dominio 'unreal_audio' — sigue el mismo patron que
project_tools.py / umg_tools.py despues del refactor a routers:
una sola tool MCP (`unreal_audio`) que recibe `action` + `params` y
delega al comando C++ correspondiente vía unreal.send_command().
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

# Nombre de la accion -> docstring corta para el listado en el prompt del router
_AUDIO_ACTIONS = {
    "create_sound_attenuation": "create_sound_attenuation(name, path, inner_radius, falloff_distance, shape): Create a SoundAttenuation asset.",
    "create_sound_class": "create_sound_class(name, path, volume, pitch, parent_class_path): Create a SoundClass asset.",
    "create_sound_cue": "create_sound_cue(name, path, sound_wave_path, looping): Create a SoundCue asset wrapping a single SoundWave.",
    "spawn_ambient_sound": "spawn_ambient_sound(name, location, rotation, sound_path, volume_multiplier, pitch_multiplier, auto_activate): Spawn an AmbientSound actor in the level.",
    "set_ambient_sound_properties": "set_ambient_sound_properties(actor_name, sound_path, volume_multiplier, pitch_multiplier, attenuation_path, is_ui_sound): Update an existing AmbientSound actor.",
    "play_sound_2d": "play_sound_2d(sound_path, volume_multiplier, pitch_multiplier): Quick 2D playback for testing a sound asset.",
}


def register_audio_tools(mcp: FastMCP):
    """Register the unified 'unreal_audio' router tool with the MCP server."""

    actions_doc = "\n      - ".join([""] + list(_AUDIO_ACTIONS.values()))

    @mcp.tool(
        name="unreal_audio",
        description=(
            "Router para operaciones de dominio 'unreal_audio' en Unreal Engine.\n"
            "    \n"
            "    Parametros:\n"
            "      action: nombre de la operacion (ver lista abajo)\n"
            "      params: dict con los argumentos de esa operacion\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_audio(
        ctx: Context,
        action: str,
        params: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        params = params or {}

        if action not in _AUDIO_ACTIONS:
            known = ", ".join(sorted(_AUDIO_ACTIONS.keys()))
            return {
                "success": False,
                "message": f"Unknown unreal_audio action: '{action}'. Known actions: {known}",
            }

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            logger.info(f"unreal_audio: dispatching action '{action}' with params={params}")
            response = unreal.send_command(action, params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"unreal_audio '{action}' response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error executing unreal_audio action '{action}': {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Audio tools (router) registered successfully")
