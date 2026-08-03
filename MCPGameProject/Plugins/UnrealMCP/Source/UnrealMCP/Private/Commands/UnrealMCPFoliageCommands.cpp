#include "Commands/UnrealMCPFoliageCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"
#include "InstancedFoliage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Dom/JsonValue.h"

UFoliageType* FUnrealMCPFoliageCommands::LoadFoliageTypeAsset(const FString& FoliageTypePath, FString& OutError)
{
    UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
    if (!FoliageType)
    {
        OutError = FString::Printf(TEXT("Foliage type not found: %s"), *FoliageTypePath);
        return nullptr;
    }
    return FoliageType;
}

AInstancedFoliageActor* FUnrealMCPFoliageCommands::GetOrCreateFoliageActor(FString& OutError)
{
    if (!GEditor)
    {
        OutError = TEXT("No editor context available");
        return nullptr;
    }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        OutError = TEXT("No active editor world");
        return nullptr;
    }
    AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
    if (!IFA)
    {
        OutError = TEXT("Failed to get or create InstancedFoliageActor for current level");
        return nullptr;
    }
    return IFA;
}

TSharedPtr<FJsonObject> FUnrealMCPFoliageCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_foliage_type"))
        return HandleCreateFoliageType(Params);
    if (CommandType == TEXT("add_foliage_instances"))
        return HandleAddFoliageInstances(Params);
    if (CommandType == TEXT("remove_foliage_instances"))
        return HandleRemoveFoliageInstances(Params);
    if (CommandType == TEXT("list_foliage_types"))
        return HandleListFoliageTypes(Params);

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown foliage command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPFoliageCommands::HandleCreateFoliageType(const TSharedPtr<FJsonObject>& Params)
{
    FString MeshPath;
    if (!Params->TryGetStringField(TEXT("mesh_path"), MeshPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'mesh_path' parameter"));
    }
    FString FoliageTypeName;
    if (!Params->TryGetStringField(TEXT("foliage_type_name"), FoliageTypeName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'foliage_type_name' parameter"));
    }
    FString FolderPath = TEXT("/Game/Foliage");
    Params->TryGetStringField(TEXT("folder_path"), FolderPath);

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!Mesh)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath));
    }

    FString PackageName = FolderPath / FoliageTypeName;
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for foliage type"));
    }

    UFoliageType_InstancedStaticMesh* NewFoliageType = NewObject<UFoliageType_InstancedStaticMesh>(
        Package, FName(*FoliageTypeName), RF_Standalone | RF_Public);
    if (!NewFoliageType)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create foliage type asset"));
    }

    NewFoliageType->SetStaticMesh(Mesh);

    double Density = 100.0;
    if (Params->TryGetNumberField(TEXT("density"), Density))
    {
        NewFoliageType->Density = (float)Density;
    }

    double MinScale = 1.0, MaxScale = 1.0;
    bool bHasMinScale = Params->TryGetNumberField(TEXT("min_scale"), MinScale);
    bool bHasMaxScale = Params->TryGetNumberField(TEXT("max_scale"), MaxScale);
    if (bHasMinScale || bHasMaxScale)
    {
        NewFoliageType->ScaleX.Min = (float)MinScale;
        NewFoliageType->ScaleX.Max = (float)MaxScale;
        NewFoliageType->ScaleY.Min = (float)MinScale;
        NewFoliageType->ScaleY.Max = (float)MaxScale;
        NewFoliageType->ScaleZ.Min = (float)MinScale;
        NewFoliageType->ScaleZ.Max = (float)MaxScale;
    }

    bool bAlignToNormal = true;
    if (Params->TryGetBoolField(TEXT("align_to_normal"), bAlignToNormal))
    {
        NewFoliageType->AlignToNormal = bAlignToNormal;
    }

    bool bRandomYaw = true;
    if (Params->TryGetBoolField(TEXT("random_yaw"), bRandomYaw))
    {
        NewFoliageType->RandomYaw = bRandomYaw;
    }

    NewFoliageType->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewFoliageType);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewFoliageType, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("foliage_type_path"), PackageName);
    Result->SetBoolField(TEXT("saved"), bSaved);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPFoliageCommands::HandleAddFoliageInstances(const TSharedPtr<FJsonObject>& Params)
{
    FString FoliageTypePath;
    if (!Params->TryGetStringField(TEXT("foliage_type_path"), FoliageTypePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'foliage_type_path' parameter"));
    }

    FString Err;
    UFoliageType* FoliageType = LoadFoliageTypeAsset(FoliageTypePath, Err);
    if (!FoliageType)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Err);
    }

    AInstancedFoliageActor* IFA = GetOrCreateFoliageActor(Err);
    if (!IFA)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Err);
    }

    const TArray<TSharedPtr<FJsonValue>>* InstancesArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("instances"), InstancesArray) || !InstancesArray)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'instances' array parameter (each item: {location:[x,y,z], rotation:[pitch,yaw,roll] (optional), scale:[x,y,z] or number (optional)})"));
    }

    FFoliageInfo* Info = IFA->FindOrAddMesh(FoliageType);
    if (!Info)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to find or add foliage mesh info"));
    }

    int32 AddedCount = 0;
    for (const TSharedPtr<FJsonValue>& Item : *InstancesArray)
    {
        const TSharedPtr<FJsonObject>* ItemObj;
        if (!Item->TryGetObject(ItemObj))
        {
            continue;
        }

        const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
        if (!(*ItemObj)->TryGetArrayField(TEXT("location"), LocArr) || !LocArr || LocArr->Num() != 3)
        {
            continue;
        }
        FVector Location((*LocArr)[0]->AsNumber(), (*LocArr)[1]->AsNumber(), (*LocArr)[2]->AsNumber());

        FRotator Rotation = FRotator::ZeroRotator;
        const TArray<TSharedPtr<FJsonValue>>* RotArr = nullptr;
        if ((*ItemObj)->TryGetArrayField(TEXT("rotation"), RotArr) && RotArr && RotArr->Num() == 3)
        {
            Rotation = FRotator((*RotArr)[0]->AsNumber(), (*RotArr)[1]->AsNumber(), (*RotArr)[2]->AsNumber());
        }

        FVector3f Scale3D(1.0f, 1.0f, 1.0f);
        double UniformScale;
        const TArray<TSharedPtr<FJsonValue>>* ScaleArr = nullptr;
        if ((*ItemObj)->TryGetArrayField(TEXT("scale"), ScaleArr) && ScaleArr && ScaleArr->Num() == 3)
        {
            Scale3D = FVector3f((*ScaleArr)[0]->AsNumber(), (*ScaleArr)[1]->AsNumber(), (*ScaleArr)[2]->AsNumber());
        }
        else if ((*ItemObj)->TryGetNumberField(TEXT("scale"), UniformScale))
        {
            Scale3D = FVector3f((float)UniformScale, (float)UniformScale, (float)UniformScale);
        }

        FFoliageInstance NewInstance;
        NewInstance.Location = Location;
        NewInstance.Rotation = Rotation;
        NewInstance.DrawScale3D = Scale3D;

        Info->AddInstance(FoliageType, NewInstance);
        AddedCount++;
    }

    Info->PostUpdateInstances(TArrayView<const int32>(), false, false);
    IFA->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("added_count"), AddedCount);
    Result->SetNumberField(TEXT("total_instance_count"), Info->Instances.Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPFoliageCommands::HandleRemoveFoliageInstances(const TSharedPtr<FJsonObject>& Params)
{
    FString FoliageTypePath;
    if (!Params->TryGetStringField(TEXT("foliage_type_path"), FoliageTypePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'foliage_type_path' parameter"));
    }

    FString Err;
    UFoliageType* FoliageType = LoadFoliageTypeAsset(FoliageTypePath, Err);
    if (!FoliageType)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Err);
    }

    AInstancedFoliageActor* IFA = GetOrCreateFoliageActor(Err);
    if (!IFA)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Err);
    }

    FFoliageInfo* Info = IFA->FindInfo(FoliageType);
    if (!Info)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No instances found for this foliage type in the current level"));
    }

    const TArray<TSharedPtr<FJsonValue>>* IndicesArray = nullptr;
    if (!Params->TryGetArrayField(TEXT("indices"), IndicesArray) || !IndicesArray)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'indices' array parameter"));
    }

    TArray<int32> Indices;
    for (const TSharedPtr<FJsonValue>& V : *IndicesArray)
    {
        Indices.Add((int32)V->AsNumber());
    }

    Info->RemoveInstances(Indices, true);
    IFA->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetNumberField(TEXT("removed_count"), Indices.Num());
    Result->SetNumberField(TEXT("remaining_instance_count"), Info->Instances.Num());
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPFoliageCommands::HandleListFoliageTypes(const TSharedPtr<FJsonObject>& Params)
{
    FString Err;
    AInstancedFoliageActor* IFA = GetOrCreateFoliageActor(Err);
    if (!IFA)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(Err);
    }

    TArray<TSharedPtr<FJsonValue>> TypesArray;
    for (const auto& Pair : IFA->GetFoliageInfos())
    {
        UFoliageType* FoliageType = Pair.Key;
        const FFoliageInfo& Info = Pair.Value.Get();

        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("foliage_type_path"), FoliageType ? FoliageType->GetPathName() : TEXT(""));
        Entry->SetNumberField(TEXT("instance_count"), Info.Instances.Num());
        TypesArray.Add(MakeShared<FJsonValueObject>(Entry));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("foliage_types"), TypesArray);
    return Result;
}
