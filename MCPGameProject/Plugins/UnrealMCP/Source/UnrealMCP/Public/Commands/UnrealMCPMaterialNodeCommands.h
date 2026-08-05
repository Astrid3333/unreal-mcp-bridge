#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UMaterial;
class UMaterialExpression;

// =====================================================================
// FUnrealMCPMaterialNodeCommands
//
// Grafo de Material generico: agregar cualquier UMaterialExpression por
// nombre de clase, conectar pines entre nodos, y setear los outputs
// finales del material (BaseColor, Roughness, etc). Complementa a los
// comandos "receta fija" que ya existen en UnrealMCPEditorCommands
// (create_material, create_moss_stone_material, create_pbr_material).
//
// IDENTIDAD DE NODOS: cada expression creada recibe un nombre unico de
// UObject (via NewObject con FName explicito, ej "Multiply_0",
// "Multiply_1"...) y ese nombre es el "node_id" que se devuelve al
// crearlo y el que hay que pasar despues a connect_material_expressions.
// No usamos el indice en el array de expressions porque ese indice no
// es estable si en el futuro se borran nodos.
//
// PINES: la conexion de pines NO es generica via reflection -- FExpressionInput
// no es un FProperty reflejado de forma uniforme entre todas las clases de
// expression, asi que SetExpressionInputPin() resuelve el pin a mano por
// clase (mismo enfoque que ya usa HandleCreateMossStoneMaterial). Si agregas
// un tipo de expression nuevo al mapa de EXPRESSION_TYPES, tenes que agregar
// su caso correspondiente en SetExpressionInputPin() para que sus pines de
// entrada sean conectables. Los tipos ya cubiertos alcanzan para la mayoria
// de grafos proceduales (aritmetica, lerp, clamp, noise, texturas, coords).
//
// INTROSPECCION: list_available_expression_types expone en vivo las keys
// del mapa EXPRESSION_TYPES (ver GetExpressionTypeMap() en el .cpp) para
// poder verificar el vocabulario documentado en material_node_tools.md
// contra el binario compilado real, sin mantener una segunda lista a mano.
// =====================================================================
class UNREALMCP_API FUnrealMCPMaterialNodeCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialOutput(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListMaterialExpressions(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialExpressionConstant(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListAvailableExpressionTypes(const TSharedPtr<FJsonObject>& Params);

    // Crea la UMaterialExpression correspondiente a expression_type (ver
    // EXPRESSION_TYPES en el .cpp para la lista de nombres soportados).
    UMaterialExpression* CreateExpressionByType(UMaterial* Material, const FString& ExpressionType, const FString& NodeId, FString& OutError);

    UMaterialExpression* FindExpressionByNodeId(UMaterial* Material, const FString& NodeId);

    // Conecta SourceNode (con su OutputIndex, 0 = output default) al pin
    // de entrada PinName de TargetNode. Devuelve false + OutError si
    // TargetNode no tiene ese pin o el tipo de TargetNode no esta cubierto.
    bool SetExpressionInputPin(UMaterialExpression* TargetNode, const FString& PinName, UMaterialExpression* SourceNode, int32 OutputIndex, FString& OutError);

    // Guarda el paquete del material a disco (mismo patron que el resto
    // del modulo de materiales).
    bool SaveMaterialPackage(UMaterial* Material);
};
