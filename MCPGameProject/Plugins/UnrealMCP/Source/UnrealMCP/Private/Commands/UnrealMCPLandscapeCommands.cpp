#include "Commands/UnrealMCPLandscapeCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeLayerInfoObject.h"
#include "Math/UnrealMathUtility.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

FUnrealMCPLandscapeCommands::FUnrealMCPLandscapeCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_landscape_info"))
    {
        return HandleGetLandscapeInfo(Params);
    }
    else if (CommandType == TEXT("sculpt_landscape_region"))
    {
        return HandleSculptLandscapeRegion(Params);
    }
    else if (CommandType == TEXT("paint_landscape_layer"))
    {
        return HandlePaintLandscapeLayer(Params);
    }
    else if (CommandType == TEXT("create_landscape_layer_info"))
    {
        return HandleCreateLandscapeLayerInfo(Params);
    }
    else if (CommandType == TEXT("add_landscape_material_layer_blend_input"))
    {
        return HandleAddLandscapeMaterialLayerBlendInput(Params);
    }

    else if (CommandType == TEXT("create_landscape_material_with_layer_blend"))
    {
        return HandleCreateLandscapeMaterialWithLayerBlend(Params);
    }
    else if (CommandType == TEXT("set_landscape_material"))
    {
        return HandleSetLandscapeMaterial(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown landscape command: %s"), *CommandType));
}

// Shared helper: find a Landscape actor by name (actor label or object name),
// or the first Landscape found in the world if no name is given.
static ALandscape* FindLandscapeByName(UWorld* World, const FString& LandscapeName)
{
    for (TActorIterator<ALandscape> It(World); It; ++It)
    {
        if (LandscapeName.IsEmpty() || It->GetActorLabel() == LandscapeName || It->GetName() == LandscapeName)
        {
            return *It;
        }
    }
    return nullptr;
}

// =====================================================================
// get_landscape_info
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found in the level"));
    }

    ULandscapeInfo* LandscapeInfo = FoundLandscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape has no LandscapeInfo (not registered?)"));
    }

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    const bool bHasExtent = LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
    ResultObj->SetBoolField(TEXT("has_extent"), bHasExtent);
    if (bHasExtent)
    {
        // Coordenadas en "quads" (unidades de heightmap), no en cm de mundo.
        // Usar estos valores como rango válido para x/y en sculpt/paint.
        ResultObj->SetNumberField(TEXT("min_x"), MinX);
        ResultObj->SetNumberField(TEXT("min_y"), MinY);
        ResultObj->SetNumberField(TEXT("max_x"), MaxX);
        ResultObj->SetNumberField(TEXT("max_y"), MaxY);
    }
    ResultObj->SetNumberField(TEXT("component_size_quads"), FoundLandscape->ComponentSizeQuads);
    ResultObj->SetNumberField(TEXT("subsections_per_component"), FoundLandscape->NumSubsections);

    UMaterialInterface* LandscapeMat = FoundLandscape->GetLandscapeMaterial();
    if (LandscapeMat)
    {
        ResultObj->SetStringField(TEXT("material_path"), LandscapeMat->GetPathName());

        bool bHasLayerBlendNode = false;
        TArray<FString> ExistingBlendLayerNames;
        if (UMaterial* BaseMaterial = LandscapeMat->GetMaterial())
        {
            for (UMaterialExpression* Expression : BaseMaterial->GetExpressions())
            {
                if (UMaterialExpressionLandscapeLayerBlend* LayerBlendExpr = Cast<UMaterialExpressionLandscapeLayerBlend>(Expression))
                {
                    bHasLayerBlendNode = true;
                    for (const FLayerBlendInput& Input : LayerBlendExpr->Layers)
                    {
                        ExistingBlendLayerNames.Add(Input.LayerName.ToString());
                    }
                }
            }
        }
        ResultObj->SetBoolField(TEXT("has_layer_blend_node"), bHasLayerBlendNode);

        TArray<TSharedPtr<FJsonValue>> BlendLayerNamesArr;
        for (const FString& Name : ExistingBlendLayerNames)
        {
            BlendLayerNamesArr.Add(MakeShared<FJsonValueString>(Name));
        }
        ResultObj->SetArrayField(TEXT("layer_blend_node_layers"), BlendLayerNamesArr);
    }
    else
    {
        ResultObj->SetStringField(TEXT("material_path"), TEXT(""));
        ResultObj->SetBoolField(TEXT("has_layer_blend_node"), false);
    }

    const FVector Scale = FoundLandscape->GetActorScale3D();
    TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
    ScaleObj->SetNumberField(TEXT("x"), Scale.X);
    ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
    ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
    ResultObj->SetObjectField(TEXT("scale"), ScaleObj);

    TArray<TSharedPtr<FJsonValue>> LayerNamesArr;
    for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
    {
        if (LayerSettings.LayerInfoObj)
        {
            LayerNamesArr.Add(MakeShared<FJsonValueString>(LayerSettings.LayerInfoObj->LayerName.ToString()));
        }
        else if (!LayerSettings.LayerName.IsNone())
        {
            LayerNamesArr.Add(MakeShared<FJsonValueString>(
                FString::Printf(TEXT("%s (sin LayerInfo asset asignado)"), *LayerSettings.LayerName.ToString())));
        }
    }
    ResultObj->SetArrayField(TEXT("layers"), LayerNamesArr);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}

// =====================================================================
// sculpt_landscape_region
//
// NOTE: LANDSCAPE_ZSCALE (1/128) es la constante fija de UE para convertir
// el valor crudo uint16 del heightmap a Z de mundo. Si SetHeightData /
// GetHeightData no compilan con la firma de abajo, grepear "LandscapeEdit.h"
// -- esta API cambió de firma entre versiones 5.x.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleSculptLandscapeRegion(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    double CenterX = 0.0, CenterY = 0.0;
    if (!Params->TryGetNumberField(TEXT("x"), CenterX) || !Params->TryGetNumberField(TEXT("y"), CenterY))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'x'/'y' (coordenadas de quad -- llamar a get_landscape_info para el rango valido)"));
    }

    double Radius = 20.0;
    Params->TryGetNumberField(TEXT("radius"), Radius);

    double StrengthCm = 100.0;
    Params->TryGetNumberField(TEXT("strength_cm"), StrengthCm);

    FString Mode = TEXT("raise");
    Params->TryGetStringField(TEXT("mode"), Mode);
    if (Mode != TEXT("raise") && Mode != TEXT("lower") && Mode != TEXT("flatten") && Mode != TEXT("noise"))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Modo desconocido '%s' -- usar raise, lower, flatten o noise"), *Mode));
    }

    double TargetHeightCm = 0.0;
    const bool bHasTarget = Params->TryGetNumberField(TEXT("target_height_cm"), TargetHeightCm);
    if (Mode == TEXT("flatten") && !bHasTarget)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("El modo 'flatten' requiere 'target_height_cm'"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found"));
    }

    ULandscapeInfo* LandscapeInfo = FoundLandscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape has no LandscapeInfo"));
    }

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not determine landscape extent"));
    }

    const int32 IRadius = FMath::CeilToInt(Radius);
    int32 X1 = FMath::Clamp(FMath::FloorToInt(CenterX) - IRadius, MinX, MaxX);
    int32 Y1 = FMath::Clamp(FMath::FloorToInt(CenterY) - IRadius, MinY, MaxY);
    int32 X2 = FMath::Clamp(FMath::FloorToInt(CenterX) + IRadius, MinX, MaxX);
    int32 Y2 = FMath::Clamp(FMath::FloorToInt(CenterY) + IRadius, MinY, MaxY);

    if (X2 <= X1 || Y2 <= Y1)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("La region de sculpt cae fuera del extent del landscape"));
    }

    const int32 Width = X2 - X1 + 1;
    const int32 Height = Y2 - Y1 + 1;

    const double LandscapeZScale = 1.0 / 128.0;
    const FVector ActorScale = FoundLandscape->GetActorScale3D();
    const double CmPerRawUnit = LandscapeZScale * ActorScale.Z;
    const int32 StrengthRaw = (CmPerRawUnit != 0.0) ? FMath::RoundToInt(StrengthCm / CmPerRawUnit) : 0;
    const int32 TargetRaw = (bHasTarget && CmPerRawUnit != 0.0)
        ? (32768 + FMath::RoundToInt(TargetHeightCm / CmPerRawUnit))
        : 32768;

    FLandscapeEditDataInterface EditInterface(LandscapeInfo);

    TArray<uint16> HeightData;
    HeightData.AddZeroed(Width * Height);
    EditInterface.GetHeightData(X1, Y1, X2, Y2, HeightData.GetData(), 0);

    for (int32 Y = Y1; Y <= Y2; ++Y)
    {
        for (int32 X = X1; X <= X2; ++X)
        {
            const double Dist = FVector2D((double)(X - CenterX), (double)(Y - CenterY)).Size();
            if (Dist > Radius)
            {
                continue;
            }
            const double Falloff = 1.0 - FMath::SmoothStep(0.0, Radius, Dist);
            const int32 Index = (Y - Y1) * Width + (X - X1);
            int32 Value = HeightData[Index];

            if (Mode == TEXT("raise"))
            {
                Value += FMath::RoundToInt(StrengthRaw * Falloff);
            }
            else if (Mode == TEXT("lower"))
            {
                Value -= FMath::RoundToInt(StrengthRaw * Falloff);
            }
            else if (Mode == TEXT("flatten"))
            {
                Value = FMath::RoundToInt(FMath::Lerp((double)Value, (double)TargetRaw, Falloff));
            }
            else // "noise"
            {
                const double NoiseSample = FMath::PerlinNoise2D(FVector2D(X * 0.05, Y * 0.05));
                Value += FMath::RoundToInt(StrengthRaw * Falloff * NoiseSample);
            }

            HeightData[Index] = (uint16)FMath::Clamp(Value, 0, 65535);
        }
    }

    EditInterface.SetHeightData(X1, Y1, X2, Y2, HeightData.GetData(), 0, /*bCalcNormals=*/true);
    FoundLandscape->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
    ResultObj->SetStringField(TEXT("mode"), Mode);
    ResultObj->SetNumberField(TEXT("x"), CenterX);
    ResultObj->SetNumberField(TEXT("y"), CenterY);
    ResultObj->SetNumberField(TEXT("radius"), Radius);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}

// =====================================================================
// paint_landscape_layer
//
// NOTE: la firma exacta de GetWeightData/SetAlphaData tambien ha cambiado
// entre versiones. Si no compila, grepear "LandscapeEdit.h" buscando
// "SetAlphaData" y "GetWeightData" y ajustar solo estas dos llamadas.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandlePaintLandscapeLayer(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    FString LayerName;
    if (!Params->TryGetStringField(TEXT("layer_name"), LayerName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'layer_name' -- llamar a get_landscape_info para ver las capas disponibles"));
    }

    double CenterX = 0.0, CenterY = 0.0;
    if (!Params->TryGetNumberField(TEXT("x"), CenterX) || !Params->TryGetNumberField(TEXT("y"), CenterY))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'x'/'y' (coordenadas de quad)"));
    }

    double Radius = 20.0;
    Params->TryGetNumberField(TEXT("radius"), Radius);

    double Strength = 1.0; // 0.0 - 1.0, peso de pintado en el centro del pincel
    Params->TryGetNumberField(TEXT("strength"), Strength);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found"));
    }

    ULandscapeInfo* LandscapeInfo = FoundLandscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape has no LandscapeInfo"));
    }

    ULandscapeLayerInfoObject* TargetLayerInfo = nullptr;
    for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
    {
        if (LayerSettings.LayerInfoObj && LayerSettings.LayerInfoObj->LayerName.ToString() == LayerName)
        {
            TargetLayerInfo = LayerSettings.LayerInfoObj;
            break;
        }
    }
    if (!TargetLayerInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("Capa '%s' no encontrada en este landscape (o no tiene un LayerInfo asset asignado en la lista de target layers del editor)"), *LayerName));
    }

    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Could not determine landscape extent"));
    }

    const int32 IRadius = FMath::CeilToInt(Radius);
    int32 X1 = FMath::Clamp(FMath::FloorToInt(CenterX) - IRadius, MinX, MaxX);
    int32 Y1 = FMath::Clamp(FMath::FloorToInt(CenterY) - IRadius, MinY, MaxY);
    int32 X2 = FMath::Clamp(FMath::FloorToInt(CenterX) + IRadius, MinX, MaxX);
    int32 Y2 = FMath::Clamp(FMath::FloorToInt(CenterY) + IRadius, MinY, MaxY);

    if (X2 <= X1 || Y2 <= Y1)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("La region de pintado cae fuera del extent del landscape"));
    }

    const int32 Width = X2 - X1 + 1;
    const int32 Height = Y2 - Y1 + 1;

    FLandscapeEditDataInterface EditInterface(LandscapeInfo);

    TArray<uint8> WeightData;
    WeightData.AddZeroed(Width * Height);
    EditInterface.GetWeightData(TargetLayerInfo, X1, Y1, X2, Y2, WeightData.GetData(), 0);

    const double ClampedStrength = FMath::Clamp(Strength, 0.0, 1.0);
    for (int32 Y = Y1; Y <= Y2; ++Y)
    {
        for (int32 X = X1; X <= X2; ++X)
        {
            const double Dist = FVector2D((double)(X - CenterX), (double)(Y - CenterY)).Size();
            if (Dist > Radius)
            {
                continue;
            }
            const double Falloff = 1.0 - FMath::SmoothStep(0.0, Radius, Dist);
            const int32 Index = (Y - Y1) * Width + (X - X1);
            const double NewWeight = FMath::Lerp((double)WeightData[Index], 255.0 * ClampedStrength, Falloff);
            WeightData[Index] = (uint8)FMath::Clamp(NewWeight, 0.0, 255.0);
        }
    }

    EditInterface.SetAlphaData(TargetLayerInfo, X1, Y1, X2, Y2, WeightData.GetData(), 0, ELandscapeLayerPaintingRestriction::None);
    FoundLandscape->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
    ResultObj->SetStringField(TEXT("layer_name"), LayerName);
    ResultObj->SetNumberField(TEXT("x"), CenterX);
    ResultObj->SetNumberField(TEXT("y"), CenterY);
    ResultObj->SetNumberField(TEXT("radius"), Radius);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}

// =====================================================================
// create_landscape_layer_info
//
// Crea un asset ULandscapeLayerInfoObject nuevo y lo registra como target
// layer del landscape. Si el Landscape Material ya define un layer con
// este mismo nombre via un nodo Layer Blend, ese slot existente (con
// LayerInfoObj == nullptr) se completa con el asset nuevo. Si el material
// no lo define, se agrega un slot nuevo directamente al array
// LandscapeInfo->Layers -- pintar en ese layer va a funcionar y quedar
// guardado en el weightmap, pero no se va a VER hasta que el material
// tenga un Layer Blend node con ese nombre.
//
// NOTE: el constructor de FLandscapeInfoLayerSettings y la firma de
// UPackage::SavePackage han cambiado entre versiones de UE5.x. Si esto
// no compila, grepear "FLandscapeInfoLayerSettings(" en LandscapeInfo.h
// y "SavePackage(" en UnrealMCPEditorCommands.cpp (create_material ya
// resuelve el mismo problema de guardado ahi) para copiar la firma
// correcta y pegarla aca.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleCreateLandscapeLayerInfo(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString LayerName;
    if (!Params->TryGetStringField(TEXT("layer_name"), LayerName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'layer_name'"));
    }

    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    FString BlendMode = TEXT("WeightBlend");
    Params->TryGetStringField(TEXT("blend_mode"), BlendMode);
    if (BlendMode != TEXT("WeightBlend") && BlendMode != TEXT("NoWeightBlend"))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Modo desconocido '%s' -- usar WeightBlend o NoWeightBlend"), *BlendMode));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found"));
    }

    ULandscapeInfo* LandscapeInfo = FoundLandscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape has no LandscapeInfo"));
    }

    // Si ya existe un slot con un LayerInfo asset asignado para este nombre, no lo pisamos.
    for (const FLandscapeInfoLayerSettings& ExistingSettings : LandscapeInfo->Layers)
    {
        if (ExistingSettings.LayerInfoObj && ExistingSettings.LayerInfoObj->LayerName.ToString() == LayerName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("La capa '%s' ya tiene un LayerInfo asset asignado"), *LayerName));
        }
    }

    // Crear el asset ULandscapeLayerInfoObject
    const FString PackagePath = FString::Printf(TEXT("/Game/Landscape/Layers/%s"), *LayerName);
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("No se pudo crear el package '%s'"), *PackagePath));
    }
    Package->FullyLoad();

    ULandscapeLayerInfoObject* NewLayerInfo = NewObject<ULandscapeLayerInfoObject>(
        Package, ULandscapeLayerInfoObject::StaticClass(), FName(*LayerName), RF_Public | RF_Standalone);
    if (!NewLayerInfo)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No se pudo crear el ULandscapeLayerInfoObject"));
    }

    NewLayerInfo->LayerName = FName(*LayerName);
    NewLayerInfo->bNoWeightBlend = (BlendMode == TEXT("NoWeightBlend"));

    FAssetRegistryModule::AssetCreated(NewLayerInfo);
    NewLayerInfo->MarkPackageDirty();

    // Guardar a disco
    const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    const bool bSaved = UPackage::SavePackage(Package, NewLayerInfo, *PackageFileName, SaveArgs);
    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("El asset se creo pero no se pudo guardar a disco en '%s'"), *PackageFileName));
    }

    // Registrar (o completar) el slot en LandscapeInfo->Layers
    bool bReusedMaterialSlot = false;
    for (FLandscapeInfoLayerSettings& ExistingSettings : LandscapeInfo->Layers)
    {
        if (!ExistingSettings.LayerInfoObj && ExistingSettings.LayerName == FName(*LayerName))
        {
            ExistingSettings.LayerInfoObj = NewLayerInfo;
            bReusedMaterialSlot = true;
            break;
        }
    }
    if (!bReusedMaterialSlot)
    {
        LandscapeInfo->Layers.Add(FLandscapeInfoLayerSettings(NewLayerInfo, FoundLandscape));
    }

    FoundLandscape->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
    ResultObj->SetStringField(TEXT("layer_name"), LayerName);
    ResultObj->SetStringField(TEXT("blend_mode"), BlendMode);
    ResultObj->SetStringField(TEXT("package_path"), PackagePath);
    ResultObj->SetBoolField(TEXT("reused_material_slot"), bReusedMaterialSlot);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}

// =====================================================================
// add_landscape_material_layer_blend_input
//
// LIMITACION IMPORTANTE: esto SOLO agrega una entrada nueva a un nodo
// UMaterialExpressionLandscapeLayerBlend que YA EXISTE en el material.
// No crea el nodo desde cero ni lo conecta a Base Color -- eso requeriria
// manipular pines del material graph a mano, mucho mas fragil. Si el
// material no tiene ningun nodo LandscapeLayerBlend, este comando falla
// con un mensaje pidiendo agregarlo a mano en el Material Editor
// (conectado a Base Color) una sola vez -- despues de eso este comando
// puede seguir agregando layers nuevos sin volver a tocar el grafo.
//
// El layer nuevo se agrega con un color plano (ConstLayerInput), no una
// textura -- sirve para verificar que pinta bien, pero probablemente
// quieras reemplazarlo por una Texture Sample a mano despues.
//
// NOTE: los nombres de campos de FLayerBlendInput (LayerName, BlendType,
// ConstLayerInput, PreviewWeight) son de UE5.3. Si no compila, grepear
// "struct FLayerBlendInput" en MaterialExpressionLandscapeLayerBlend.h
// y ajustar los nombres que hayan cambiado.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleAddLandscapeMaterialLayerBlendInput(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString LayerName;
    if (!Params->TryGetStringField(TEXT("layer_name"), LayerName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'layer_name'"));
    }

    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    double R = 0.0, G = 1.0, B = 0.0; // verde por defecto (util para pasto)
    Params->TryGetNumberField(TEXT("color_r"), R);
    Params->TryGetNumberField(TEXT("color_g"), G);
    Params->TryGetNumberField(TEXT("color_b"), B);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found"));
    }

    UMaterialInterface* LandscapeMaterialInterface = FoundLandscape->GetLandscapeMaterial();
    if (!LandscapeMaterialInterface)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("El landscape no tiene un Landscape Material asignado"));
    }

    UMaterial* BaseMaterial = LandscapeMaterialInterface->GetMaterial();
    if (!BaseMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No se pudo resolver el UMaterial base del Landscape Material"));
    }

    UMaterialExpressionLandscapeLayerBlend* LayerBlendNode = nullptr;
    for (const TObjectPtr<UMaterialExpression>& Expression : BaseMaterial->GetExpressions())
    {
        LayerBlendNode = Cast<UMaterialExpressionLandscapeLayerBlend>(Expression.Get());
        if (LayerBlendNode)
        {
            break;
        }
    }

    if (!LayerBlendNode)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
            TEXT("El material '%s' no tiene ningun nodo LandscapeLayerBlend. Agregalo una vez a mano en el Material Editor (conectado a Base Color) y volve a llamar este comando."),
            *BaseMaterial->GetName()));
    }

    for (const FLayerBlendInput& ExistingLayer : LayerBlendNode->Layers)
    {
        if (ExistingLayer.LayerName == FName(*LayerName))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("El nodo LandscapeLayerBlend ya tiene una entrada '%s'"), *LayerName));
        }
    }

    FLayerBlendInput NewLayer;
    NewLayer.LayerName = FName(*LayerName);
    NewLayer.BlendType = LB_WeightBlend;
    NewLayer.ConstLayerInput = FVector4(R, G, B, 1.0);
    NewLayer.PreviewWeight = 0.0f;

    LayerBlendNode->Layers.Add(NewLayer);
    LayerBlendNode->Modify();
    BaseMaterial->Modify();
    BaseMaterial->PreEditChange(nullptr);
    BaseMaterial->PostEditChange();
    BaseMaterial->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_name"), BaseMaterial->GetName());
    ResultObj->SetStringField(TEXT("layer_name"), LayerName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}


// =====================================================================
// create_landscape_material_with_layer_blend
//
// Crea un Material nuevo con un nodo LandscapeLayerBlend conectado a
// Base Color, con una entrada FLayerBlendInput por cada nombre en
// 'layer_names', cada una con un color plano (Constant3Vector) como
// fallback antes de pintar. Opcionalmente lo asigna al landscape.
//
// NOTE: en UE 5.1+ las propiedades base del material (BaseColor, etc.)
// viven en Material->GetEditorOnlyData() en vez de directamente en el
// UMaterial. Si esto no compila, grepear "GetEditorOnlyData" en
// Material.h -- puede que en tu version siga siendo Material->BaseColor
// directamente.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleCreateLandscapeMaterialWithLayerBlend(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString MaterialName;
    if (!Params->TryGetStringField(TEXT("name"), MaterialName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name'"));
    }

    FString PackageFolder = TEXT("/Game/Landscape/Materials");
    Params->TryGetStringField(TEXT("path"), PackageFolder);

    const TArray<TSharedPtr<FJsonValue>>* LayerNamesJson = nullptr;
    if (!Params->TryGetArrayField(TEXT("layer_names"), LayerNamesJson) || LayerNamesJson->Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing or empty 'layer_names' (array de strings)"));
    }

    TArray<FString> LayerNames;
    for (const TSharedPtr<FJsonValue>& Value : *LayerNamesJson)
    {
        FString LayerNameStr;
        if (Value->TryGetString(LayerNameStr) && !LayerNameStr.IsEmpty())
        {
            LayerNames.Add(LayerNameStr);
        }
    }
    if (LayerNames.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'layer_names' no contiene ningun string valido"));
    }

    // Colores de fallback -- ciclan si hay mas capas que colores.
    static const FLinearColor DefaultColors[] = {
        FLinearColor(0.1f, 0.6f, 0.1f),  // verde (pasto)
        FLinearColor(0.4f, 0.3f, 0.2f),  // marron (tierra)
        FLinearColor(0.5f, 0.5f, 0.5f),  // gris (roca)
        FLinearColor(0.9f, 0.85f, 0.6f), // arena
    };
    const int32 NumDefaultColors = UE_ARRAY_COUNT(DefaultColors);

    const FString PackagePath = FString::Printf(TEXT("%s/%s"), *PackageFolder, *MaterialName);
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("No se pudo crear el package '%s'"), *PackagePath));
    }
    Package->FullyLoad();

    UMaterial* NewMaterial = NewObject<UMaterial>(Package, UMaterial::StaticClass(), FName(*MaterialName), RF_Public | RF_Standalone);
    if (!NewMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No se pudo crear el UMaterial"));
    }

    UMaterialExpressionLandscapeLayerBlend* LayerBlendExpr = NewObject<UMaterialExpressionLandscapeLayerBlend>(NewMaterial);
    NewMaterial->GetExpressionCollection().AddExpression(LayerBlendExpr);
    LayerBlendExpr->MaterialExpressionEditorX = -300;

    int32 LayerIndex = 0;
    for (const FString& LayerNameStr : LayerNames)
    {
        const FLinearColor& Color = DefaultColors[LayerIndex % NumDefaultColors];

        FLayerBlendInput Input;
        Input.LayerName = FName(*LayerNameStr);
        Input.BlendType = LB_WeightBlend;
        Input.PreviewWeight = 1.0f / LayerNames.Num();
        Input.ConstLayerInput = FVector(Color.R, Color.G, Color.B);
        LayerBlendExpr->Layers.Add(Input);

        ++LayerIndex;
    }

    NewMaterial->GetEditorOnlyData()->BaseColor.Expression = LayerBlendExpr;

    NewMaterial->PreEditChange(nullptr);
    NewMaterial->PostEditChange();

    FAssetRegistryModule::AssetCreated(NewMaterial);
    NewMaterial->MarkPackageDirty();

    const FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    const bool bSaved = UPackage::SavePackage(Package, NewMaterial, *PackageFileName, SaveArgs);
    if (!bSaved)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("El material se creo pero no se pudo guardar a disco en '%s'"), *PackageFileName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material_path"), PackagePath);
    ResultObj->SetNumberField(TEXT("num_layers"), LayerNames.Num());

    TArray<TSharedPtr<FJsonValue>> LayerNamesArr;
    for (const FString& LayerNameStr : LayerNames)
    {
        LayerNamesArr.Add(MakeShared<FJsonValueString>(LayerNameStr));
    }
    ResultObj->SetArrayField(TEXT("layer_names"), LayerNamesArr);

    bool bAssign = false;
    Params->TryGetBoolField(TEXT("assign_to_landscape"), bAssign);
    if (bAssign)
    {
        FString LandscapeName;
        Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

        UWorld* World = GEditor->GetEditorWorldContext().World();
        ALandscape* FoundLandscape = World ? FindLandscapeByName(World, LandscapeName) : nullptr;
        if (FoundLandscape)
        {
            FoundLandscape->LandscapeMaterial = NewMaterial;
            FoundLandscape->PostEditChange();
            FoundLandscape->MarkPackageDirty();
            ResultObj->SetBoolField(TEXT("assigned_to_landscape"), true);
            ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
        }
        else
        {
            ResultObj->SetBoolField(TEXT("assigned_to_landscape"), false);
            ResultObj->SetStringField(TEXT("assign_error"), TEXT("No matching Landscape actor found -- material creado pero no asignado"));
        }
    }
    else
    {
        ResultObj->SetBoolField(TEXT("assigned_to_landscape"), false);
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}

// =====================================================================
// set_landscape_material
//
// Asigna un Material Interface ya existente (por su asset path) como
// el Landscape Material del landscape. Distinto de set_actor_material
// (que es para StaticMeshActor) porque ALandscapeProxy expone
// LandscapeMaterial como propiedad propia, no como componente con slots.
//
// NOTE: PostEditChange() deberia disparar la actualizacion de las
// material instances de los componentes del landscape. Si el cambio no
// se ve reflejado visualmente, grepear "UpdateAllComponentMaterialInstances"
// en Landscape.h/LandscapeProxy.h y llamarlo explicitamente aca.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPLandscapeCommands::HandleSetLandscapeMaterial(const TSharedPtr<FJsonObject>& Params)
{
#if WITH_EDITOR
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'material_path'"));
    }

    FString LandscapeName;
    Params->TryGetStringField(TEXT("landscape_name"), LandscapeName);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No active editor world"));
    }

    ALandscape* FoundLandscape = FindLandscapeByName(World, LandscapeName);
    if (!FoundLandscape)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No matching Landscape actor found"));
    }

    UMaterialInterface* Material = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath));
    if (!Material)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("No se pudo cargar un UMaterialInterface en '%s'"), *MaterialPath));
    }

    FoundLandscape->LandscapeMaterial = Material;
    FoundLandscape->PostEditChange();
    FoundLandscape->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("landscape_name"), FoundLandscape->GetActorLabel());
    ResultObj->SetStringField(TEXT("material_path"), MaterialPath);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
#else
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Landscape editing requires WITH_EDITOR"));
#endif
}
