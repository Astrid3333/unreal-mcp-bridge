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

## Type Reference

### expression_type válidos (28)

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
— pero armable hoy con los 5 primitivos existentes, sin tooling nuevo.

## Error Handling

Todas las respuestas incluyen `success`. Los errores de `connect_material_expressions` devuelven la
clase real del nodo y el nombre de pin pedido para facilitar el debug:

```json
{
  "success": false,
  "message": "El nodo 'lerp_final' (clase MaterialExpressionLinearInterpolate) no tiene un pin de entrada llamado 'X' -- revisa el nombre o agrega el caso en SetExpressionInputPin()"
}
```
