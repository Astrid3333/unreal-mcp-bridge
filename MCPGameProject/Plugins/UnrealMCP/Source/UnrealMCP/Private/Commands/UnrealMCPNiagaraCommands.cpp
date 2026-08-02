#include "Commands/UnrealMCPNiagaraCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

// Niagara-specific includes
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraEmitter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PackageTools.h"

FUnrealMCPNiagaraCommands::FUnrealMCPNiagaraCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("spawn_niagara_system"))
    {
        return HandleSpawnNiagaraSystem(Params);
    }
    else if (CommandType == TEXT("set_niagara_float_parameter"))
    {
        return HandleSetNiagaraFloatParameter(Params);
    }
    else if (CommandType == TEXT("set_niagara_vector_parameter"))
    {
        return HandleSetNiagaraVectorParameter(Params);
    }
    else if (CommandType == TEXT("set_niagara_color_parameter"))
    {
        return HandleSetNiagaraColorParameter(Params);
    }
    else if (CommandType == TEXT("activate_niagara_component"))
    {
        return HandleActivateNiagaraComponent(Params);
    }
    else if (CommandType == TEXT("deactivate_niagara_component"))
    {
        return HandleDeactivateNiagaraComponent(Params);
    }
    else if (CommandType == TEXT("add_niagara_user_parameter"))
    {
        return HandleAddNiagaraUserParameter(Params);
    }
    else if (CommandType == TEXT("list_niagara_user_parameters"))
    {
        return HandleListNiagaraUserParameters(Params);
    }
    else if (CommandType == TEXT("create_niagara_emitter"))
    {
        return HandleCreateNiagaraEmitter(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown niagara command: %s"), *CommandType));
}

// Shared helper: find a spawned ANiagaraActor by actor name.
// Mirrors the lookup pattern used in set_ambient_sound_properties.
static ANiagaraActor* FindNiagaraActorByName(const FString& ActorName)
{
    ANiagaraActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Cast<ANiagaraActor>(Actor);
            break;
        }
    }
    return TargetActor;
}

// =====================================================================
// spawn_niagara_system
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleSpawnNiagaraSystem(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString SystemPath;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("NiagaraSystem not found: %s"), *SystemPath));
    }

    FVector Location(0.0f, 0.0f, 0.0f);
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    FVector Scale(1.0f, 1.0f, 1.0f);
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Same "already exists" guard used by spawn_actor / spawn_ambient_sound
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    ANiagaraActor* NewActor = World->SpawnActor<ANiagaraActor>(ANiagaraActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!NewActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn NiagaraActor"));
    }
    NewActor->SetActorScale3D(Scale);

    UNiagaraComponent* NiagaraComp = NewActor->GetNiagaraComponent();
    if (!NiagaraComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NiagaraActor has no NiagaraComponent"));
    }

    NiagaraComp->SetAsset(System);

    bool bAutoActivate = true;
    Params->TryGetBoolField(TEXT("auto_activate"), bAutoActivate);
    NiagaraComp->bAutoActivate = bAutoActivate;
    if (bAutoActivate)
    {
        NiagaraComp->Activate(true);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), NewActor->GetName());
    ResultObj->SetStringField(TEXT("system_path"), SystemPath);
    ResultObj->SetBoolField(TEXT("auto_activate"), bAutoActivate);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// set_niagara_float_parameter
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleSetNiagaraFloatParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }
    double Value = 0.0;
    if (!Params->TryGetNumberField(TEXT("value"), Value))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }

    ANiagaraActor* TargetActor = FindNiagaraActorByName(ActorName);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("NiagaraActor not found: %s"), *ActorName));
    }
    UNiagaraComponent* NiagaraComp = TargetActor->GetNiagaraComponent();
    if (!NiagaraComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NiagaraActor has no NiagaraComponent"));
    }

    // NOTE: this sets a User (exposed) parameter on the component instance, the
    // same call the Details panel makes when you edit a system's user parameters
    // on a per-instance basis. The parameter must exist on the system as a User
    // Exposed Float, otherwise this is a silent no-op (confirmed against UE5.3
    // NiagaraComponent.h; if your checkout renamed this API the compiler error
    // will point straight at it).
    NiagaraComp->SetVariableFloat(FName(*ParameterName), static_cast<float>(Value));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    ResultObj->SetNumberField(TEXT("value"), Value);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// set_niagara_vector_parameter
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleSetNiagaraVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    double X = 0.0, Y = 0.0, Z = 0.0;
    Params->TryGetNumberField(TEXT("x"), X);
    Params->TryGetNumberField(TEXT("y"), Y);
    Params->TryGetNumberField(TEXT("z"), Z);

    ANiagaraActor* TargetActor = FindNiagaraActorByName(ActorName);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("NiagaraActor not found: %s"), *ActorName));
    }
    UNiagaraComponent* NiagaraComp = TargetActor->GetNiagaraComponent();
    if (!NiagaraComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NiagaraActor has no NiagaraComponent"));
    }

    NiagaraComp->SetVariableVec3(FName(*ParameterName), FVector(X, Y, Z));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    ResultObj->SetNumberField(TEXT("x"), X);
    ResultObj->SetNumberField(TEXT("y"), Y);
    ResultObj->SetNumberField(TEXT("z"), Z);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// set_niagara_color_parameter
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleSetNiagaraColorParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
    Params->TryGetNumberField(TEXT("r"), R);
    Params->TryGetNumberField(TEXT("g"), G);
    Params->TryGetNumberField(TEXT("b"), B);
    Params->TryGetNumberField(TEXT("a"), A);

    ANiagaraActor* TargetActor = FindNiagaraActorByName(ActorName);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("NiagaraActor not found: %s"), *ActorName));
    }
    UNiagaraComponent* NiagaraComp = TargetActor->GetNiagaraComponent();
    if (!NiagaraComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NiagaraActor has no NiagaraComponent"));
    }

    NiagaraComp->SetVariableLinearColor(FName(*ParameterName), FLinearColor(R, G, B, A));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetStringField(TEXT("parameter_name"), ParameterName);
    ResultObj->SetNumberField(TEXT("r"), R);
    ResultObj->SetNumberField(TEXT("g"), G);
    ResultObj->SetNumberField(TEXT("b"), B);
    ResultObj->SetNumberField(TEXT("a"), A);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// activate_niagara_component
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleActivateNiagaraComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    bool bReset = true;
    Params->TryGetBoolField(TEXT("reset"), bReset);

    ANiagaraActor* TargetActor = FindNiagaraActorByName(ActorName);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("NiagaraActor not found: %s"), *ActorName));
    }
    UNiagaraComponent* NiagaraComp = TargetActor->GetNiagaraComponent();
    if (!NiagaraComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NiagaraActor has no NiagaraComponent"));
    }

    NiagaraComp->Activate(bReset);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetBoolField(TEXT("reset"), bReset);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// deactivate_niagara_component
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleDeactivateNiagaraComponent(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    ANiagaraActor* TargetActor = FindNiagaraActorByName(ActorName);
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("NiagaraActor not found: %s"), *ActorName));
    }
    UNiagaraComponent* NiagaraComp = TargetActor->GetNiagaraComponent();
    if (!NiagaraComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("NiagaraActor has no NiagaraComponent"));
    }

    NiagaraComp->Deactivate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor_name"), ActorName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// add_niagara_user_parameter -- adds a User-exposed parameter to a
// NiagaraSystem ASSET. Mirrors clicking "+" under "User Exposed" in the
// System's Parameters panel.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleAddNiagaraUserParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
    }

    FString ParameterName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }

    FString ParameterType = TEXT("float");
    Params->TryGetStringField(TEXT("parameter_type"), ParameterType);

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("NiagaraSystem not found: %s"), *SystemPath));
    }

    FNiagaraTypeDefinition TypeDef;
    if (ParameterType == TEXT("vector"))
    {
        TypeDef = FNiagaraTypeDefinition::GetVec3Def();
    }
    else if (ParameterType == TEXT("color"))
    {
        TypeDef = FNiagaraTypeDefinition::GetColorDef();
    }
    else
    {
        TypeDef = FNiagaraTypeDefinition::GetFloatDef();
    }

    FString FullName = ParameterName.StartsWith(TEXT("User.")) ? ParameterName : (TEXT("User.") + ParameterName);
    FNiagaraVariable NewVar(TypeDef, FName(*FullName));

    FNiagaraUserRedirectionParameterStore& ExposedParams = System->GetExposedParameters();

    TArray<FNiagaraVariable> ExistingVars;
    ExposedParams.GetParameters(ExistingVars);
    for (const FNiagaraVariable& V : ExistingVars)
    {
        if (V.GetName() == NewVar.GetName())
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parameter '%s' already exists on this system"), *FullName));
        }
    }

    ExposedParams.AddParameter(NewVar);
    System->PostEditChange();
    System->MarkPackageDirty();

    UPackage* Package = System->GetOutermost();
    FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, System, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("system_path"), SystemPath);
    ResultObj->SetStringField(TEXT("parameter_name"), FullName);
    ResultObj->SetStringField(TEXT("parameter_type"), ParameterType);
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// list_niagara_user_parameters -- reads back what's exposed on a
// NiagaraSystem asset.
// =====================================================================
TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleListNiagaraUserParameters(const TSharedPtr<FJsonObject>& Params)
{
    FString SystemPath;
    if (!Params->TryGetStringField(TEXT("system_path"), SystemPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'system_path' parameter"));
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!System)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("NiagaraSystem not found: %s"), *SystemPath));
    }

    FNiagaraUserRedirectionParameterStore& ExposedParams = System->GetExposedParameters();
    TArray<FNiagaraVariable> Variables;
    ExposedParams.GetParameters(Variables);

    TArray<TSharedPtr<FJsonValue>> ParamArray;
    for (const FNiagaraVariable& Var : Variables)
    {
        FString TypeStr = TEXT("other");
        if (Var.GetType() == FNiagaraTypeDefinition::GetFloatDef()) TypeStr = TEXT("float");
        else if (Var.GetType() == FNiagaraTypeDefinition::GetVec3Def()) TypeStr = TEXT("vector");
        else if (Var.GetType() == FNiagaraTypeDefinition::GetColorDef()) TypeStr = TEXT("color");

        TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
        ParamObj->SetStringField(TEXT("name"), Var.GetName().ToString());
        ParamObj->SetStringField(TEXT("type"), TypeStr);
        ParamArray.Add(MakeShared<FJsonValueObject>(ParamObj));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("system_path"), SystemPath);
    ResultObj->SetArrayField(TEXT("parameters"), ParamArray);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}


TSharedPtr<FJsonObject> FUnrealMCPNiagaraCommands::HandleCreateNiagaraEmitter(const TSharedPtr<FJsonObject>& Params)
{
    FString EmitterName;
    if (!Params->TryGetStringField(TEXT("name"), EmitterName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    FString FolderPath = TEXT("/Game/FX/Emitters");
    Params->TryGetStringField(TEXT("path"), FolderPath);

    bool bAddDefaultModules = true;
    Params->TryGetBoolField(TEXT("add_default_modules"), bAddDefaultModules);

    FString PackageName = FolderPath / EmitterName;
    PackageName = UPackageTools::SanitizePackageName(PackageName);

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create package for emitter"));
    }

    UNiagaraEmitterFactoryNew* Factory = NewObject<UNiagaraEmitterFactoryNew>();

    // OJO: NO llamamos a Factory->ConfigureProperties() -- esa funcion abre
    // SNewEmitterDialog, un modal de Slate que bloquea esperando input humano
    // en el editor. Como esto corre headless via MCP, saltarlo es correcto:
    // el constructor de UNiagaraEmitterFactoryNew ya deja EmitterToCopy=nullptr
    // y bUseInheritance=false, que es exactamente la rama "crear emitter vacio"
    // de FactoryCreateNew. Solo pisamos el flag que si queremos controlar:
    Factory->EmitterToCopy = nullptr;
    Factory->bUseInheritance = false;
    Factory->bAddDefaultModulesAndRenderersToEmptyEmitter = bAddDefaultModules;

    UNiagaraEmitter* NewEmitter = Cast<UNiagaraEmitter>(Factory->FactoryCreateNew(
        UNiagaraEmitter::StaticClass(), Package, FName(*EmitterName), RF_Standalone | RF_Public, nullptr, GWarn));

    if (!NewEmitter)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create Niagara emitter asset"));
    }

    NewEmitter->PreEditChange(nullptr);
    NewEmitter->PostEditChange();
    NewEmitter->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewEmitter);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Package, NewEmitter, *PackageFileName, SaveArgs);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("emitter_path"), NewEmitter->GetPathName());
    ResultObj->SetBoolField(TEXT("saved_to_disk"), bSaved);
    ResultObj->SetBoolField(TEXT("default_modules_added"), bAddDefaultModules);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}
