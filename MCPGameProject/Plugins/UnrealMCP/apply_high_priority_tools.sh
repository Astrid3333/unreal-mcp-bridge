#!/usr/bin/env bash
# Ejecutar desde: ~/unreal-mcp/MCPGameProject/Plugins/UnrealMCP
set -euo pipefail

H="Source/UnrealMCP/Public/Commands/UnrealMCPEditorCommands.h"
CPP="Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp"

for f in "$H" "$CPP"; do
    if [ ! -f "$f" ]; then
        echo "ERROR: no encuentro $f — ¿estás parado en ~/unreal-mcp/MCPGameProject/Plugins/UnrealMCP?"
        exit 1
    fi
done

# Backups
cp "$H" "$H.bak"
cp "$CPP" "$CPP.bak"
echo "Backups creados: $H.bak / $CPP.bak"

# ---------------------------------------------------------------
# 1) Header: agregar declaraciones de los nuevos Handle*
# ---------------------------------------------------------------
python3 << 'PYEOF'
path = "Source/UnrealMCP/Public/Commands/UnrealMCPEditorCommands.h"
with open(path) as f:
    content = f.read()

anchor = "    TSharedPtr<FJsonObject> HandleSetActorMaterial(const TSharedPtr<FJsonObject>& Params);"
assert content.count(anchor) == 1, "Anchor de header no encontrado o duplicado — revisar manualmente."

insertion = anchor + """

    // --- Alta prioridad: materiales ---
    TSharedPtr<FJsonObject> HandleGetActorMaterial(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCreateDynamicMaterialInstance(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialScalarParameter(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetMaterialVectorParameter(const TSharedPtr<FJsonObject>& Params);

    // --- Alta prioridad: actores ---
    TSharedPtr<FJsonObject> HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetActorBounds(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAttachActorToActor(const TSharedPtr<FJsonObject>& Params);"""

content = content.replace(anchor, insertion)
with open(path, "w") as f:
    f.write(content)
print("OK: header actualizado")
PYEOF

# ---------------------------------------------------------------
# 2) .cpp: includes necesarios
# ---------------------------------------------------------------
python3 << 'PYEOF'
path = "Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp"
with open(path) as f:
    content = f.read()

anchor = '#include "Commands/UnrealMCPEditorCommands.h"'
assert anchor in content, "No encontré el include propio del .cpp — revisar manualmente."

new_includes = anchor + """
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h\""""

if "Materials/MaterialInstanceDynamic.h" not in content:
    content = content.replace(anchor, new_includes, 1)
    with open(path, "w") as f:
        f.write(content)
    print("OK: includes agregados")
else:
    print("SKIP: includes ya estaban")
PYEOF

# ---------------------------------------------------------------
# 3) .cpp: dispatcher (HandleCommand) — agregar los nuevos "else if"
# ---------------------------------------------------------------
python3 << 'PYEOF'
path = "Source/UnrealMCP/Private/Commands/UnrealMCPEditorCommands.cpp"
with open(path) as f:
    content = f.read()

anchor = """    else if (CommandType == TEXT("set_actor_material"))
    {
        return HandleSetActorMaterial(Params);
    }"""
assert content.count(anchor) == 1, "Anchor de dispatcher no encontrado o duplicado."

insertion = anchor + """
    else if (CommandType == TEXT("get_actor_material"))
    {
        return HandleGetActorMaterial(Params);
    }
    else if (CommandType == TEXT("create_dynamic_material_instance"))
    {
        return HandleCreateDynamicMaterialInstance(Params);
    }
    else if (CommandType == TEXT("set_material_scalar_parameter"))
    {
        return HandleSetMaterialScalarParameter(Params);
    }
    else if (CommandType == TEXT("set_material_vector_parameter"))
    {
        return HandleSetMaterialVectorParameter(Params);
    }
    else if (CommandType == TEXT("duplicate_actor"))
    {
        return HandleDuplicateActor(Params);
    }
    else if (CommandType == TEXT("get_actor_bounds"))
    {
        return HandleGetActorBounds(Params);
    }
    else if (CommandType == TEXT("attach_actor_to_actor"))
    {
        return HandleAttachActorToActor(Params);
    }"""

content = content.replace(anchor, insertion)
with open(path, "w") as f:
    f.write(content)
print("OK: dispatcher actualizado")
PYEOF

# ---------------------------------------------------------------
# 4) .cpp: implementaciones — se agregan al final del archivo
# ---------------------------------------------------------------
cat >> "$CPP" << 'CPPEOF'

// =====================================================================
// Alta prioridad — Materiales
// =====================================================================

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorMaterial(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    UMaterialInterface* Material = MeshComp->GetMaterial(SlotIndex);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    ResultObj->SetStringField(TEXT("material_path"), Material ? Material->GetPathName() : TEXT(""));
    ResultObj->SetBoolField(TEXT("is_dynamic_instance"), Material ? Material->IsA<UMaterialInstanceDynamic>() : false);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCreateDynamicMaterialInstance(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    UMaterialInstanceDynamic* DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(SlotIndex);
    if (!DynMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create dynamic material instance (check the slot has a valid parent material)"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetNumberField(TEXT("slot_index"), SlotIndex);
    ResultObj->SetStringField(TEXT("dynamic_instance_name"), DynMaterial->GetName());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetMaterialScalarParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString ParamName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }
    double Value = 0.0;
    if (!Params->TryGetNumberField(TEXT("value"), Value))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    // Si el slot todavía no es una instancia dinámica, la promovemos automáticamente
    // asi el tool funciona directo sobre un actor recien spawneado sin pasos previos.
    UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(SlotIndex));
    if (!DynMaterial)
    {
        DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(SlotIndex);
    }
    if (!DynMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get or create a dynamic material instance for this slot"));
    }

    DynMaterial->SetScalarParameterValue(FName(*ParamName), static_cast<float>(Value));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("parameter"), ParamName);
    ResultObj->SetNumberField(TEXT("value"), Value);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetMaterialVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString ParamName;
    if (!Params->TryGetStringField(TEXT("parameter_name"), ParamName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parameter_name' parameter"));
    }
    int32 SlotIndex = 0;
    Params->TryGetNumberField(TEXT("slot_index"), SlotIndex);

    const TSharedPtr<FJsonObject>* ValueObj;
    if (!Params->TryGetObjectField(TEXT("value"), ValueObj))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("'value' must be an object like {\"r\":1.0,\"g\":0.0,\"b\":0.0,\"a\":1.0}"));
    }
    double R = 0.0, G = 0.0, B = 0.0, A = 1.0;
    (*ValueObj)->TryGetNumberField(TEXT("r"), R);
    (*ValueObj)->TryGetNumberField(TEXT("g"), G);
    (*ValueObj)->TryGetNumberField(TEXT("b"), B);
    (*ValueObj)->TryGetNumberField(TEXT("a"), A);
    FLinearColor ColorValue(R, G, B, A);

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UStaticMeshComponent* MeshComp = TargetActor->FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor '%s' has no StaticMeshComponent"), *ActorName));
    }

    UMaterialInstanceDynamic* DynMaterial = Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(SlotIndex));
    if (!DynMaterial)
    {
        DynMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(SlotIndex);
    }
    if (!DynMaterial)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get or create a dynamic material instance for this slot"));
    }

    DynMaterial->SetVectorParameterValue(FName(*ParamName), ColorValue);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("parameter"), ParamName);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

// =====================================================================
// Alta prioridad — Actores
// =====================================================================

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params)
{
    FString SourceActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), SourceActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }
    FString NewActorName;
    Params->TryGetStringField(TEXT("new_name"), NewActorName);

    AActor* SourceActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == SourceActorName)
        {
            SourceActor = Actor;
            break;
        }
    }
    if (!SourceActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *SourceActorName));
    }

    FVector Offset = FVector::ZeroVector;
    if (Params->HasField(TEXT("location_offset")))
    {
        Offset = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location_offset"));
    }

    FTransform NewTransform = SourceActor->GetActorTransform();
    NewTransform.SetLocation(NewTransform.GetLocation() + Offset);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = GWorld->SpawnActor<AActor>(SourceActor->GetClass(), NewTransform, SpawnParams);
    if (!NewActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn duplicated actor"));
    }

    // Copia mesh, materiales y demas propiedades del actor original (mismo mecanismo
    // que usa el editor internamente al duplicar objetos no relacionados por herencia directa)
    UEngine::CopyPropertiesForUnrelatedObjects(SourceActor, NewActor);
    NewActor->SetActorTransform(NewTransform);

    FString FinalLabel = NewActorName.IsEmpty() ? (SourceActorName + TEXT("_Copy")) : NewActorName;
    NewActor->SetActorLabel(FinalLabel);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("source_actor"), SourceActorName);
    ResultObj->SetStringField(TEXT("new_actor"), NewActor->GetActorLabel());
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorBounds(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }
    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    FVector Origin, BoxExtent;
    TargetActor->GetActorBounds(true, Origin, BoxExtent);

    TSharedPtr<FJsonObject> OriginObj = MakeShared<FJsonObject>();
    OriginObj->SetNumberField(TEXT("x"), Origin.X);
    OriginObj->SetNumberField(TEXT("y"), Origin.Y);
    OriginObj->SetNumberField(TEXT("z"), Origin.Z);

    TSharedPtr<FJsonObject> ExtentObj = MakeShared<FJsonObject>();
    ExtentObj->SetNumberField(TEXT("x"), BoxExtent.X);
    ExtentObj->SetNumberField(TEXT("y"), BoxExtent.Y);
    ExtentObj->SetNumberField(TEXT("z"), BoxExtent.Z);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetObjectField(TEXT("origin"), OriginObj);
    ResultObj->SetObjectField(TEXT("box_extent"), ExtentObj);
    ResultObj->SetNumberField(TEXT("min_z"), Origin.Z - BoxExtent.Z);
    ResultObj->SetNumberField(TEXT("max_z"), Origin.Z + BoxExtent.Z);
    ResultObj->SetBoolField(TEXT("success"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleAttachActorToActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ChildName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ChildName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter (the actor to attach)"));
    }
    FString ParentName;
    if (!Params->TryGetStringField(TEXT("parent_actor_name"), ParentName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'parent_actor_name' parameter"));
    }
    FString SocketName;
    Params->TryGetStringField(TEXT("socket_name"), SocketName);

    // KeepRelative | KeepWorld | SnapToTarget — default KeepRelative
    FString RuleString = TEXT("KeepRelative");
    Params->TryGetStringField(TEXT("attachment_rule"), RuleString);

    EAttachmentRule Rule = EAttachmentRule::KeepRelative;
    if (RuleString == TEXT("KeepWorld"))
    {
        Rule = EAttachmentRule::KeepWorld;
    }
    else if (RuleString == TEXT("SnapToTarget"))
    {
        Rule = EAttachmentRule::SnapToTarget;
    }

    AActor* ChildActor = nullptr;
    AActor* ParentActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        if (Actor->GetName() == ChildName) { ChildActor = Actor; }
        if (Actor->GetName() == ParentName) { ParentActor = Actor; }
    }
    if (!ChildActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ChildName));
    }
    if (!ParentActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Parent actor not found: %s"), *ParentName));
    }

    FAttachmentTransformRules TransformRules(Rule, Rule, Rule, false);
    bool bSuccess = ChildActor->AttachToActor(ParentActor, TransformRules, FName(*SocketName));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ChildName);
    ResultObj->SetStringField(TEXT("parent"), ParentName);
    ResultObj->SetStringField(TEXT("attachment_rule"), RuleString);
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    return ResultObj;
}
CPPEOF

echo "OK: implementaciones agregadas al final de $CPP"
echo ""
echo "===================================================================="
echo "Listo. Ahora compilá el proyecto (ejemplo, ajustá a tu setup):"
echo "  cd ~/unreal-mcp/MCPGameProject"
echo "  ./RunUAT.sh BuildEditor -project=\"MCPGameProject.uproject\"  # o tu build script habitual"
echo ""
echo "Si algo no compila, los backups quedaron en $H.bak y $CPP.bak"
echo "===================================================================="
