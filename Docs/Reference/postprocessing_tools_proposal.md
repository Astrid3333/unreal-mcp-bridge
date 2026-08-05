# Postproduccion en unreal-mcp-bridge - propuesta de herramientas

Referencia sobre el modulo de postproduccion (Post Process Volumes, camara, color grading) que falta en unreal-mcp-bridge. El control del look final de la escena (color, bloom, profundidad de campo, exposicion) es relevante tanto para VFX como para simulacion medica (claridad visual) y cientifica (representacion precisa de datos).

## Que es la postproduccion en Unreal

- **Post Process Volumes (PPV)**: actores que aplican efectos a las camaras dentro de su volumen.
- **Configuraciones de camara**: las camaras pueden tener sus propias anulaciones de post-procesado.
- **Global Settings**: configuraciones aplicadas a toda la escena si no hay volumenes.

## Estado actual vs propuesto

| Dominio | Estado actual | Accion propuesta |
|---|---|---|
| Postproduccion (Volumenes) | No existe | Crear postprocessing_tools.py |
| Configuracion de camara | Parcial (solo creacion) | Anadir control de DOF, exposicion, etc. |
| LUTs y color grading | No existe | Anadir herramientas para aplicar LUTs |
| Estilos predefinidos | No existe | Anadir apply_cinematic_look |

## Herramientas propuestas (Python/tools/postprocessing_tools.py)

### 1. Gestion de Post Process Volumes

- `create_post_process_volume(location, rotation, scale, unbounded: bool)` - crea un PPV.
- `set_post_process_priority(volume_name, priority: float)` - prioridad entre volumenes solapados.
- `set_blend_weight(volume_name, weight: float)` - peso de mezcla del efecto (0.0-1.0).

### 2. Iluminacion y exposicion (clave en simulacion)

- `set_exposure(compensation_ev100, min_brightness, max_brightness, adaptation_speed)`.
- `set_bloom(intensity, threshold, size, tint_color)`.

### 3. Enfoque y nitidez (clave en simulacion medica)

- `set_depth_of_field(focal_distance, aperture_fstop, blur_amount)`.
- `set_motion_blur(amount: float)`.

### 4. Color y estilo (VFX y cine)

- `set_color_grading(saturation, contrast, gamma, color_gain_shadows, color_gain_midtones, color_gain_highlights)`.
- `apply_lut(lut_path: str, intensity: float = 1.0)` - aplica un archivo .cube.

### 5. Efectos de lente y vineta

- `set_vignette(intensity: float)`.
- `set_chromatic_aberration(intensity: float)`.

### 6. Herramienta de alto nivel (intencion -> accion)

- `apply_cinematic_look(style: str)` - estilos: "horror", "calido", "frio", "documental", "quirurgico". Configura automaticamente color, bloom, vineta y DOF segun el estilo elegido.

## Conexion con simulacion medica y cientifica

| Area | Aplicacion de postproduccion | Comando ejemplo |
|---|---|---|
| Simulacion quirurgica | Aumentar nitidez/contraste para ver tejidos con claridad; desactivar bloom para evitar reflejos molestos | "Ajusta el contraste al maximo y desactiva el bloom para la cirugia." |
| Simulacion cientifica (datos) | Color grading neutro y exposicion fija para que colores de datos (ej. mapas de calor) sean precisos | "Fija la exposicion en 0 EV y usa un color grading lineal." |
| VFX y cine | LUTs, bloom dramatico y profundidad de campo para realismo cinematografico | "Aplica un LUT de 'Blockbuster' y enfoca al protagonista." |

## Estructura de codigo sugerida (Python)

```python
# Python/tools/postprocessing_tools.py

@mcp.tool()
def set_bloom(intensity: float = 0.5, threshold: float = 1.0) -> str:
    """Controla el efecto Bloom de la escena."""
    pass

@mcp.tool()
def set_depth_of_field(focal_distance: float, aperture: float) -> str:
    """Configura la profundidad de campo."""
    pass

@mcp.tool()
def apply_lut(lut_path: str, intensity: float = 1.0) -> str:
    """Aplica un archivo LUT (.cube) a la escena."""
    pass
```

## Implementacion tecnica en el plugin C++

En UnrealMCP, se necesitan funciones que interactuen con `FPostProcessSettings`. Ejemplo de obtener y modificar el volumen activo:

```cpp
// En el plugin C++
void UUnrealMCPBridge::SetBloom(float Intensity, float Threshold)
{
    if (APostProcessVolume* Volume = GetActivePostProcessVolume())
    {
        FPostProcessSettings& Settings = Volume->Settings;
        Settings.bOverride_BloomIntensity = true;
        Settings.BloomIntensity = Intensity;
        Settings.bOverride_BloomThreshold = true;
        Settings.BloomThreshold = Threshold;
    }
}
```

## Conclusion

Con estas herramientas, el bridge pasaria de controlar que hay en la escena a controlar como se ve la escena: el salto necesario para dirigir el look de una simulacion (VFX, quirofano virtual, o visualizacion de datos cientificos) solo con lenguaje natural.
