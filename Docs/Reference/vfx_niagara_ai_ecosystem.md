# Ecosistema IA + VFX + Niagara + Unreal MCP

Referencia sobre integracion de IA con efectos visuales (VFX) en Unreal Engine via Niagara, para uso futuro en unreal-mcp-bridge.

## Que es VFX en Unreal Engine

En Unreal, el VFX moderno se construye casi exclusivamente con Niagara, el sistema de particulas y efectos visuales que reemplazo al antiguo Cascade. Permite desde simples chispas hasta simulaciones complejas de fluidos, fuego, humo, explosiones, magia y sistemas que reaccionan al entorno en tiempo real.

## Ecosistema IA + VFX + MCP (proyectos de referencia)

1. **unreal-ai-connection** (151 herramientas) - incluye `spawn_niagara_at_location` para spawnear efectos Niagara en la escena, ~50ms de latencia.
2. **monolith** (1400+ acciones) - acceso completo a Niagara: lectura/escritura de sistemas, emisores, modulos y parametros. 25+ namespaces nativos en C++ sin dependencias de Python.
3. **ue-mcp** (736+ acciones en 24 categorias) - categoria especifica de VFX con herramientas para sistemas Niagara, emisores y parametros. Requiere plugin de Niagara habilitado.
4. **unreal-niagara-mcp** (70 herramientas) - servidor MCP especializado solo en Niagara: inspeccionar emisores, editar stacks de modulos, crear sistemas desde plantillas, generar modulos HLSL, distribuciones procedurales de particulas, analisis de rendimiento.
5. **NeuralScape** - spawn de assets incluyendo Niagara VFX con patrones de colocacion precisos.

## Como integrarlo en unreal-mcp-bridge

### Fase 1: Herramientas basicas de Niagara

```python
# Python/tools/niagara_tools.py

@mcp.tool()
def spawn_niagara_effect(
    system_path: str,
    location: tuple[float, float, float],
    rotation: tuple[float, float, float] = (0, 0, 0),
    scale: float = 1.0
) -> str:
    """Spawnea un sistema Niagara en una ubicacion."""
    pass

@mcp.tool()
def set_niagara_parameter(
    effect_id: str,
    parameter_name: str,
    value: float | tuple[float, float, float] | str
) -> str:
    """Cambia un parametro de un efecto Niagara en tiempo real."""
    pass

@mcp.tool()
def trigger_niagara_event(
    effect_id: str,
    event_name: str
) -> str:
    """Dispara un evento dentro de un sistema Niagara."""
    pass
```

### Fase 2: Herramientas de alto nivel (intencion -> VFX)

```python
@mcp.tool()
def create_fire_effect(
    location: tuple[float, float, float],
    intensity: str = "medium",
    color: str = "orange"
) -> str:
    """Crea un efecto de fuego a partir de una descripcion en palabras."""
    pass

@mcp.tool()
def create_explosion(
    location: tuple[float, float, float],
    radius: float = 500,
    force: float = 1000
) -> str:
    """Crea una explosion con onda expansiva (VFX + fisica + opcional dano)."""
    pass
```

### Fase 3: Integracion con otros sistemas

```python
@mcp.tool()
def create_magical_effect_on_actor(
    actor_name: str,
    effect_type: str,  # "heal", "shield", "fire", "lightning"
    duration: float = 5.0
) -> str:
    """Adjunta un efecto Niagara a un actor por una duracion determinada."""
    pass
```

## Casos de uso avanzados

- Simulacion de clima dinamico: activar nieve progresivamente segun temperatura.
- Efectos reactivos al jugador: rastro de particulas que sigue y se apaga segun movimiento.
- Generacion procedural de VFX: multiples efectos distribuidos aleatoriamente con variacion de color.
- Simulacion de destruccion: nube de polvo con miles de particulas dispersadas por viento al colapsar una estructura.

## Consideraciones tecnicas

- **API de Python limitada**: la API de Python de Unreal tiene restricciones para manipular Niagara directamente: la mayoria de proyectos usan un plugin C++ como puente (equivalente a UnrealMCP).
- **Rendimiento**: sistemas Niagara con muchas particulas pueden ser costosos; conviene agregar herramientas de analisis de rendimiento de efectos.
- **Seguridad**: un agente IA mal configurado podria eliminar sistemas Niagara en bucle; implementar politicas de seguridad/limites.
- **Carga dinamica de herramientas**: algunos proyectos cargan herramientas VFX solo cuando se necesitan, para no saturar el contexto de la IA.

## Tabla de proyectos de referencia

| Proyecto | Aportacion |
|---|---|
| unreal-ai-connection | spawn_niagara_at_location - referencia para spawn basico |
| monolith | Acceso completo a Niagara desde C++ nativo |
| unreal-niagara-mcp | 70 herramientas especializadas en Niagara |
| ue-mcp | Categoria VFX con Niagara systems, emitters, parameters |
