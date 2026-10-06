#include "Commands/UnrealMCPAssetCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/Material.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

FUnrealMCPAssetCommands::FUnrealMCPAssetCommands()
{
}

namespace
{
    // Valida que 'Name' sirva como nombre de asset/paquete.
    bool LMCP_IsValidAssetName(const FString& Name)
    {
        if (Name.IsEmpty())
        {
            return false;
        }
        for (TCHAR C : Name)
        {
            if (C == TEXT('/') || C == TEXT('\\') || C == TEXT('.') || FChar::IsWhitespace(C))
            {
                return false;
            }
        }
        return true;
    }

    // Resuelve una clase por: ruta completa (/Script/Engine.Actor), nombre
    // corto nativo (Actor, Material, StaticMesh) o asset Blueprint (_C).
    UClass* LMCP_ResolveClass(const FString& Target)
    {
        if (Target.IsEmpty())
        {
            return nullptr;
        }
        if (Target.StartsWith(TEXT("/Script/")))
        {
            if (UClass* Cls = LoadClass<UObject>(nullptr, *Target))
            {
                return Cls;
            }
        }
        // Nombre corto nativo, ej. "Material" -> /Script/Engine.Material
        if (UClass* Cls = LoadClass<UObject>(nullptr, *(TEXT("/Script/Engine.") + Target)))
        {
            return Cls;
        }
        if (UObject* Obj = FindObject<UObject>(nullptr, *Target))
        {
            if (UClass* AsCls = Cast<UClass>(Obj))
            {
                return AsCls;
            }
            if (UBlueprint* BP = Cast<UBlueprint>(Obj))
            {
                return BP->GeneratedClass;
            }
        }
        // Clase generada de un Blueprint: /Game/.../MiBP_MiBP_C
        if (UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *Target))
        {
            if (UClass* GenCls = Cast<UClass>(Obj))
            {
                return GenCls;
            }
        }
        return nullptr;
    }
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("import_asset"))
    {
        return HandleImportAsset(Params);
    }
    else if (CommandType == TEXT("find_assets"))
    {
        return HandleFindAssets(Params);
    }
    else if (CommandType == TEXT("create_asset"))
    {
        return HandleCreateAsset(Params);
    }
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown asset command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleImportAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString SourcePath;
    if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) || SourcePath.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'source_path' parameter"));
    }
    if (!FPaths::FileExists(SourcePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Archivo no encontrado: %s"), *SourcePath));
    }

    FString DestPath = TEXT("/Game/Imported");
    Params->TryGetStringField(TEXT("path"), DestPath);
    DestPath.TrimStartAndEndInline();
    if (DestPath.IsEmpty())
    {
        DestPath = TEXT("/Game/Imported");
    }

    FString DestName = FPaths::GetBaseFilename(SourcePath);
    Params->TryGetStringField(TEXT("name"), DestName);
    if (!LMCP_IsValidAssetName(DestName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'name' invalido (sin espacios, puntos ni barras)"));
    }

    bool bReplace = true;
    Params->TryGetBoolField(TEXT("replace"), bReplace);
    bool bSave = true;
    Params->TryGetBoolField(TEXT("save"), bSave);

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    UAssetImportTask* ImportTask = NewObject<UAssetImportTask>();
    ImportTask->Filename = SourcePath;
    ImportTask->DestinationPath = DestPath;
    ImportTask->DestinationName = DestName;
    ImportTask->bAutomated = true;
    ImportTask->bSave = bSave;
    ImportTask->bReplaceExisting = bReplace;

    TArray<UAssetImportTask*> Tasks = { ImportTask };
    AssetTools.ImportAssetTasks(Tasks);

    TArray<UObject*> Imported = ImportTask->GetObjects();
    if (Imported.Num() == 0)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Fallo al importar '%s' -- formato no soportado (sin factory) o archivo invalido"), *SourcePath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("source_path"), SourcePath);
    ResultObj->SetStringField(TEXT("destination"), DestPath);
    ResultObj->SetNumberField(TEXT("imported_count"), Imported.Num());

    TArray<TSharedPtr<FJsonValue>> AssetsArr;
    for (UObject* Obj : Imported)
    {
        if (!Obj)
        {
            continue;
        }
        TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
        A->SetStringField(TEXT("path"), Obj->GetPathName());
        A->SetStringField(TEXT("name"), Obj->GetName());
        A->SetStringField(TEXT("class"), Obj->GetClass()->GetName());
        AssetsArr.Add(MakeShared<FJsonValueObject>(A));
    }
    ResultObj->SetArrayField(TEXT("assets"), AssetsArr);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleFindAssets(const TSharedPtr<FJsonObject>& Params)
{
    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& Registry = ARM.Get();

    FString SearchName;
    Params->TryGetStringField(TEXT("name"), SearchName);

    FString ClassName;
    Params->TryGetStringField(TEXT("class"), ClassName);

    FString PackagePath = TEXT("/Game");
    Params->TryGetStringField(TEXT("path"), PackagePath);
    PackagePath.TrimStartAndEndInline();
    if (PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/Game");
    }

    bool bRecursive = true;
    Params->TryGetBoolField(TEXT("recursive"), bRecursive);

    int32 Limit = 50;
    Params->TryGetNumberField(TEXT("limit"), Limit);
    if (Limit <= 0)
    {
        Limit = 50;
    }

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*PackagePath));
    Filter.bRecursivePaths = bRecursive;
    Filter.bIncludeOnlyOnDiskAssets = false;

    if (!ClassName.IsEmpty())
    {
        UClass* Cls = LMCP_ResolveClass(ClassName);
        if (!Cls)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Clase no encontrada: %s (usa '/Script/Engine.Material' o nombre corto como 'Material')"), *ClassName));
        }
        Filter.ClassPaths.Add(Cls->GetClassPathName());
        Filter.bRecursiveClasses = true;
    }

    TArray<FAssetData> Found;
    if (!Registry.GetAssets(Filter, Found))
    {
        // GetAssets devuelve false si el filtro es invalido; tratamos como vacio.
        Found.Reset();
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> AssetsArr;
    int32 Total = 0;
    for (const FAssetData& A : Found)
    {
        if (!SearchName.IsEmpty() && !A.AssetName.ToString().Contains(SearchName, ESearchCase::IgnoreCase))
        {
            continue;
        }
        Total++;
        if (AssetsArr.Num() < Limit)
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("name"), A.AssetName.ToString());
            Obj->SetStringField(TEXT("path"), A.PackageName.ToString());
            Obj->SetStringField(TEXT("object"), FString::Printf(TEXT("%s.%s"), *A.PackageName.ToString(), *A.AssetName.ToString()));
            Obj->SetStringField(TEXT("class"), A.AssetClassPath.ToString().Contains(TEXT("."))
                ? A.AssetClassPath.ToString().RightChop(A.AssetClassPath.ToString().Find(TEXT(".")) + 1)
                : A.AssetClassPath.ToString());
            AssetsArr.Add(MakeShared<FJsonValueObject>(Obj));
        }
    }

    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("path"), PackagePath);
    if (!ClassName.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("class_filter"), ClassName);
    }
    if (!SearchName.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("name_filter"), SearchName);
    }
    ResultObj->SetNumberField(TEXT("total"), Total);
    ResultObj->SetNumberField(TEXT("returned"), AssetsArr.Num());
    ResultObj->SetArrayField(TEXT("assets"), AssetsArr);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPAssetCommands::HandleCreateAsset(const TSharedPtr<FJsonObject>& Params)
{
    FString Type;
    if (!Params->TryGetStringField(TEXT("type"), Type) || Type.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter (material|blueprint)"));
    }
    Type.ToLowerInline();

    FString AssetName;
    if (!Params->TryGetStringField(TEXT("name"), AssetName) || AssetName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }
    if (!LMCP_IsValidAssetName(AssetName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'name' invalido (sin espacios, puntos ni barras)"));
    }

    FString FolderPath = TEXT("/Game");
    Params->TryGetStringField(TEXT("path"), FolderPath);
    FolderPath.TrimStartAndEndInline();
    if (FolderPath.IsEmpty())
    {
        FolderPath = TEXT("/Game");
    }

    FString PackageName = UPackageTools::SanitizePackageName(FolderPath / AssetName);
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("No se pudo crear el paquete: %s"), *PackageName));
    }
    if (FindObject<UObject>(Package, *AssetName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("El asset ya existe: %s"), *PackageName));
    }

    UObject* NewAsset = nullptr;

    if (Type == TEXT("material"))
    {
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        NewAsset = Factory->FactoryCreateNew(UMaterial::StaticClass(), Package, FName(*AssetName),
            RF_Public | RF_Standalone, nullptr, GWarn);
    }
    else if (Type == TEXT("blueprint"))
    {
        FString ParentTarget = TEXT("/Script/Engine.Actor");
        Params->TryGetStringField(TEXT("parent_class"), ParentTarget);
        UClass* ParentClass = LMCP_ResolveClass(ParentTarget);
        if (!ParentClass)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("parent_class no encontrado: %s"), *ParentTarget));
        }
        NewAsset = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*AssetName),
            BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Tipo no soportado: %s (material|blueprint)"), *Type));
    }

    if (!NewAsset)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Fallo al crear el asset '%s' (%s)"), *AssetName, *Type));
    }

    FAssetRegistryModule::AssetCreated(NewAsset);
    Package->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("type"), Type);
    ResultObj->SetStringField(TEXT("path"), NewAsset->GetPathName());
    ResultObj->SetStringField(TEXT("package"), PackageName);
    return ResultObj;
}
