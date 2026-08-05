# Roadmap de Gameplay para unreal-mcp-bridge (2D/3D)

Referencia sobre las areas de gameplay, logica y experiencia de juego que faltan expandir en unreal-mcp-bridge para convertirlo en una herramienta completa de desarrollo de videojuegos (mas alla de edicion de escenas).

## Areas clave para videojuegos 3D

### 1. Blueprints y logica de juego (gameplay programming)

Columna vertebral de cualquier juego.

- `create_blueprint` / `compile_blueprint`
- `add_blueprint_component` (ej. StaticMeshComponent, CharacterMovementComponent)
- `add_blueprint_variable` / `set_blueprint_property`
- `add_blueprint_function`
- `add_blueprint_event_node` / `connect_blueprint_nodes`
- `set_blueprint_parent_class` (heredar de Character, Pawn, etc.)

Ejemplo: "Crea un Blueprint de personaje llamado 'Heroe' que herede de Character. Anade un componente de movimiento y una variable de 'Vida'."

### 2. Gameplay Ability System (GAS)

Estandar de Epic para juegos complejos (habilidades, efectos, atributos).

- `create_gameplay_ability`
- `create_gameplay_effect` (dano, curacion, buffs con duracion y modificadores)
- `create_gameplay_attribute` (vida, mana, fuerza)
- `assign_ability_to_character`

Ejemplo: "Crea una habilidad de 'Lanzar Bola de Fuego' que cause 50 de dano y tenga un cooldown de 3 segundos."

### 3. UI y HUD (UMG)

- `create_widget_blueprint`
- `add_text_block_to_widget` / `add_button_to_widget`
- `bind_widget_event`
- `set_widget_variable`

Ejemplo: "Crea un HUD que muestre la vida del jugador con una barra roja en la esquina superior izquierda."

### 4. Audio

- `create_sound_cue`
- `spawn_ambient_sound`
- `play_sound_2d` / `play_sound_3d`
- `set_sound_mix`

Ejemplo: "Reproduce un sonido de explosion 3D en la posicion del actor 'Coche'."

### 5. Animacion

- `create_animation_blueprint`
- `set_animation_state` (idle, walk, run, jump)
- `play_montage`
- `set_skeleton`

Ejemplo: "Asigna la animacion de 'correr' al personaje cuando se mueve y 'saltar' cuando presiona espacio."

### 6. IA y navegacion (Behavior Trees, EQS, NavMesh)

- `create_behavior_tree`
- `create_blackboard`
- `add_bt_task` / `add_bt_decorator` / `add_bt_service`
- `create_environment_query_system` (EQS)
- `rebuild_navmesh`
- `find_path`

Ejemplo: "Crea un enemigo que patrulla entre tres puntos y persigue al jugador si lo ve."

### 7. Fisica y colisiones

- `set_physics_properties` (masa, gravedad, friccion)
- `apply_force` / `apply_impulse`
- `set_collision_profile` (BlockAll, OverlapAll, etc.)
- `create_ragdoll`
- `create_constraint` (bisagras, muelles)

Ejemplo: "Haz que esta caja pese 50 kilos y aplicale un impulso hacia adelante."

### 8. Generacion procedural (PCG) y mundo

- `create_pcg_graph`
- `add_pcg_node` (Surface Sampler, Mesh Spawner, etc.)
- `connect_pcg_nodes`
- `run_pcg_graph`
- `set_pcg_seed`

Ejemplo: "Genera un bosque procedural de 1 km2 con arboles de distintos tamanos."

### 9. Networking y multijugador

- `set_replication`
- `set_replication_condition`
- `spawn_actor_networked`

Ejemplo: "Haz que la variable 'Vida' del jugador se replique a todos los clientes."

### 10. VFX (Niagara) - ya cubierto parcialmente

- `spawn_niagara_system`
- `set_niagara_parameter`
- `trigger_niagara_event`

Ejemplo: "Crea una explosion de particulas en la posicion del enemigo cuando muera."

### 11. Secuenciador y cinematicas

- `create_level_sequence`
- `add_actor_to_sequencer`
- `add_transform_keyframe`
- `add_camera_cut`
- `play_sequence` / `pause_sequence`

Ejemplo: "Crea una cinematica de 10 segundos donde la camara orbita alrededor del personaje."

## Consideraciones especiales para videojuegos 2D

Unreal soporta 2D nativamente via Paper2D y PaperZD.

### Paper2D (sistema 2D nativo)

- `create_paper_flipbook` (animacion 2D desde sprites)
- `create_paper_sprite` (sprite desde textura)
- `set_flipbook`
- `play_flipbook`
- `set_sprite_color`

Ejemplo: "Crea un personaje 2D con un Flipbook de caminar y otro de saltar."

### Tile Maps

- `create_tile_map`
- `set_tile`
- `create_tile_set` (desde una textura)

Ejemplo: "Crea un mapa de 20x20 teselas con suelo de hierba y paredes de piedra."

### Camara 2D

- `set_camera_2d` (ortografica)
- `set_camera_bounds`
- `follow_actor_2d`

### Gestion de sprites y texturas

- `import_sprite_sheet`
- `create_flipbook_from_sheet`

## Hoja de ruta de implementacion sugerida

### Fase 1: Fundamentos de gameplay (prioridad alta)

- Blueprint Graph: `add_blueprint_event_node`, `add_blueprint_function_node`, `connect_blueprint_nodes` (parcialmente cubierto)
- Variables y propiedades: `add_blueprint_variable`, `set_blueprint_property` (parcialmente cubierto)
- UI (UMG): `create_widget_blueprint`, `add_text_block`, `add_button`, `bind_event` (cubierto)
- Audio: `create_sound_cue`, `spawn_ambient_sound` (cubierto)
- IA basica: `create_behavior_tree`, `create_blackboard` (cubierto)
- Fisica: `set_physics_properties` (cubierto)

### Fase 2: Sistemas avanzados

- GAS: `create_gameplay_ability`, `create_gameplay_effect`
- Animacion: `create_animation_blueprint`, `play_montage`
- PCG: `create_pcg_graph`, `add_pcg_node`, `run_pcg_graph`
- Networking: `set_replication`, `spawn_actor_networked`

### Fase 3: Soporte 2D

- Paper2D: `create_paper_flipbook`, `create_paper_sprite`, `set_flipbook`
- Tile Maps: `create_tile_map`, `set_tile`
- Camara 2D: `set_camera_2d`, `follow_actor_2d`

### Fase 4: Orquestacion de alto nivel

Herramientas compuestas combinando multiples sistemas:

- `create_rpg_character`: personaje con Blueprint, GAS, animaciones y UI.
- `create_platformer_level`: nivel 2D con tiles, enemigos y power-ups.
- `create_boss_ai`: jefe con Behavior Tree, GAS y VFX.

## Comparativa con proyectos existentes (referencia de escala)

| Proyecto | Herramientas | Cobertura de gameplay |
|---|---|---|
| unreal-mcp-bridge (propio) | ~47 | Basica (actors, materiales, algo de Blueprint) |
| ue-mcp | 736+ | Completa (GAS, networking, PCG, animacion, UI) |
| UnrealMCPPro | 200+ | 39 categorias |
| Ultimate Unreal MCP | 133 | 26 dominios |
| PrismMCP | 1678 | 58 sistemas del editor |

## Conclusion

Para que unreal-mcp-bridge sea una herramienta completa para videojuegos, faltan sistemas de gameplay (GAS, animacion, IA avanzada, networking), sistemas de mundo (PCG) y soporte 2D (Paper2D, Tile Maps). El salto conceptual: de comandos como "crea un cubo" a "crea un juego de plataformas 2D con un personaje que salta, enemigos que patrullan y un jefe final".
