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
#include "Factories/MaterialFactoryNew.h"
#include "PackageTools.h"

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

// LoadOrCreateAssetPackage de UnrealMCPEditorCommands.cpp es static (file-local)
// y el modulo es unity: no se puede redefinir aqui. Misma logica y mismo
// comentario: cargar si existe en disco (queda fully-loaded) o crear vacio;
// nunca guardar un package nunca cargado (crash del run 2, SavePackage2.cpp:195).
static UPackage* LoadOrCreateAssetPackageMatNode(const FString& PackageName, FString& OutErrorMsg)
{
    UPackage* Package = FindPackage(nullptr, *PackageName);
    if (!Package || !Package->IsFullyLoaded())
    {
        if (FPackageName::DoesPackageExist(PackageName))
        {
            Package = LoadPackage(nullptr, *PackageName, LOAD_None);
        }
    }
    if (!Package)
    {
        Package = CreatePackage(*PackageName);
    }
    if (!Package)
    {
        OutErrorMsg = FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
        return nullptr;
    }
    if (!Package->IsFullyLoaded())
    {
        OutErrorMsg = FString::Printf(
            TEXT("Package '%s' is not fully loaded; refusing to save it (would clobber on-disk content)"),
            *PackageName);
        return nullptr;
    }
    return Package;
}

// Itera todos los FExpressionInput reflejados de una expression y devuelve
// cada pin con su nombre. Mismo patron que UMaterialExpression::GetInputName
// (MaterialExpressions.cpp:1692): TFieldIterator<FStructProperty> con
// Struct->GetFName() == "ExpressionInput"; nombre = InputName si no es
// None, si no el nombre de propiedad (+_N solo si ArrayDim>1). Callback
// devuelve true para cortar la iteracion.
template <typename CallbackType>
static void ForEachExpressionInput(UMaterialExpression* Expr, CallbackType&& Callback)
{
    if (!Expr)
    {
        return;
    }
    for (TFieldIterator<FStructProperty> It(Expr->GetClass(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated); It; ++It)
    {
        FStructProperty* StructProp = *It;
        if (StructProp->Struct->GetFName() != FName(TEXT("ExpressionInput")))
        {
            continue;
        }
        for (int32 ArrayIndex = 0; ArrayIndex < StructProp->ArrayDim; ++ArrayIndex)
        {
            FExpressionInput& Input = *StructProp->ContainerPtrToValuePtr<FExpressionInput>(Expr, ArrayIndex);
            FString PinName;
            if (!Input.InputName.IsNone())
            {
                PinName = Input.InputName.ToString();
            }
            else
            {
                PinName = StructProp->GetFName().ToString();
                if (StructProp->ArrayDim > 1)
                {
                    PinName += FString::Printf(TEXT("_%d"), ArrayIndex);
                }
            }
            if (Callback(PinName, Input))
            {
                return;
            }
        }
    }
}

// Anula toda referencia a Node dentro de Material: inputs de todas las
// expressions y outputs editor-only del material. Necesario antes de
// RemoveExpression+Rename, si no el material queda con un puntero colgando
// a un objeto que se movio a transient (dangling en el editor / crash al
// guardar).
static void NullNodeReferences(UMaterial* Material, UMaterialExpression* Node)
{
    for (const TObjectPtr<UMaterialExpression>& Expr : Material->GetExpressionCollection().Expressions)
    {
        if (!Expr || Expr == Node)
        {
            continue;
        }
        ForEachExpressionInput(Expr.Get(), [Node](const FString&, FExpressionInput& Input)
        {
            if (Input.Expression == Node)
            {
                Input.Expression = nullptr;
                Input.OutputIndex = 0;
            }
            return false;
        });
    }
    if (UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData())
    {
        auto ClearIfMatch = [Node](FExpressionInput& Input)
        {
            if (Input.Expression == Node)
            {
                Input.Expression = nullptr;
                Input.OutputIndex = 0;
            }
        };
        ClearIfMatch(EditorOnly->BaseColor);
        ClearIfMatch(EditorOnly->Metallic);
        ClearIfMatch(EditorOnly->Specular);
        ClearIfMatch(EditorOnly->Roughness);
        ClearIfMatch(EditorOnly->EmissiveColor);
        ClearIfMatch(EditorOnly->Opacity);
        ClearIfMatch(EditorOnly->OpacityMask);
        ClearIfMatch(EditorOnly->Normal);
        ClearIfMatch(EditorOnly->WorldPositionOffset);
        ClearIfMatch(EditorOnly->AmbientOcclusion);
    }
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
    // Solo guardar contenido de /Game/: un material de /Engine u otro plugin
    // no se toca (evita pisar assets del motor en disco). El caller recibe
    // saved_to_disk=false y sigue siendo un exito logico (la mutacion en
    // memoria se aplico).
    if (!Package->GetName().StartsWith(TEXT("/Game/")))
    {
        return false;
    }
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
    if (CommandType == TEXT("create_empty_material"))
        return HandleCreateEmptyMaterial(Params);
    if (CommandType == TEXT("delete_material_expression"))
        return HandleDeleteMaterialExpression(Params);
    if (CommandType == TEXT("set_material_expression_property"))
        return HandleSetMaterialExpressionProperty(Params);
    if (CommandType == TEXT("get_material_expression"))
        return HandleGetMaterialExpression(Params);
    if (CommandType == TEXT("disconnect_material_input"))
        return HandleDisconnectMaterialInput(Params);
    if (CommandType == TEXT("save_material"))
        return HandleSaveMaterial(Params);

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
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
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
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
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
        // Retrocompatible: se suman campos, no se quitan los originales.
        NodeObj->SetStringField(TEXT("expression_type"), GetExpressionTypeForClass(Expr->GetClass()));
        NodeObj->SetNumberField(TEXT("position_x"), Expr->MaterialExpressionEditorX);
        NodeObj->SetNumberField(TEXT("position_y"), Expr->MaterialExpressionEditorY);
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
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
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

FString FUnrealMCPMaterialNodeCommands::GetExpressionTypeForClass(UClass* Class) const
{
    if (!Class)
    {
        return FString();
    }
    for (const TPair<FString, UClass*>& Pair : GetExpressionTypeMap())
    {
        if (Pair.Value == Class)
        {
            return Pair.Key;
        }
    }
    return FString();
}

// =====================================================================
// Dump de un escalar reflejado a JSON para get_material_expression.
// Devuelve nullptr si el tipo no tiene representacion JSON simple (arrays
// no numericos, structs arbitrarios, FExpressionInput -- que ya se cubre
// en la seccion inputs[]). Numerics: signo correcto via NumericIsUnsigned();
// byte/enum: nombre del enum si esta definido (ej. Mobility -> "Static").
// =====================================================================
// FNumericProperty no expone IsUnsigned() en UE5.3 -> chequear subclases.
static bool NumericIsUnsigned(const FNumericProperty* Prop)
{
    return Prop->IsA<FByteProperty>() || Prop->IsA<FUInt16Property>() ||
           Prop->IsA<FUInt32Property>() || Prop->IsA<FUInt64Property>();
}

static TSharedPtr<FJsonValue> JsonValueFromPropertyElement(const FProperty* Property, const void* ElementPtr)
{
    if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ElementPtr));
    }
    if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (UEnum* EnumDef = ByteProp->GetIntPropertyEnum())
        {
            const uint8 Raw = ByteProp->GetPropertyValue(ElementPtr);
            const FString EnumName = EnumDef->GetNameStringByValue((int64)Raw);
            if (!EnumName.IsEmpty())
            {
                return MakeShared<FJsonValueString>(EnumName);
            }
            return MakeShared<FJsonValueNumber>((double)Raw);
        }
        return MakeShared<FJsonValueNumber>((double)ByteProp->GetPropertyValue(ElementPtr));
    }
    if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        if (const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty())
        {
            const int64 Raw = NumericIsUnsigned(Underlying)
                ? (int64)Underlying->GetUnsignedIntPropertyValue(ElementPtr)
                : Underlying->GetSignedIntPropertyValue(ElementPtr);
            const FString EnumName = EnumProp->GetEnum() ? EnumProp->GetEnum()->GetNameStringByValue(Raw) : FString();
            if (!EnumName.IsEmpty())
            {
                return MakeShared<FJsonValueString>(EnumName);
            }
            return MakeShared<FJsonValueNumber>((double)Raw);
        }
        return nullptr;
    }
    if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
    {
        if (NumProp->IsInteger())
        {
            const double Value = NumericIsUnsigned(NumProp)
                ? (double)NumProp->GetUnsignedIntPropertyValue(ElementPtr)
                : (double)NumProp->GetSignedIntPropertyValue(ElementPtr);
            return MakeShared<FJsonValueNumber>(Value);
        }
        return MakeShared<FJsonValueNumber>(NumProp->GetFloatingPointPropertyValue(ElementPtr));
    }
    if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ElementPtr));
    }
    if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ElementPtr).ToString());
    }
    if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ElementPtr).ToString());
    }
    if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
    {
        if (const UObject* Obj = ObjProp->GetObjectPropertyValue(ElementPtr))
        {
            return MakeShared<FJsonValueString>(Obj->GetPathName());
        }
        return nullptr;
    }
    if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor* Color = (const FLinearColor*)ElementPtr;
            TArray<TSharedPtr<FJsonValue>> Values;
            Values.Add(MakeShared<FJsonValueNumber>(Color->R));
            Values.Add(MakeShared<FJsonValueNumber>(Color->G));
            Values.Add(MakeShared<FJsonValueNumber>(Color->B));
            Values.Add(MakeShared<FJsonValueNumber>(Color->A));
            return MakeShared<FJsonValueArray>(Values);
        }
        if (StructProp->Struct == TBaseStructure<FColor>::Get())
        {
            const FColor* Color = (const FColor*)ElementPtr;
            TArray<TSharedPtr<FJsonValue>> Values;
            Values.Add(MakeShared<FJsonValueNumber>(Color->R / 255.0));
            Values.Add(MakeShared<FJsonValueNumber>(Color->G / 255.0));
            Values.Add(MakeShared<FJsonValueNumber>(Color->B / 255.0));
            Values.Add(MakeShared<FJsonValueNumber>(Color->A / 255.0));
            return MakeShared<FJsonValueArray>(Values);
        }
        if (StructProp->Struct == TBaseStructure<FVector>::Get())
        {
            const FVector* Vector = (const FVector*)ElementPtr;
            TArray<TSharedPtr<FJsonValue>> Values;
            Values.Add(MakeShared<FJsonValueNumber>(Vector->X));
            Values.Add(MakeShared<FJsonValueNumber>(Vector->Y));
            Values.Add(MakeShared<FJsonValueNumber>(Vector->Z));
            return MakeShared<FJsonValueArray>(Values);
        }
        return nullptr;
    }
    return nullptr;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleCreateEmptyMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("name"), MaterialName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));

    FString FolderPath = TEXT("/Game/Materials");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    const FString PackageName = UPackageTools::SanitizePackageName(FolderPath / MaterialName);

    FString PackageErrorMsg;
    UPackage* Package = LoadOrCreateAssetPackageMatNode(PackageName, PackageErrorMsg);
    if (!Package)
        return FUnrealMCPCommonUtils::CreateErrorResponse(PackageErrorMsg);

    // Reusar la si ya existe (disco o memoria): vaciar en sitio conserva
    // las referencias de actores que ya la tengan asignada (mismo patron
    // que HandleCreateMaterial en UnrealMCPEditorCommands.cpp).
    bool bNewlyCreated = false;
    UMaterial* Material = FindObject<UMaterial>(Package, *MaterialName);
    if (Material)
    {
        for (UMaterialExpression* OldExpr : Material->GetExpressions())
        {
            if (OldExpr)
            {
                OldExpr->Rename(nullptr, GetTransientPackage(),
                    REN_DontCreateRedirectors | REN_ForceGlobalUnique);
            }
        }
        Material->GetExpressionCollection().Empty();
        if (UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData())
        {
            auto ClearOutput = [](FExpressionInput& Input)
            {
                Input.Expression = nullptr;
                Input.OutputIndex = 0;
            };
            ClearOutput(EditorOnly->BaseColor);
            ClearOutput(EditorOnly->Roughness);
            ClearOutput(EditorOnly->Metallic);
            ClearOutput(EditorOnly->Specular);
            ClearOutput(EditorOnly->EmissiveColor);
            ClearOutput(EditorOnly->Opacity);
            ClearOutput(EditorOnly->OpacityMask);
            ClearOutput(EditorOnly->Normal);
            ClearOutput(EditorOnly->WorldPositionOffset);
            ClearOutput(EditorOnly->AmbientOcclusion);
        }
    }
    else
    {
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        Material = Cast<UMaterial>(Factory->FactoryCreateNew(
            UMaterial::StaticClass(), Package, FName(*MaterialName), RF_Standalone | RF_Public, nullptr, GWarn));
        bNewlyCreated = true;
    }

    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create material asset"));

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (bNewlyCreated)
    {
        FAssetRegistryModule::AssetCreated(Material);
    }

    const bool bSaved = SaveMaterialPackage(Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), Material->GetPathName());
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleDeleteMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath, NodeId;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    UMaterialExpression* Node = FindExpressionByNodeId(Material, NodeId);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Nodo '%s' no existe en '%s'"), *NodeId, *MaterialPath));

    Material->Modify();
    Node->Modify();

    // 1) anular referencias antes de sacarlo del array, si no el material
    //    queda con un puntero colgado a un objeto que va a transient;
    // 2) quitarlo de la coleccion;
    // 3) moverlo a transient con nombre unico -- eso libera el node_id para
    //    poder reusarse en una proxima creacion (mismo patron que OldExpr en
    //    HandleCreateMaterial).
    NullNodeReferences(Material, Node);
    Material->GetExpressionCollection().RemoveExpression(Node);
    Node->Rename(nullptr, GetTransientPackage(),
        REN_DontCreateRedirectors | REN_ForceGlobalUnique | REN_NonTransactional);

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    const bool bSaved = SaveMaterialPackage(Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("deleted_node_id"), NodeId);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleSetMaterialExpressionProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath, NodeId, PropertyName;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));
    if (!Params->TryGetStringField(TEXT("property"), PropertyName))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property'"));
    if (!Params->HasField(TEXT("value")))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value'"));

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    UMaterialExpression* Node = FindExpressionByNodeId(Material, NodeId);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Nodo '%s' no existe en '%s'"), *NodeId, *MaterialPath));

    const TSharedPtr<FJsonValue> ValueJson = Params->Values.FindRef(TEXT("value"));
    FString Error;
    if (!FUnrealMCPCommonUtils::SetObjectProperty(Node, PropertyName, ValueJson, Error))
        return FUnrealMCPCommonUtils::CreateErrorResponse(Error);

    Material->Modify();
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    const bool bSaved = SaveMaterialPackage(Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), NodeId);
    ResultObj->SetStringField(TEXT("property"), PropertyName);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleGetMaterialExpression(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath, NodeId;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    UMaterialExpression* Node = FindExpressionByNodeId(Material, NodeId);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Nodo '%s' no existe en '%s'"), *NodeId, *MaterialPath));

    TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
    NodeObj->SetStringField(TEXT("node_id"), Node->GetName());
    NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    NodeObj->SetStringField(TEXT("expression_type"), GetExpressionTypeForClass(Node->GetClass()));
    NodeObj->SetNumberField(TEXT("position_x"), Node->MaterialExpressionEditorX);
    NodeObj->SetNumberField(TEXT("position_y"), Node->MaterialExpressionEditorY);

    // Pines de entrada: reflection generica (ForEachExpressionInput) --
    // funciona para cualquier clase sin tener que mantener un switch.
    TArray<TSharedPtr<FJsonValue>> Inputs;
    ForEachExpressionInput(Node, [&](const FString& PinName, FExpressionInput& Input)
    {
        TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
        PinObj->SetStringField(TEXT("pin"), PinName);
        PinObj->SetBoolField(TEXT("connected"), Input.Expression != nullptr);
        if (Input.Expression)
        {
            PinObj->SetStringField(TEXT("source_node_id"), Input.Expression->GetName());
            PinObj->SetNumberField(TEXT("output_index"), Input.OutputIndex);
        }
        Inputs.Add(MakeShared<FJsonValueObject>(PinObj));
        return false;
    });
    NodeObj->SetArrayField(TEXT("inputs"), Inputs);

    TArray<TSharedPtr<FJsonValue>> Outputs;
    for (int32 OutputIndex = 0; OutputIndex < Node->Outputs.Num(); ++OutputIndex)
    {
        Outputs.Add(MakeShared<FJsonValueNumber>(OutputIndex));
    }
    NodeObj->SetArrayField(TEXT("outputs"), Outputs);

    // Dump de propiedades escalares (R, G, B, ConstA, Texture, SamplerType,
    // bClamp...). FExpressionInput se salta aca: ya esta en inputs[].
    TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
    for (TFieldIterator<FProperty> It(Node->GetClass()); It; ++It)
    {
        const FProperty* Property = *It;
        if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
        {
            if (StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput")))
            {
                continue;
            }
        }
        const void* BasePtr = Property->ContainerPtrToValuePtr<void>(Node);
        if (Property->ArrayDim == 1)
        {
            const TSharedPtr<FJsonValue> Value = JsonValueFromPropertyElement(Property, BasePtr);
            if (Value.IsValid())
            {
                Props->SetField(Property->GetName(), Value);
            }
        }
        else
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
            {
                const TSharedPtr<FJsonValue> Value = JsonValueFromPropertyElement(
                    Property, (const char*)BasePtr + Property->ElementSize * Index);
                if (!Value.IsValid())
                {
                    Values.Reset();
                    break;
                }
                Values.Add(Value);
            }
            if (Values.Num() > 0)
            {
                Props->SetArrayField(Property->GetName(), Values);
            }
        }
    }
    NodeObj->SetObjectField(TEXT("properties"), Props);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetObjectField(TEXT("expression"), NodeObj);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleDisconnectMaterialInput(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath, NodeId, TargetPin;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id'"));
    if (!Params->TryGetStringField(TEXT("target_pin"), TargetPin))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin'"));

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    UMaterialExpression* Node = FindExpressionByNodeId(Material, NodeId);
    if (!Node)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Nodo '%s' no existe en '%s'"), *NodeId, *MaterialPath));

    bool bFound = false;
    FString AvailablePins;
    ForEachExpressionInput(Node, [&](const FString& PinName, FExpressionInput& Input)
    {
        if (!AvailablePins.IsEmpty())
        {
            AvailablePins += TEXT(", ");
        }
        AvailablePins += PinName;
        if (PinName == TargetPin)
        {
            Input.Expression = nullptr;
            Input.OutputIndex = 0;
            bFound = true;
            return true;
        }
        return false;
    });

    if (!bFound)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("El nodo '%s' no tiene el pin '%s'. Pines disponibles: [%s]"),
            *NodeId, *TargetPin, *AvailablePins));
    }

    Material->Modify();
    Node->Modify();
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    const bool bSaved = SaveMaterialPackage(Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), NodeId);
    ResultObj->SetStringField(TEXT("disconnected_pin"), TargetPin);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPMaterialNodeCommands::HandleSaveMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to load UMaterial en '%s'"), *MaterialPath));

    Material->MarkPackageDirty();
    const bool bSaved = SaveMaterialPackage(Material);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), Material->GetPathName());
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}
#pragma clang diagnostic pop
