# Unreal MCP Material Node Tools

Este documento describe el dominio genérico `unreal_material_node`: construcción de grafos
de material (`UMaterial`) nodo por nodo, vía reflection directa sobre `FMaterialExpressionCollection`
— sin abrir el Material Editor. Complementa a los tools de material de alto nivel en `unreal_actor`
(`create_material`, `create_moss_stone_material`, `set_material_scalar_parameter`, etc.), que cubren
casos comunes con una sola llamada; este dominio es para armar grafos custom.

## Material Node Tools

### add_material_expression

Crea un nodo nuevo en el grafo de un Material.

**Parámetros:**
- `material_path` (string)
- `expression_type` (string) — ver Type Reference abajo
- `node_id` (string) — nombre único que vos elegís para referenciar este nodo en llamadas siguientes
- `position_x` / `position_y` (number, opcional) — posición en el grafo del editor

**Returns:** `node_id`, `expression_type`, `success`

**Ejemplo:**
```json
{
  "action": "add_material_expression",
  "params": {
    "material_path": "/Game/Materials/M_Ejemplo",
    "expression_type": "LinearInterpolate",
    "node_id": "lerp_final"
  }
}
```

### connect_material_expressions

Conecta la salida de un nodo al pin de entrada de otro.

**Parámetros:**
- `material_path` (string)
- `source_node_id` (string)
- `target_node_id` (string)
- `target_pin` (string) — ver tabla de pines válidos por tipo abajo
- `source_output_index` (number, opcional, default 0)

Si `target_pin` no existe en ese tipo de nodo, el error devuelve la clase real del nodo y el pin pedido.

### set_material_output

Conecta un nodo a una de las entradas del material (BaseColor, Roughness, etc.) y **guarda el material a disco**.

**Parámetros:**
- `material_path` (string)
- `output_pin` (string) — ver valores válidos abajo
- `node_id` (string)
- `output_index` (number, opcional)

**Returns:** `saved_to_disk`, `success`

### list_material_expressions

Lista todos los nodos existentes en el grafo de un material.

**Parámetros:** `material_path`

**Returns:** `nodes`: array de `{node_id, class}`

### set_material_expression_constant

Setea el valor "default" de un nodo constante/parámetro sin conectarle nada — para tunear el grafo
después de armado.

**Parámetros:**
- `material_path` (string)
- `node_id` (string)
- `value` (array) — `[x]` para escalar, `[r,g,b]` o `[r,g,b,a]` para vector

Solo funciona sobre: `Constant`, `ScalarParameter`, `Constant2Vector`, `Constant3Vector`, `Constant4Vector`, `VectorParameter`.

## Comandos de ciclo de vida (nuevos)

Seis comandos más para armar/editar/guardar el grafo completo sin salir de este dominio.
Todos trabajan sobre `material_path` y guardan el `.uasset` a disco (solo `/Game/`).

### create_empty_material

Crea un material vacío (sin nodos) y lo guarda a disco. Es el material base para
`add_material_expression`. Si el material ya existe lo vacía en sitio (conserva las
referencias de actores que ya lo tengan asignado).

**Parámetros:** `name` (string), `path` (string, opcional, default `/Game/Materials`)

**Returns:** `material_path` — lo que se pasa al resto de los comandos.

Nota: el wrapper de alto nivel `unreal_actor.create_material` sigue ruteando al handler
antiguo (`create_material` de EditorCommands, que crea un material con color/roughness
fijos); para un grafo nodo por nodo usar este `create_empty_material`.

### get_material_expression

Lee un nodo: clase, `expression_type`, posición, pines de entrada (con su conexión),
outputs disponibles y dump de propiedades escalares. Es la forma de descubrir qué
propiedades acepta `set_material_expression_property`.

**Parámetros:** `material_path`, `node_id`

**Returns:** `class`, `expression_type`, `position`, `inputs` (pin → fuente conectada),
`outputs`, `properties`.

### set_material_expression_property

Setea cualquier propiedad escalar/reflejada de un nodo y guarda a disco.
A diferencia de `set_material_expression_constant` (que solo toca el valor default
de constantes/parámetros), éste sirve para propiedades arbitrarias del nodo.

**Parámetros:**
- `material_path` (string)
- `node_id` (string)
- `property` (string) — nombre real de la UPROPERTY en C++ (ej. `R`, `G`, `B`,
  `ConstA`, `Texture`, `SamplerType`, `bClamp`)
- `value` (number | string | bool | `[r,g,b,a]`) — según el tipo de la propiedad

### delete_material_expression

Borra un nodo del grafo, anula todas sus referencias (nada queda apuntando a él) y
guarda a disco. El `node_id` queda libre para reusarse.

**Parámetros:** `material_path`, `node_id`

### disconnect_material_input

Desconecta el pin de entrada `target_pin` de un nodo (lo deja sin fuente) y guarda a
disco. Si el pin no existe, el error lista los pines disponibles del nodo.

**Parámetros:** `material_path`, `node_id`, `target_pin`

### save_material

Guarda el `.uasset` del material a disco. Los demás comandos ya guardan solos; éste
existe para forzar el guardado tras una tanda de ediciones externas.

**Parámetros:** `material_path`

**Returns:** `saved_to_disk`, `success`

## Type Reference

### expression_type válidos (29)

`Add`, `Subtract`, `Multiply`, `Divide`, `LinearInterpolate`, `Clamp`, `Power`, `OneMinus`,
`Desaturation`, `DotProduct`, `Normalize`, `ComponentMask`, `AppendVector`, `Distance`, `Fresnel`,
`Panner`, `Time`, `TextureCoordinate`, `TextureSample`, `TransformPosition`, `WorldPosition`,
`VertexNormalWS`, `Noise`, `Constant`, `Constant2Vector`, `Constant3Vector`, `Constant4Vector`,
`ScalarParameter`, `VectorParameter`

⚠️ El nodo "Lerp" de Unreal se pide como **`LinearInterpolate`**, no `"Lerp"` — es un error común al
traducir descripciones en lenguaje natural.

Agregar un tipo nuevo: sumar el `#include` correspondiente y una entrada al mapa `EXPRESSION_TYPES`
en `UnrealMCPMaterialNodeCommands.cpp`, más el caso en `SetExpressionInputPin()` si el nodo tiene
pines de entrada conectables.

### target_pin válidos por tipo de nodo (para connect_material_expressions)

| Tipo de nodo | Pines de entrada válidos |
|---|---|
| Add, Subtract, Multiply, Divide, DotProduct, Distance, AppendVector | `A`, `B` |
| Power | `Base`, `Exponent` |
| LinearInterpolate | `A`, `B`, `Alpha` |
| Clamp, OneMinus, Desaturation, ComponentMask, TransformPosition | `Input` |
| Normalize | `VectorInput` |
| Noise | `Position`, `FilterWidth` |
| Panner | `Coordinate`, `Time` |
| TextureSample | `Coordinates` |
| Fresnel | `Normal`, `ExponentIn`, `BaseReflectFractionIn` |

Sin pines de entrada conectables (nodos fuente, o se configuran con `set_material_expression_constant`):
`TextureCoordinate`, `Time`, `WorldPosition`, `VertexNormalWS`, `Constant`, `Constant2Vector`,
`Constant3Vector`, `Constant4Vector`, `ScalarParameter`, `VectorParameter`.

### output_pin válidos (para set_material_output)

`BaseColor`, `Metallic`, `Specular`, `Roughness`, `EmissiveColor`, `Opacity`, `OpacityMask`,
`Normal`, `WorldPositionOffset`, `AmbientOcclusion`

## Ejemplo completo: Lerp madera/musgo por parámetro escalar

Grafo tipo: dos texturas mezcladas con un `LinearInterpolate` controlado por un `ScalarParameter`.

```json
[
  {"action": "add_material_expression", "params": {"material_path": "/Game/Materials/M_Ejemplo", "expression_type": "TextureSample", "node_id": "tex_madera"}},
  {"action": "add_material_expression", "params": {"material_path": "/Game/Materials/M_Ejemplo", "expression_type": "TextureSample", "node_id": "tex_musgo"}},
  {"action": "add_material_expression", "params": {"material_path": "/Game/Materials/M_Ejemplo", "expression_type": "ScalarParameter", "node_id": "mezcla"}},
  {"action": "add_material_expression", "params": {"material_path": "/Game/Materials/M_Ejemplo", "expression_type": "LinearInterpolate", "node_id": "lerp_final"}},
  {"action": "set_material_expression_constant", "params": {"material_path": "/Game/Materials/M_Ejemplo", "node_id": "mezcla", "value": [0.5]}},
  {"action": "connect_material_expressions", "params": {"material_path": "/Game/Materials/M_Ejemplo", "source_node_id": "tex_madera", "target_node_id": "lerp_final", "target_pin": "A"}},
  {"action": "connect_material_expressions", "params": {"material_path": "/Game/Materials/M_Ejemplo", "source_node_id": "tex_musgo", "target_node_id": "lerp_final", "target_pin": "B"}},
  {"action": "connect_material_expressions", "params": {"material_path": "/Game/Materials/M_Ejemplo", "source_node_id": "mezcla", "target_node_id": "lerp_final", "target_pin": "Alpha"}},
  {"action": "set_material_output", "params": {"material_path": "/Game/Materials/M_Ejemplo", "output_pin": "BaseColor", "node_id": "lerp_final"}}
]
```

Este es exactamente el ejemplo que propone el documento de "Fase 3" (create_material_graph_from_steps)
— armarlo hoy requiere las 12 acciones del dominio: empezar con `create_empty_material`
(si el material no existe aún), las 5 primitivos de grafo de arriba, y `save_material`
solo si querés forzar el guardado (`set_material_output` ya guarda solo).

## Error Handling

Todas las respuestas incluyen `success`. Los errores de `connect_material_expressions` devuelven la
clase real del nodo y el nombre de pin pedido para facilitar el debug:

```json
{
  "success": false,
  "message": "El nodo 'lerp_final' (clase MaterialExpressionLinearInterpolate) no tiene un pin de entrada llamado 'X' -- revisa el nombre o agrega el caso en SetExpressionInputPin()"
}
```
