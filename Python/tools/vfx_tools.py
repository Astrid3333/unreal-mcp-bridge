"""
VFX / efectos especiales para Unreal MCP.

Compone comandos ya soportados por el plugin (spawn_actor, set_actor_property,
set_cvar) en tools de alto nivel para luces, volumenes de post-proceso,
camara y calidad de render. Todo verificado en vivo contra Unreal Editor 5.3.
"""
import logging
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger('UnrealMCP')

def register_vfx_tools(mcp: FastMCP):
    """Registra el router `unreal_vfx` (cada accion se publica como `vfx_<accion>`)."""

    def _send(ctx: Context, command: str, params: Dict[str, Any]) -> Any:
        from unreal_mcp_server import get_unreal_connection
        unreal = get_unreal_connection()
        if not unreal:
            return {"success": False, "message": "Failed to connect to Unreal Engine"}
        response = unreal.send_command(command, params)
        if not response:
            return {"success": False, "message": "No response from Unreal Engine"}
        if response.get("status") == "error":
            return {"success": False, "message": response.get("error", "unknown error")}
        return response.get("result", response)

    def _set_props(ctx: Context, actor_name: str, props: Dict[str, Any]) -> Dict[str, Any]:
        applied, errors = {}, {}
        for prop, value in props.items():
            if value is None:
                continue
            result = _send(ctx, "set_actor_property", {
                "name": actor_name,
                "property_name": prop,
                "property_value": value,
            })
            if isinstance(result, dict) and result.get("success"):
                applied[prop] = value
            else:
                errors[prop] = result.get("message", result) if isinstance(result, dict) else str(result)
        return {"success": not errors, "actor": actor_name, "applied": applied, "errors": errors}

    def _spawn(ctx: Context, name: str, actor_type: str, location: List[float],
               rotation: Optional[List[float]] = None) -> Dict[str, Any]:
        params: Dict[str, Any] = {"name": name, "type": actor_type, "location": location or [0.0, 0.0, 0.0]}
        if rotation:
            params["rotation"] = rotation
        return _send(ctx, "spawn_actor", params)

    # ------------------------------------------------------------------ luces
    def create_point_light(ctx: Context, name: str, location: List[float] = [0.0, 0.0, 300.0],
                           intensity: float = 5000.0, color: List[int] = [255, 255, 255],
                           attenuation_radius: float = 1000.0,
                           temperature: Optional[float] = None) -> Dict[str, Any]:
        """Crea un PointLight en el nivel.

        Args:
            name: nombre unico del actor.
            location: [x, y, z] en cm.
            intensity: intensidad en lumen (o unidades del modo de la luz).
            color: [r, g, b] 0-255.
            attenuation_radius: radio de caida en cm.
            temperature: temperatura de color en Kelvin (opcional, activa bUseTemperature).
        """
        result = _spawn(ctx, name, "PointLight", location)
        if isinstance(result, dict) and result.get("success") is False:
            return result
        props = {
            "Intensity": float(intensity),
            "LightColor": list(color) + [255] * (4 - len(color)),
            "AttenuationRadius": float(attenuation_radius),
        }
        if temperature is not None:
            props["bUseTemperature"] = True
            props["Temperature"] = float(temperature)
        applied = _set_props(ctx, name, props)
        return {"success": applied["success"], "actor": name, "applied": applied["applied"],
                "errors": applied["errors"], "spawned": result}

    def create_spot_light(ctx: Context, name: str, location: List[float] = [0.0, 0.0, 300.0],
                          rotation: Optional[List[float]] = None, intensity: float = 5000.0,
                          color: List[int] = [255, 255, 255], inner_cone_angle: float = 10.0,
                          outer_cone_angle: float = 45.0,
                          attenuation_radius: float = 1000.0) -> Dict[str, Any]:
        """Crea un SpotLight con conos interior/exterior.

        Args:
            name: nombre unico del actor.
            location: [x, y, z] en cm.
            rotation: [pitch, yaw, roll] grados (opcional).
            intensity: intensidad de la luz.
            color: [r, g, b] 0-255.
            inner_cone_angle: angulo interior en grados (brillo total).
            outer_cone_angle: angulo exterior en grados (borde de la luz).
            attenuation_radius: radio de caida en cm.
        """
        result = _spawn(ctx, name, "SpotLight", location, rotation)
        if isinstance(result, dict) and result.get("success") is False:
            return result
        props = {
            "Intensity": float(intensity),
            "LightColor": list(color) + [255] * (4 - len(color)),
            "InnerConeAngle": float(inner_cone_angle),
            "OuterConeAngle": float(outer_cone_angle),
            "AttenuationRadius": float(attenuation_radius),
        }
        applied = _set_props(ctx, name, props)
        return {"success": applied["success"], "actor": name, "applied": applied["applied"],
                "errors": applied["errors"], "spawned": result}

    def set_light_intensity(ctx: Context, actor_name: str, intensity: float) -> Dict[str, Any]:
        """Cambia la intensidad de una luz (PointLight/SpotLight/DirectionalLight)."""
        return _set_props(ctx, actor_name, {"Intensity": float(intensity)})

    def set_light_color(ctx: Context, actor_name: str, color: List[int]) -> Dict[str, Any]:
        """Cambia el color de una luz. color = [r, g, b] (0-255), opcion [r, g, b, a]."""
        rgb = list(color) + [255] * (4 - len(color))
        return _set_props(ctx, actor_name, {"LightColor": rgb[:4]})

    def set_light_attenuation(ctx: Context, actor_name: str, radius: float) -> Dict[str, Any]:
        """Cambia el radio de caida (AttenuationRadius, en cm) de una luz."""
        return _set_props(ctx, actor_name, {"AttenuationRadius": float(radius)})

    def set_light_temperature(ctx: Context, actor_name: str, temperature: float,
                              enabled: bool = True) -> Dict[str, Any]:
        """Activa la temperatura de color (Kelvin) de una luz: 1500 (calida) - 12000 (fria)."""
        return _set_props(ctx, actor_name, {"bUseTemperature": bool(enabled),
                                            "Temperature": float(temperature)})

    def set_light_visibility(ctx: Context, actor_name: str, visible: bool) -> Dict[str, Any]:
        """Enciende/apaga una luz (LightComponent.bVisible)."""
        return _set_props(ctx, actor_name, {"LightComponent.bVisible": bool(visible)})

    def set_light_property(ctx: Context, actor_name: str, property_name: str,
                           value: Any) -> Dict[str, Any]:
        """Set generico de propiedad de luz (ShadowBias, SourceRadius, VolumetricScatteringIntensity...).

        Args:
            actor_name: nombre del actor luz.
            property_name: propiedad con punto opcional, ej. "LightComponent.ShadowBias".
            value: valor JSON (numero, bool, cadena o lista para vectores/colores).
        """
        return _set_props(ctx, actor_name, {property_name: value})

    # ----------------------------------------------------------- post-proceso
    def create_post_process_volume(ctx: Context, name: str,
                                   location: List[float] = [0.0, 0.0, 0.0],
                                   unbound: bool = True) -> Dict[str, Any]:
        """Crea un PostProcessVolume. Con unbound=True aplica a toda la escena.

        Args:
            name: nombre unico del actor.
            location: [x, y, z] de la caja (si unbound=False).
            unbound: si True el volumen afecta toda la escena.
        """
        result = _spawn(ctx, name, "PostProcessVolume", location)
        if isinstance(result, dict) and result.get("success") is False:
            return result
        applied = _set_props(ctx, name, {"bEnabled": True, "bUnbound": bool(unbound)})
        return {"success": applied["success"], "actor": name, "applied": applied["applied"],
                "errors": applied["errors"], "spawned": result}

    def set_post_process_unbound(ctx: Context, volume_name: str, unbound: bool) -> Dict[str, Any]:
        """Activa/desactiva que el volumen de post-proceso aplique a toda la escena."""
        return _set_props(ctx, volume_name, {"bUnbound": bool(unbound)})

    def set_bloom(ctx: Context, volume_name: str, intensity: Optional[float] = None,
                  threshold: Optional[float] = None, override: bool = True) -> Dict[str, Any]:
        """Bloom (resplandor) de un PostProcessVolume. override=True marca bOverride_*. Args: volume_name, intensity, threshold, override."""
        props: Dict[str, Any] = {}
        if intensity is not None:
            props["Settings.bOverride_BloomIntensity"] = bool(override)
            props["Settings.BloomIntensity"] = float(intensity)
        if threshold is not None:
            props["Settings.bOverride_BloomThreshold"] = bool(override)
            props["Settings.BloomThreshold"] = float(threshold)
        if not props:
            return {"success": False, "message": "Pasa intensity y/o threshold"}
        return _set_props(ctx, volume_name, props)

    def set_exposure(ctx: Context, volume_name: str, bias: Optional[float] = None,
                     min_brightness: Optional[float] = None,
                     max_brightness: Optional[float] = None,
                     override: bool = True) -> Dict[str, Any]:
        """Exposicion automatica: bias (+ = mas claro), min/max brightness."""
        props: Dict[str, Any] = {}
        if bias is not None:
            props["Settings.bOverride_AutoExposureBias"] = bool(override)
            props["Settings.AutoExposureBias"] = float(bias)
        if min_brightness is not None:
            props["Settings.bOverride_AutoExposureMinBrightness"] = bool(override)
            props["Settings.AutoExposureMinBrightness"] = float(min_brightness)
        if max_brightness is not None:
            props["Settings.bOverride_AutoExposureMaxBrightness"] = bool(override)
            props["Settings.AutoExposureMaxBrightness"] = float(max_brightness)
        if not props:
            return {"success": False, "message": "Pasa bias, min_brightness o max_brightness"}
        return _set_props(ctx, volume_name, props)

    def set_motion_blur(ctx: Context, volume_name: str, amount: Optional[float] = None,
                        max_blur: Optional[float] = None, target_fps: Optional[int] = None,
                        override: bool = True) -> Dict[str, Any]:
        """Motion blur: amount (0 = off), max_blur, target_fps."""
        props: Dict[str, Any] = {}
        if amount is not None:
            props["Settings.bOverride_MotionBlurAmount"] = bool(override)
            props["Settings.MotionBlurAmount"] = float(amount)
        if max_blur is not None:
            props["Settings.bOverride_MotionBlurMax"] = bool(override)
            props["Settings.MotionBlurMax"] = float(max_blur)
        if target_fps is not None:
            props["Settings.MotionBlurTargetFPS"] = int(target_fps)
        if not props:
            return {"success": False, "message": "Pasa amount, max_blur o target_fps"}
        return _set_props(ctx, volume_name, props)

    def set_depth_of_field(ctx: Context, volume_name: str,
                           focal_distance: Optional[float] = None,
                           fstop: Optional[float] = None,
                           blur_amount: Optional[float] = None,
                           override: bool = True) -> Dict[str, Any]:
        """Depth of field: focal_distance (cm), fstop (apertura), blur_amount (bokeh)."""
        props: Dict[str, Any] = {}
        if focal_distance is not None:
            props["Settings.bOverride_DepthOfFieldFocalDistance"] = bool(override)
            props["Settings.DepthOfFieldFocalDistance"] = float(focal_distance)
        if fstop is not None:
            props["Settings.bOverride_DepthOfFieldFstop"] = bool(override)
            props["Settings.DepthOfFieldFstop"] = float(fstop)
        if blur_amount is not None:
            props["Settings.bOverride_DepthOfFieldDepthBlurAmount"] = bool(override)
            props["Settings.DepthOfFieldDepthBlurAmount"] = float(blur_amount)
        if not props:
            return {"success": False, "message": "Pasa focal_distance, fstop o blur_amount"}
        return _set_props(ctx, volume_name, props)

    def set_vignette(ctx: Context, volume_name: str, intensity: float,
                     override: bool = True) -> Dict[str, Any]:
        """Viñeta (oscurecimiento de bordes). intensity 0 = off, tipico 0.3 - 1.0."""
        return _set_props(ctx, volume_name, {
            "Settings.bOverride_VignetteIntensity": bool(override),
            "Settings.VignetteIntensity": float(intensity),
        })

    def set_film_grain(ctx: Context, volume_name: str, intensity: float,
                       override: bool = True) -> Dict[str, Any]:
        """Grano de pelicula. intensity 0 = off, tipico 0.1 - 0.5."""
        return _set_props(ctx, volume_name, {
            "Settings.bOverride_FilmGrainIntensity": bool(override),
            "Settings.FilmGrainIntensity": float(intensity),
        })

    def set_chromatic_aberration(ctx: Context, volume_name: str, start_offset: float,
                                 override: bool = True) -> Dict[str, Any]:
        """Aberracion cromatica en los bordes. 0 = off, tipico 0.05 - 0.5."""
        return _set_props(ctx, volume_name, {
            "Settings.bOverride_ChromaticAberrationStartOffset": bool(override),
            "Settings.ChromaticAberrationStartOffset": float(start_offset),
        })

    def set_lens_flare(ctx: Context, volume_name: str, intensity: float,
                       override: bool = True) -> Dict[str, Any]:
        """Lens flare. intensity 0 = off, tipico 0.5 - 2.0."""
        return _set_props(ctx, volume_name, {
            "Settings.bOverride_LensFlareIntensity": bool(override),
            "Settings.LensFlareIntensity": float(intensity),
        })

    def set_post_process_property(ctx: Context, volume_name: str, property_path: str,
                                  value: Any) -> Dict[str, Any]:
        """Set generico dentro de Settings.* (ej. "Settings.AmbientCubemapIntensity").

        Args:
            volume_name: nombre del PostProcessVolume.
            property_path: ruta con punto, siempre empezo por "Settings.".
            value: valor JSON (numero, bool, cadena o lista para colores/vectores).
        """
        return _set_props(ctx, volume_name, {property_path: value})

    # ----------------------------------------------------------------- camara
    def set_camera_fov(ctx: Context, camera_name: str, fov_degrees: float) -> Dict[str, Any]:
        """Cambia el FOV (grados) de un CameraActor. 90 = normal, 30 = teleobjetivo, 120 = gran angular."""
        return _set_props(ctx, camera_name, {"FOVAngle": float(fov_degrees)})

    def set_camera_depth_of_field(ctx: Context, camera_name: str,
                                  focal_distance: Optional[float] = None,
                                  fstop: Optional[float] = None,
                                  override: bool = True) -> Dict[str, Any]:
        """DOF en un CameraActor (PostProcessSettings de la camara).

        Args:
            camera_name: nombre del CameraActor.
            focal_distance: distancia focal en cm (punto de enfoque).
            fstop: apertura; menor = mas desenfoque.
            override: marca los flags bOverride_* (dejar True para que aplique).
        """
        props: Dict[str, Any] = {}
        if focal_distance is not None:
            props["PostProcessSettings.bOverride_DepthOfFieldFocalDistance"] = bool(override)
            props["PostProcessSettings.DepthOfFieldFocalDistance"] = float(focal_distance)
        if fstop is not None:
            props["PostProcessSettings.bOverride_DepthOfFieldFstop"] = bool(override)
            props["PostProcessSettings.DepthOfFieldFstop"] = float(fstop)
        if not props:
            return {"success": False, "message": "Pasa focal_distance o fstop"}
        return _set_props(ctx, camera_name, props)

    # ------------------------------------------------------ calidad de render
    def set_screen_percentage(ctx: Context, percentage: int) -> Dict[str, Any]:
        """Resolucion de render (r.ScreenPercentage). 100 = nativo, 50 = mas rapido."""
        return _send(ctx, "set_cvar", {"cvar_name": "r.ScreenPercentage", "value": str(int(percentage))})

    def set_post_process_quality(ctx: Context, level: int) -> Dict[str, Any]:
        """Calidad de post-proceso (sg.PostProcessQuality). 0=baja, 3=epica."""
        return _send(ctx, "set_cvar", {"cvar_name": "sg.PostProcessQuality", "value": str(int(level))})

    def set_effects_quality(ctx: Context, level: int) -> Dict[str, Any]:
        """Calidad de efectos/particulas (sg.EffectsQuality). 0=baja, 3=alta."""
        return _send(ctx, "set_cvar", {"cvar_name": "sg.EffectsQuality", "value": str(int(level))})

    def set_shadow_quality(ctx: Context, level: int) -> Dict[str, Any]:
        """Calidad de sombras (sg.ShadowQuality). 0=baja, 3=alta."""
        return _send(ctx, "set_cvar", {"cvar_name": "sg.ShadowQuality", "value": str(int(level))})

    ACTIONS = {
        'create_point_light': create_point_light,
        'create_spot_light': create_spot_light,
        'set_light_intensity': set_light_intensity,
        'set_light_color': set_light_color,
        'set_light_attenuation': set_light_attenuation,
        'set_light_temperature': set_light_temperature,
        'set_light_visibility': set_light_visibility,
        'set_light_property': set_light_property,
        'create_post_process_volume': create_post_process_volume,
        'set_post_process_unbound': set_post_process_unbound,
        'set_bloom': set_bloom,
        'set_exposure': set_exposure,
        'set_motion_blur': set_motion_blur,
        'set_depth_of_field': set_depth_of_field,
        'set_vignette': set_vignette,
        'set_film_grain': set_film_grain,
        'set_chromatic_aberration': set_chromatic_aberration,
        'set_lens_flare': set_lens_flare,
        'set_post_process_property': set_post_process_property,
        'set_camera_fov': set_camera_fov,
        'set_camera_depth_of_field': set_camera_depth_of_field,
        'set_screen_percentage': set_screen_percentage,
        'set_post_process_quality': set_post_process_quality,
        'set_effects_quality': set_effects_quality,
        'set_shadow_quality': set_shadow_quality,
    }

    actions_doc = "\n      - ".join([""] + [f"{name}(...)" for name in ACTIONS])

    @mcp.tool(
        name="unreal_vfx",
        description=(
            "Router de efectos especiales (VFX) en Unreal Engine.\n"
            "    \n"
            "    Parametros:\n"
            "      action: nombre de la operacion (ver lista abajo)\n"
            "      params: dict con los argumentos de esa operacion\n"
            "    \n"
            "    Operaciones disponibles:" + actions_doc + "\n    "
        ),
    )
    def unreal_vfx(ctx: Context, action: str,
                   params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        params = params or {}
        if action not in ACTIONS:
            return {"success": False,
                    "message": f"Unknown unreal_vfx action: '{action}'. Known: {sorted(ACTIONS)}"}
        try:
            return ACTIONS[action](ctx, **params)
        except Exception as exc:
            logger.error(f"unreal_vfx '{action}' error: {exc}")
            return {"success": False, "message": f"Error ejecutando {action}: {exc}"}

    logger.info("VFX tools (router) registered successfully")
