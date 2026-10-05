"""
VFX / efectos especiales para Unreal MCP.

Compone comandos ya soportados por el plugin (spawn_actor, set_actor_property,
set_cvar) en tools de alto nivel para luces, volumenes de post-proceso,
camara y calidad de render. Todo verificado en vivo contra Unreal Editor 5.3.
"""
import logging
import os
import time
import uuid
from typing import Any, Dict, List, Optional, Tuple

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
        taken = _name_taken(ctx, name)
        if taken:
            return {"success": False,
                    "message": (f"Ya existe un actor con el nombre '{name}'. "
                                "Reusar nombres provoca un crash fatal en UE "
                                "(Cannot generate unique name); usa uno distinto.")}
        params: Dict[str, Any] = {"name": name, "type": actor_type, "location": location or [0.0, 0.0, 0.0]}
        if rotation:
            params["rotation"] = rotation
        return _send(ctx, "spawn_actor", params)

    def _name_taken(ctx: Context, name: str) -> bool:
        res = _send(ctx, "find_actors_by_name", {"pattern": name})
        actors = res.get("actors", []) if isinstance(res, dict) else []
        return any(isinstance(a, dict) and a.get("name") == name for a in actors)

    def _unique(base: str) -> str:
        return f"{base}_{os.getpid()}_{int(time.time())}{uuid.uuid4().hex[:4]}"

    def _delete(ctx: Context, name: str) -> Any:
        if _name_taken(ctx, name):
            return _send(ctx, "delete_actor", {"name": name})
        return {"success": True, "message": f"'{name}' no existe, nada que borrar"}

    def _read_prop(ctx: Context, actor_name: str, component_hint: str,
                   property_path: str) -> Tuple[bool, Any]:
        comps = _send(ctx, "list_components", {"actor_name": actor_name})
        names = [c.get("name", "") for c in comps.get("components", [])] if isinstance(comps, dict) else []
        candidate = next((n for n in names if component_hint.lower() in n.lower()), None)
        if not candidate:
            return False, f"sin componente que contenga '{component_hint}' (hay: {names})"
        res = _send(ctx, "get_component_property", {"actor_name": actor_name,
                                                    "component_name": candidate,
                                                    "property_path": property_path})
        if isinstance(res, dict) and res.get("success"):
            return True, res.get("value")
        return False, res.get("message", res) if isinstance(res, dict) else str(res)

    def _num_ok(value: Any, target: float, tolerance: float = 0.5) -> bool:
        try:
            return abs(float(value) - float(target)) <= tolerance
        except (TypeError, ValueError):
            return False

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
        """Cambia el FOV (grados) de un CameraActor. 90 = normal, 30 = teleobjetivo, 120 = gran angular.

        Usa CameraComponent.FieldOfView (UE5); en UE4 cae a FOVAngle.
        """
        applied = _set_props(ctx, camera_name, {"FieldOfView": float(fov_degrees)})
        if applied["applied"]:
            return applied
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

    # ------------------------------------------------- introspeccion / presets
    PPV_CANDIDATES = [
        "Settings.BloomIntensity", "Settings.BloomThreshold", "Settings.BloomIntensityOverride",
        "Settings.AutoExposureBias", "Settings.AutoExposureMinBrightness",
        "Settings.AutoExposureMaxBrightness", "Settings.MotionBlurAmount", "Settings.MotionBlurMax",
        "Settings.DepthOfFieldFocalDistance", "Settings.DepthOfFieldFstop",
        "Settings.DepthOfFieldDepthBlurAmount", "Settings.VignetteIntensity",
        "Settings.FilmGrainIntensity", "Settings.ChromaticAberrationStartOffset",
        "Settings.LensFlareIntensity", "Settings.AmbientCubemapIntensity",
        "Settings.AmbientCubemapTint", "Settings.ColorSaturation", "Settings.ColorContrast",
        "Settings.ColorGamma", "Settings.ColorGain", "Settings.ColorOffset",
        "Settings.ColorShadows", "Settings.ColorHighlights", "Settings.FringeIntensity",
        "Settings.SceneColorFringeIntensity", "Settings.SceneFringeWidth",
        "Settings.GlobalIlluminationBlend", "Settings.ReflectionsBlend",
        "Settings.AOIntensity", "Settings.AORadiusMax", "Settings.AOType",
        "Settings.AmbientOcclusionIntensity", "Settings.BloomMethod", "Settings.AutoExposureMethod",
        "Settings.AntiAliasingMethod", "Settings.DepthOfFieldMethod",
        "Settings.AmbientOcclusionType", "Settings.DynamicGlobalIlluminationMethod",
        "Settings.ReflectionsMethod", "Settings.TranslucencyType", "Settings.ConvolutionBloomSize",
        "Settings.MotionBlurTargetFPS", "Settings.BloomConvolutionTextureSize",
        "Settings.dof.KernelSize", "Settings.fog.Amount", "Settings.fog.Strength",
        "Settings.fog.Density", "Settings.fog.HDR", "Settings.fog.InscatteringLuminance",
        "Settings.fog.InscatteringColorTint", "Settings.fog.Falloff", "Settings.fog.SkyNetContribution",
        "Settings.fog.VolumetricFogScatteringDistribution", "Settings.fog.VolumetricFogExtinctionScale",
        "Settings.fog.VolumetricFogDistance", "Settings.fog.VolumetricFogStartDistance",
        "Settings.PathTracingMaxBounces", "Settings.PathTracingMaxSamples",
        "Settings.PathTracingSamplesPerPixel", "Settings.PathTracingMaxRoughness",
        "Settings.PathTracingLightingMode", "Settings.PathTracingRadianceCache",
        "Settings.bOverride_OverrideFlatToneMapping",
    ]
    LIGHT_CANDIDATES = [
        "Intensity", "LightColor", "AttenuationRadius", "bUseTemperature", "Temperature",
        "bUseInverseSquaredFalloff", "FalloffExponent", "ShadowBias", "ShadowSlopeBias",
        "ShadowResolutionScale", "ShadowBlurRadius", "bCastShadows", "bCastVolumetricShadow",
        "bCastStaticShadows", "bCastDynamicShadows", "bAffectDynamicIndirectLighting",
        "bAffectDistanceFieldLighting", "bUseRayTracedShadows", "bCastVolumetricShadow",
        "SourceRadius", "SourceLength", "SoftSourceRadius", "bUseIESBrightness",
        "IESBrightnessScale", "LightmassSettings.Intensity", "VolumetricScatteringIntensity",
        "bEnabled", "bVisible", "Mobility", "IntensityScale", "BarnDoorAngle", "BarnDoorLength",
        "bUseMenuShadowCasting", "bCastContactShadows", "ContactShadowLength",
        "ContactShadowLengthInWorldSpace", "Transmission", "bTransmission", "bCastShadowsFromMovingBodies",
    ]
    CAMERA_CANDIDATES = [
        "FieldOfView", "FOVAngle", "PostProcessSettings.BloomIntensity", "PostProcessSettings.BloomThreshold",
        "PostProcessSettings.AutoExposureBias", "PostProcessSettings.VignetteIntensity",
        "PostProcessSettings.FilmGrainIntensity", "PostProcessSettings.DepthOfFieldFocalDistance",
        "PostProcessSettings.DepthOfFieldFstop", "PostProcessSettings.MotionBlurAmount",
        "PostProcessSettings.ChromaticAberrationStartOffset", "PostProcessSettings.LensFlareIntensity",
        "PostProcessSettings.bOverride_BloomIntensity", "PostProcessSettings.bOverride_AutoExposureBias",
        "bConstrainAspectRatio", "AspectRatio", "bLockToHmd", "bUsePawnControlRotation",
        "ProjectionMode", "OrthoWidth", "OrthoNearClipPlane", "OrthoFarClipPlane",
        "bUseCustomNearPlane", "CustomNearPlaneZ",
    ]
    _MISSING_MARKERS = ("Property not found", "Struct field not found",
                        "Component not found", "Invalid object")
    _NO_SETTABLE_MARKERS = ("Unsupported struct type", "Unsupported nested struct type",
                            "Unsupported property type")

    def _probe_result(prop: str, applied: Dict[str, Any]) -> Tuple[str, str]:
        """Clasifica un set de prueba: "settable", "not_settable" o "missing"."""
        if applied.get("success"):
            return "settable", "ok"
        msg = str(applied.get("errors", {}).get(prop, applied.get("message", "")))
        if any(m in msg for m in _MISSING_MARKERS):
            return "missing", msg
        if any(m in msg for m in _NO_SETTABLE_MARKERS):
            return "not_settable", msg
        return "settable", msg

    def list_supported_settings(ctx: Context, target: str = "post_process") -> Dict[str, Any]:
        """Descubre que propiedades de post-proceso/luces camara soporta esta version de UE.

        Spawnea un actor desechable, intenta setear cada candidato con 0.0 y clasifica:
          - settable: la propiedad existe y el setter la escribe (o pide otro formato,
            ej. un vector → existe y es escribible con el formato correcto);
          - not_settable: existe pero el setter del plugin no la soporta (Vector4, etc.);
          - missing: la propiedad no existe en esta version de UE.
        El actor de prueba se borra siempre.

        Args:
            target: "post_process" (PostProcessVolume), "light" (PointLight) o "camera" (CameraActor).
        """
        spec: Dict[str, Tuple[str, List[str]]] = {
            "post_process": ("PostProcessVolume", PPV_CANDIDATES),
            "light": ("PointLight", LIGHT_CANDIDATES),
            "camera": ("CameraActor", CAMERA_CANDIDATES),
        }
        if target not in spec:
            return {"success": False, "message": f"target desconocido: '{target}'. Usa {sorted(spec)}"}
        actor_type, candidates = spec[target]
        name = _unique(f"VFXPROBE_{target}")
        spawn = _spawn(ctx, name, actor_type, [50000.0, 50000.0, 50000.0])
        if isinstance(spawn, dict) and spawn.get("success") is False:
            return {"success": False, "message": f"No pude crear el actor de prueba: {spawn.get('message')}"}
        buckets: Dict[str, List[str]] = {"settable": [], "not_settable": [], "missing": []}
        messages: Dict[str, str] = {}
        try:
            for prop in candidates:
                state, msg = _probe_result(prop, _set_props(ctx, name, {prop: 0.0}))
                buckets[state].append(prop)
                if msg != "ok":
                    messages[prop] = msg
        finally:
            cleanup = _delete(ctx, name)
        return {
            "success": True,
            "target": target,
            "actor_type": actor_type,
            "settable": sorted(buckets["settable"]),
            "supported": sorted(buckets["settable"]),
            "exists_not_settable": sorted(buckets["not_settable"]),
            "missing": sorted(buckets["missing"]),
            "messages": messages,
            "cleanup": cleanup,
        }

    PRESET_STEPS: Dict[str, List[Tuple[str, Dict[str, Any]]]] = {
        "night": [
            ("set_bloom", {"intensity": 0.8, "threshold": 1.2}),
            ("set_exposure", {"bias": -0.9}),
            ("set_vignette", {"intensity": 0.6}),
            ("set_film_grain", {"intensity": 0.18}),
            ("set_chromatic_aberration", {"start_offset": 0.1}),
        ],
        "horror": [
            ("set_bloom", {"intensity": 0.5, "threshold": 1.6}),
            ("set_exposure", {"bias": -1.4}),
            ("set_vignette", {"intensity": 1.0}),
            ("set_film_grain", {"intensity": 0.35}),
            ("set_motion_blur", {"amount": 0.2}),
        ],
        "sunset": [
            ("set_bloom", {"intensity": 2.4, "threshold": 0.9}),
            ("set_exposure", {"bias": 0.7}),
            ("set_vignette", {"intensity": 0.35}),
            ("set_chromatic_aberration", {"start_offset": 0.15}),
            ("set_film_grain", {"intensity": 0.1}),
        ],
        "dream": [
            ("set_bloom", {"intensity": 4.5, "threshold": 0.6}),
            ("set_depth_of_field", {"focal_distance": 1800.0, "fstop": 1.8}),
            ("set_motion_blur", {"amount": 0.8}),
            ("set_vignette", {"intensity": 0.3}),
            ("set_film_grain", {"intensity": 0.08}),
        ],
        "cinematic": [
            ("set_bloom", {"intensity": 2.0, "threshold": 1.0}),
            ("set_exposure", {"bias": 0.15}),
            ("set_depth_of_field", {"focal_distance": 3000.0, "fstop": 4.0}),
            ("set_motion_blur", {"amount": 0.4}),
            ("set_vignette", {"intensity": 0.5}),
            ("set_film_grain", {"intensity": 0.2}),
        ],
    }

    def list_presets(ctx: Context) -> Dict[str, Any]:
        """Lista los presets de look disponibles y sus pasos."""
        return {"success": True, "presets": sorted(PRESET_STEPS),
                "steps": {k: [a for a, _ in v] for k, v in PRESET_STEPS.items()}}

    def apply_preset(ctx: Context, preset: str, volume_name: Optional[str] = None) -> Dict[str, Any]:
        """Aplica un preset de look (bloom/exposicion/DOF/vignette/grano) a un PostProcessVolume.

        Args:
            preset: uno de los de list_presets ("night", "horror", "sunset", "dream", "cinematic").
            volume_name: volumen destino. Si se omite usa (o crea) "VFX_PresetVolume" unbound.
        """
        if preset not in PRESET_STEPS:
            return {"success": False, "message": f"Preset desconocido: '{preset}'. Usa {sorted(PRESET_STEPS)}"}
        created = False
        if not volume_name:
            volume_name = "VFX_PresetVolume"
        if not _name_taken(ctx, volume_name):
            spawn = _spawn(ctx, volume_name, "PostProcessVolume", [0.0, 0.0, 0.0])
            if isinstance(spawn, dict) and spawn.get("success") is False:
                return {"success": False, "message": f"No pude crear '{volume_name}': {spawn.get('message')}"}
            _set_props(ctx, volume_name, {"bEnabled": True, "bUnbound": True})
            created = True
        steps = []
        all_ok = True
        for action, kwargs in PRESET_STEPS[preset]:
            try:
                result = ACTIONS[action](ctx, volume_name=volume_name, **kwargs)
            except Exception as exc:
                result = {"success": False, "message": str(exc)}
            ok = isinstance(result, dict) and bool(result.get("success"))
            all_ok = all_ok and ok
            steps.append({"action": action, "ok": ok,
                          "detail": None if ok else (result.get("message") or result.get("errors"))})
        return {"success": all_ok, "preset": preset, "volume": volume_name,
                "created_volume": created, "steps": steps}

    def selftest(ctx: Context) -> Dict[str, Any]:
        """Autochequeo del puente VFX: conexion, luces, post-proceso, camara y cvars.

        Crea actores con nombres unicos, verifica lectura de vuelta y limpia todo
        al final (tambien si algo falla). Sirve para validar tras reiniciar UE.
        """
        checks: List[Dict[str, Any]] = []

        def check(name: str, ok: Any, detail: Any = "") -> bool:
            good = bool(ok)
            stored = detail if (not good or isinstance(detail, str)) else ""
            checks.append({"check": name, "ok": good, "detail": stored})
            return good

        ping = _send(ctx, "ping", {})
        if not check("conexion", isinstance(ping, dict) and ping.get("message") == "pong", ping):
            return {"success": False, "passed": 0, "failed": 1, "checks": checks,
                    "message": "Sin conexion con Unreal Editor"}
        cvar_before = _send(ctx, "get_cvar", {"cvar_name": "r.ScreenPercentage"})
        prev_screen = cvar_before.get("value") if isinstance(cvar_before, dict) else None

        light_name = _unique("VFXSELF_Light")
        ppv_name = _unique("VFXSELF_PPV")
        cam_name = _unique("VFXSELF_Cam")
        try:
            spawn = _spawn(ctx, light_name, "PointLight", [200.0, -400.0, 300.0])
            if check("spawn_point_light", isinstance(spawn, dict) and spawn.get("success") is not False, spawn):
                setr = _set_props(ctx, light_name, {"Intensity": 4321.0, "LightColor": [10, 200, 90, 255]})
                check("set_light_props", setr["success"], setr["errors"] or setr["applied"])
                ok, val = _read_prop(ctx, light_name, "LightComponent", "Intensity")
                check("read_back_light_intensity", ok and _num_ok(val, 4321.0, 1.0), val)
                ok, val = _read_prop(ctx, light_name, "LightComponent", "LightColor")
                check("read_back_light_color", ok, val)

            spawn = _spawn(ctx, ppv_name, "PostProcessVolume", [0.0, 0.0, 0.0])
            if check("spawn_post_process_volume", isinstance(spawn, dict) and spawn.get("success") is not False, spawn):
                _set_props(ctx, ppv_name, {"bEnabled": True, "bUnbound": True})
                for action, kwargs in [("set_bloom", {"intensity": 2.0, "threshold": 1.0}),
                                       ("set_exposure", {"bias": 0.2}),
                                       ("set_vignette", {"intensity": 0.5}),
                                       ("set_film_grain", {"intensity": 0.2}),
                                       ("set_chromatic_aberration", {"start_offset": 0.1}),
                                       ("set_lens_flare", {"intensity": 1.0}),
                                       ("set_motion_blur", {"amount": 0.5}),
                                       ("set_depth_of_field", {"focal_distance": 2000.0, "fstop": 2.8})]:
                    r = ACTIONS[action](ctx, volume_name=ppv_name, **kwargs)
                    check(f"ppv_{action}", isinstance(r, dict) and r.get("success"),
                          r.get("errors") if isinstance(r, dict) else r)

            spawn = _spawn(ctx, cam_name, "CameraActor", [0.0, 500.0, 200.0])
            if check("spawn_camera", isinstance(spawn, dict) and spawn.get("success") is not False, spawn):
                _set_props(ctx, cam_name, {"FieldOfView": 70.0})
                ok, val = _read_prop(ctx, cam_name, "CameraComponent", "FieldOfView")
                check("read_back_camera_fov", ok and _num_ok(val, 70.0, 0.5), val)

            _send(ctx, "set_cvar", {"cvar_name": "r.ScreenPercentage", "value": "85"})
            after = _send(ctx, "get_cvar", {"cvar_name": "r.ScreenPercentage"})
            check("cvar_set_get", isinstance(after, dict) and str(after.get("value")) == "85",
                  after.get("value") if isinstance(after, dict) else after)
        finally:
            for n in (light_name, ppv_name, cam_name):
                _delete(ctx, n)
            if prev_screen is not None:
                _send(ctx, "set_cvar", {"cvar_name": "r.ScreenPercentage", "value": str(prev_screen)})

        passed = sum(1 for c in checks if c["ok"])
        failed = len(checks) - passed
        return {"success": failed == 0, "passed": passed, "failed": failed,
                "checks": checks, "restored_screen_percentage": prev_screen}

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
        'list_supported_settings': list_supported_settings,
        'list_presets': list_presets,
        'apply_preset': apply_preset,
        'selftest': selftest,
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
