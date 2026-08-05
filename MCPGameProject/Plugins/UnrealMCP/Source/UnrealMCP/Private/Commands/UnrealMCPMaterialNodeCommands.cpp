#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "Commands/UnrealMCPMaterialNodeCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Engine/Texture2D.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"

// =====================================================================
// Mapa de nombres de expression_type -> UClass. Agregar aca cuando se
// sume un tipo nuevo, y agregar el caso correspondiente en
// SetExpressionInputPin() si el tipo tiene pines de entrada conectables.
//
// Vive a nivel de archivo (no local a ResolveExpressionClass) para que
// list_available_expression_types pueda leerlo tambien via
// GetExpressionTypeMap() -- introspeccion en vivo, una sola fuente de
// verdad para "que tipos existen" en vez de mantener dos listas.
// =====================================================================
static const TMap<FString, UClass*>& GetExpressionTypeMap()
{
    static const TMap<FString, UClass*> EXPRESSION_TYPES = {
        { TEXT("Add"),                 UMaterialExpressionAdd::StaticClass() },
        { TEXT("Subtract"),            UMaterialExpressionSubtract::StaticClass() },
        { TEXT("Multiply"),            UMaterialExpressionMultiply::StaticClass() },
        { TEXT("Divide"),              UMaterialExpressionDivide::StaticClass() },
        { TEXT("LinearInterpolate"),   UMaterialExpressionLinearInterpolate::StaticClass() },
        { TEXT("Clamp"),               UMaterialExpressionClamp::StaticClass() },
        { TEXT("Power"),                UMaterialExpressionPower::StaticClass() },
        { TEXT("OneMinus"),            UMaterialExpressionOneMinus::StaticClass() },
        { TEXT("Desaturation"),        UMaterialExpressionDesaturation::StaticClass() },
        { TEXT("DotProduct"),          UMaterialExpressionDotProduct::StaticClass() },
        { TEXT("Normalize"),           UMaterialExpressionNormalize::StaticClass() },
        { TEXT("ComponentMask"),       UMaterialExpressionComponentMask::StaticClass() },
        { TEXT("AppendVector"),        UMaterialExpressionAppendVector::StaticClass() },
        { TEXT("Distance"),            UMaterialExpressionDistance::StaticClass() },
        { TEXT("Fresnel"),             UMaterialExpressionFresnel::StaticClass() },
        { TEXT("Panner"),              UMaterialExpressionPanner::StaticClass() },
        { TEXT("Time"),                UMaterialExpressionTime::StaticClass() },
        { TEXT("TextureCoordinate"),   UMaterialExpressionTextureCoordinate::StaticClass() },
        { TEXT("TextureSample"),       UMaterialExpressionTextureSample::StaticClass() },
        { TEXT("TransformPosition"),   UMaterialExpressionTransformPosition::StaticClass() },
        { TEXT("WorldPosition"),       UMaterialExpressionWorldPosition::StaticClass() },
        { TEXT("VertexNormalWS"),      UMaterialExpressionVertexNormalWS::StaticClass() },
        { TEXT("Noise"),               UMaterialExpressionNoise::StaticClass() },
        { TEXT("Constant"),            UMaterialExpressionConstant::StaticClass() },
        { TEXT("Constant2Vector"),     UMaterialExpressionConstant2Vector::StaticClass() },
        { TEXT("Constant3Vector"),     UMaterialExpressionConstant3Vector::StaticClass() },
        { TEXT("Constant4Vector"),     UMaterialExpressionConstant4Vector::StaticClass() },
        { TEXT("ScalarParameter"),     UMaterialExpressionScalarParameter::StaticClass() },
        { TEXT("VectorParameter"),     UMaterialExpressionVectorParameter::StaticClass() },
    };
    return EXPRESSION_TYPES;
}

static UClass* ResolveExpressionClass(const FString& TypeName)
{
    UClass* const* Found = GetExpressionTypeMap().Find(TypeName);
    return Found ? *Found : nullptr;
}

UMaterialExpression* FUnrealMCPMaterialNodeCommands::CreateExpressionByType(
    UMaterial* Material, const FString& ExpressionType, const FString& NodeId, FString& OutError)
{
    UClass* Class = ResolveExpressionClass(ExpressionType);
    if (!Class)
    {
        OutError = FString::Printf(TEXT("Tipo de expression desconocido '%s'"), *ExpressionType);
        return nullptr;
    }

    if (FindExpressionByNodeId(Material, NodeId))
    {
        OutError = FString::Printf(TEXT("Ya existe un nodo con node_id '%s'"), *NodeId);
        return nullptr;
    }

    UMaterialExpression* NewExpr = NewObject<UMaterialExpression>(Material, Class, FName(*NodeId), RF_Transactional);
    if (!NewExpr)
    {
        OutError = TEXT("Fallo al instanciar la expression");
        return nullptr;
    }
    Material->GetExpressionCollection().AddExpression(NewExpr);
    return NewExpr;
}

UMaterialExpression* FUnrealMCPMaterialNodeCommands::FindExpressionByNodeId(UMaterial* Material, const FString& NodeId)
{
    for (const TObjectPtr<UMaterialExpression>& Expr : Material->GetExpressionCollection().Expressions)
    {
        if (Expr && Expr->GetName() == NodeId)
        {
            return Expr.Get();
        }
    }
    return nullptr;
}

bool FUnrealMCPMaterialNodeCommands::SetExpressionInputPin(
    UMaterialExpression* TargetNode, const FString& PinName, UMaterialExpression* SourceNode, int32 OutputIndex, FString& OutError)
{
    auto Wire = [&](FExpressionInput& Pin)
    {
        Pin.Expression = SourceNode;
        Pin.OutputIndex = OutputIndex;
    };

    // -- Binarios A/B --
    if (auto* N = Cast<UMaterialExpressionAdd>(TargetNode))
    {
        if (PinName == TEXT("A")) { Wire(N->A); return true; }
        if (PinName == TEXT("B")) { Wire(N->B); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionSubtract>(TargetNode))
    {
        if (PinName == TEXT("A")) { Wire(N->A); return true; }
        if (PinName == TEXT("B")) { Wire(N->B); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionMultiply>(TargetNode))
    {
        if (PinName == TEXT("A")) { Wire(N->A); return true; }
        if (PinName == TEXT("B")) { Wire(N->B); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionDivide>(TargetNode))
    {
        if (PinName == TEXT("A")) { Wire(N->A); return true; }
        if (PinName == TEXT("B")) { Wire(N->B); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionDotProduct>(TargetNode))
    {
        if (PinName == TEXT("A")) { Wire(N->A); return true; }
        if (PinName == TEXT("B")) { Wire(N->B); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionPower>(TargetNode))
    {
        if (PinName == TEXT("Base")) { Wire(N->Base); return true; }
        if (PinName == TEXT("Exponent")) { Wire(N->Exponent); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionDistance>(TargetNode))
    {
        if (PinName == TEXT("A")) { Wire(N->A); return true; }
        if (PinName == TEXT("B")) { Wire(N->B); return true; }
    }

    // -- Lerp: A, B, Alpha --
    if (auto* N = Cast<UMaterialExpressionLinearInterpolate>(TargetNode))
    {
        if (PinName == TEXT("A")) { Wire(N->A); return true; }
        if (PinName == TEXT("B")) { Wire(N->B); return true; }
        if (PinName == TEXT("Alpha")) { Wire(N->Alpha); return true; }
    }

    // -- Unarios: Input --
    if (auto* N = Cast<UMaterialExpressionClamp>(TargetNode))
    {
        if (PinName == TEXT("Input")) { Wire(N->Input); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionOneMinus>(TargetNode))
    {
        if (PinName == TEXT("Input")) { Wire(N->Input); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionDesaturation>(TargetNode))
    {
        if (PinName == TEXT("Input")) { Wire(N->Input); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionNormalize>(TargetNode))
    {
        if (PinName == TEXT("VectorInput")) { Wire(N->VectorInput); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionComponentMask>(TargetNode))
    {
        if (PinName == TEXT("Input")) { Wire(N->Input); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionAppendVector>(TargetNode))
    {
        if (PinName == TEXT("A")) { Wire(N->A); return true; }
        if (PinName == TEXT("B")) { Wire(N->B); return true; }
    }
    if (auto* N = Cast<UMaterialExpressionTransformPosition>(TargetNode))
    {
        if (PinName == TEXT("Input")) { Wire(N->Input); return true; }
    }

    // -- Noise: Position --
    if (auto* N = Cast<UMaterialExpressionNoise>(TargetNode))
    {
        if (PinName == TEXT("Position")) { Wire(N->Position); return true; }
        if (PinName == TEXT("FilterWidth")) { Wire(N->FilterWidth); return true; }
    }

    // -- Panner: Coordinate, Time --
    if (auto* N = Cast<UMaterialExpressionPanner>(TargetNode))
    {
        if (PinName == TEXT("Coordinate")) { Wire(N->Coordinate); return true; }
        if (PinName == TEXT("Time")) { Wire(N->Time); return true; }
    }

    // -- TextureSample: Coordinates --
    if (auto* N = Cast<UMaterialExpressionTextureSample>(TargetNode))
    {
        if (PinName == TEXT("Coordinates")) { Wire(N->Coordinates); return true; }
    }

    // -- Fresnel: Normal, ExponentIn, BaseReflectFractionIn --
    if (auto* N = Cast<UMaterialExpressionFresnel>(TargetNode))
    {
        if (PinName == TEXT("Normal")) { Wire(N->Normal); return true; }
        if (PinName == TEXT("ExponentIn")) { Wire(N->ExponentIn); return true; }
        if (PinName == TEXT("BaseReflectFractionIn")) { Wire(N->BaseReflectFractionIn); return true; }
    }

    OutError = FString::Printf(TEXT("El nodo '%s' (clase %s) no tiene un pin de entrada llamado '%s' -- revisa el nombre o agrega el caso en SetExpressionInputPin()"),
        *TargetNode->GetName(), *TargetNode->GetClass()->GetName(), *PinName);
    return false;
}

bool FUnrealMCPMaterialNodeCommands::SaveMaterialPackage(UMaterial* Material)
{
    UPackage* Package = Material->GetOutermost();
    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(Package, Material, *PackageFileName, SaveArgs);
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("add_material_expression"))
        return HandleAddMaterialExpression(Params);
    if (CommandType == TEXT("connect_material_expressions"))
        return HandleConnectMaterialExpressions(Params);
    if (CommandType == TEXT("set_material_output"))
        return HandleSetMaterialOutput(Params);
    if (CommandType == TEXT("list_material_expressions"))
        return HandleListMaterialExpressions(Params);
    if (CommandType == TEXT("set_material_expression_constant"))
        return HandleSetMaterialExpressionConstant(Params);
    if (CommandType == TEXT("list_available_expression_types"))
        return HandleListAvailableExpressionTypes(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown material node command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath, ExpressionType, NodeId;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    if (!Params->TryGetStringField(TEXT("expression_type"), ExpressionType))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'expression_type'"));
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' (nombre unico que vos elegis para referenciar este nodo despues)"));

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    double PosX = 0.0, PosY = 0.0;
    Params->TryGetNumberField(TEXT("position_x"), PosX);
    Params->TryGetNumberField(TEXT("position_y"), PosY);

    FString Error;
    UMaterialExpression* NewExpr = CreateExpressionByType(Material, ExpressionType, NodeId, Error);
    if (!NewExpr)
        return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

    NewExpr->MaterialExpressionEditorX = (int32)PosX;
    NewExpr->MaterialExpressionEditorY = (int32)PosY;

    Material->Modify();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), NodeId);
    ResultObj->SetStringField(TEXT("expression_type"), ExpressionType);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath, SourceNodeId, TargetNodeId, TargetPin;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id'"));
    if (!Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id'"));
    if (!Params->TryGetStringField(TEXT("target_pin"), TargetPin))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin'"));

    int32 SourceOutputIndex = 0;
    double OutIdx;
    if (Params->TryGetNumberField(TEXT("source_output_index"), OutIdx))
        SourceOutputIndex = (int32)OutIdx;

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    UMaterialExpression* SourceNode = FindExpressionByNodeId(Material, SourceNodeId);
    if (!SourceNode)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("No se encontro node_id '%s' (¿lo creaste con add_material_expression en este mismo material?)"), *SourceNodeId));

    UMaterialExpression* TargetNode = FindExpressionByNodeId(Material, TargetNodeId);
    if (!TargetNode)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("No se encontro node_id '%s'"), *TargetNodeId));

    FString Error;
    if (!SetExpressionInputPin(TargetNode, TargetPin, SourceNode, SourceOutputIndex, Error))
        return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

    Material->Modify();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleSetMaterialOutput(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath, OutputPin, NodeId;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    if (!Params->TryGetStringField(TEXT("output_pin"), OutputPin))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'output_pin' (BaseColor|Metallic|Specular|Roughness|EmissiveColor|Opacity|OpacityMask|Normal|WorldPositionOffset|AmbientOcclusion)"));
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));

    int32 OutputIndex = 0;
    double OutIdx;
    if (Params->TryGetNumberField(TEXT("output_index"), OutIdx))
        OutputIndex = (int32)OutIdx;

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    UMaterialExpression* Node = FindExpressionByNodeId(Material, NodeId);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("No se encontro node_id '%s'"), *NodeId));

    auto* EditorOnly = Material->GetEditorOnlyData();
    FExpressionInput* TargetInput = nullptr;

    if (OutputPin == TEXT("BaseColor")) TargetInput = &EditorOnly->BaseColor;
    else if (OutputPin == TEXT("Metallic")) TargetInput = &EditorOnly->Metallic;
    else if (OutputPin == TEXT("Specular")) TargetInput = &EditorOnly->Specular;
    else if (OutputPin == TEXT("Roughness")) TargetInput = &EditorOnly->Roughness;
    else if (OutputPin == TEXT("EmissiveColor")) TargetInput = &EditorOnly->EmissiveColor;
    else if (OutputPin == TEXT("Opacity")) TargetInput = &EditorOnly->Opacity;
    else if (OutputPin == TEXT("OpacityMask")) TargetInput = &EditorOnly->OpacityMask;
    else if (OutputPin == TEXT("Normal")) TargetInput = &EditorOnly->Normal;
    else if (OutputPin == TEXT("WorldPositionOffset")) TargetInput = &EditorOnly->WorldPositionOffset;
    else if (OutputPin == TEXT("AmbientOcclusion")) TargetInput = &EditorOnly->AmbientOcclusion;
    else
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("output_pin desconocido '%s'"), *OutputPin));

    TargetInput->Expression = Node;
    TargetInput->OutputIndex = OutputIndex;

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    bool bSaved = SaveMaterialPackage(Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleListMaterialExpressions(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (const TObjectPtr<UMaterialExpression>& Expr : Material->GetExpressionCollection().Expressions)
    {
        if (!Expr) continue;
        TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
        NodeObj->SetStringField(TEXT("node_id"), Expr->GetName());
        NodeObj->SetStringField(TEXT("class"), Expr->GetClass()->GetName());
        Nodes.Add(MakeShared<FJsonValueObject>(NodeObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("nodes"), Nodes);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleSetMaterialExpressionConstant(const TSharedPtr<FJsonObject>& Params)
{
    // Setea el valor "default" de nodos constantes/parametro (Constant,
    // Constant2/3/4Vector, ScalarParameter, VectorParameter) sin tener
    // que conectarles nada -- util para tunear el grafo despues de armarlo.
    FString MaterialPath, NodeId;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));

    const TArray<TSharedPtr<FJsonValue>>* ValueArray;
    if (!Params->TryGetArrayField(TEXT("value"), ValueArray) || ValueArray->Num() == 0)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' (array: [x] para escalar, [r,g,b] o [r,g,b,a] para vector)"));

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    UMaterialExpression* Node = FindExpressionByNodeId(Material, NodeId);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("No se encontro node_id '%s'"), *NodeId));

    bool bHandled = true;
    if (auto* N = Cast<UMaterialExpressionConstant>(Node)) { N->R = (*ValueArray)[0]->AsNumber(); }
    else if (auto* N = Cast<UMaterialExpressionScalarParameter>(Node)) { N->DefaultValue = (*ValueArray)[0]->AsNumber(); }
    else if (auto* N = Cast<UMaterialExpressionConstant2Vector>(Node)) { N->R = (*ValueArray)[0]->AsNumber(); if (ValueArray->Num() > 1) N->G = (*ValueArray)[1]->AsNumber(); }
    else if (auto* N = Cast<UMaterialExpressionConstant3Vector>(Node))
    {
        FLinearColor C = N->Constant;
        C.R = (*ValueArray)[0]->AsNumber();
        if (ValueArray->Num() > 1) C.G = (*ValueArray)[1]->AsNumber();
        if (ValueArray->Num() > 2) C.B = (*ValueArray)[2]->AsNumber();
        N->Constant = C;
    }
    else if (auto* N = Cast<UMaterialExpressionConstant4Vector>(Node))
    {
        FLinearColor C = N->Constant;
        C.R = (*ValueArray)[0]->AsNumber();
        if (ValueArray->Num() > 1) C.G = (*ValueArray)[1]->AsNumber();
        if (ValueArray->Num() > 2) C.B = (*ValueArray)[2]->AsNumber();
        if (ValueArray->Num() > 3) C.A = (*ValueArray)[3]->AsNumber();
        N->Constant = C;
    }
    else if (auto* N = Cast<UMaterialExpressionVectorParameter>(Node))
    {
        FLinearColor C = N->DefaultValue;
        C.R = (*ValueArray)[0]->AsNumber();
        if (ValueArray->Num() > 1) C.G = (*ValueArray)[1]->AsNumber();
        if (ValueArray->Num() > 2) C.B = (*ValueArray)[2]->AsNumber();
        if (ValueArray->Num() > 3) C.A = (*ValueArray)[3]->AsNumber();
        N->DefaultValue = C;
    }
    else
    {
        bHandled = false;
    }

    if (!bHandled)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("El nodo '%s' (clase %s) no es un tipo constante/parametro soportado por este comando"), *NodeId, *Node->GetClass()->GetName()));

    Material->Modify();
    Material->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// list_available_expression_types -- introspeccion en vivo del mapa
// EXPRESSION_TYPES (via GetExpressionTypeMap()), el mismo que usa
// CreateExpressionByType/ResolveExpressionClass. No hardcodea una lista
// nueva del lado C++ ni del lado Python -- el objetivo es poder
// verificar el vocabulario documentado en material_node_tools.md contra
// el plugin compilado real. No devuelve la tabla de target_pin por tipo
// (eso sigue viviendo solo en SetExpressionInputPin() y en el doc,
// porque duplicarla ahi requeriria una segunda tabla de datos separada
// del switch actual) -- solo confirma que expression_type existen.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleListAvailableExpressionTypes(const TSharedPtr<FJsonObject>& Params)
{
    TArray<FString> Names;
    GetExpressionTypeMap().GetKeys(Names);
    Names.Sort();

    TArray<TSharedPtr<FJsonValue>> TypeArray;
    for (const FString& Name : Names)
    {
        UClass* Class = GetExpressionTypeMap()[Name];
        TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
        TypeObj->SetStringField(TEXT("expression_type"), Name);
        TypeObj->SetStringField(TEXT("class_name"), Class ? Class->GetName() : TEXT("Unknown"));
        TypeArray.Add(MakeShared<FJsonValueObject>(TypeObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("expression_types"), TypeArray);
    ResultObj->SetNumberField(TEXT("count"), TypeArray.Num());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}
#pragma clang diagnostic pop
