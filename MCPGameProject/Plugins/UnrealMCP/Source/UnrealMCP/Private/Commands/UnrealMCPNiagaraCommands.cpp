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
