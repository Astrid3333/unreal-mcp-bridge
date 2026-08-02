#include "Commands/UnrealMCPComponentCommands.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "EditorSubsystem.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

static AActor* FindActorByLabel(const FString& Name)
{
    if (!GEditor) return nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == Name || It->GetName() == Name)
        {
            return *It;
        }
    }
    return nullptr;
}

UActorComponent* FUnrealMCPComponentCommands::FindComponentByName(AActor* Actor, const FString& ComponentName)
{
    if (!Actor) return nullptr;
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (Comp && (Comp->GetName() == ComponentName || (Comp->GetFName().ToString() == ComponentName)))
        {
            return Comp;
        }
    }
    // Fallback: coincidencia parcial (por si el nombre tiene sufijo _GEN_VARIABLE, etc.)
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (Comp && Comp->GetName().Contains(ComponentName))
        {
            return Comp;
        }
    }
    return nullptr;
}

bool FUnrealMCPComponentCommands::ResolvePropertyPath(UObject* Container, const FString& PropertyPath, FProperty*& OutProperty, void*& OutValuePtr, FString& OutError)
{
    TArray<FString> Tokens;
    PropertyPath.ParseIntoArray(Tokens, TEXT("."), true);
    if (Tokens.Num() == 0)
    {
        OutError = TEXT("property_path vacío");
        return false;
    }

    UStruct* CurrentStruct = Container->GetClass();
    void* CurrentPtr = Container;

    for (int32 i = 0; i < Tokens.Num(); ++i)
    {
        FProperty* Prop = CurrentStruct->FindPropertyByName(FName(*Tokens[i]));
        if (!Prop)
        {
            OutError = FString::Printf(TEXT("Propiedad '%s' no encontrada en '%s'"), *Tokens[i], *CurrentStruct->GetName());
            return false;
        }

        void* PropPtr = Prop->ContainerPtrToValuePtr<void>(CurrentPtr);

        bool bIsLast = (i == Tokens.Num() - 1);
        if (bIsLast)
        {
            OutProperty = Prop;
            OutValuePtr = PropPtr;
            return true;
        }

        FStructProperty* StructProp = CastField<FStructProperty>(Prop);
        if (!StructProp)
        {
            OutError = FString::Printf(TEXT("'%s' no es un struct, no se puede seguir bajando la ruta"), *Tokens[i]);
            return false;
        }
        CurrentStruct = StructProp->Struct;
        CurrentPtr = PropPtr;
    }

    OutError = TEXT("Error interno resolviendo ruta");
    return false;
}

TSharedPtr<FJsonValue> FUnrealMCPComponentCommands::PropertyToJson(FProperty* Property, const void* ValuePtr)
{
    if (FBoolProperty* P = CastField<FBoolProperty>(Property))
        return MakeShareable(new FJsonValueBoolean(P->GetPropertyValue(ValuePtr)));

    if (FFloatProperty* P = CastField<FFloatProperty>(Property))
        return MakeShareable(new FJsonValueNumber(P->GetPropertyValue(ValuePtr)));

    if (FDoubleProperty* P = CastField<FDoubleProperty>(Property))
        return MakeShareable(new FJsonValueNumber(P->GetPropertyValue(ValuePtr)));

    if (FIntProperty* P = CastField<FIntProperty>(Property))
        return MakeShareable(new FJsonValueNumber(P->GetPropertyValue(ValuePtr)));

    if (FByteProperty* P = CastField<FByteProperty>(Property))
    {
        if (UEnum* Enum = P->GetIntPropertyEnum())
        {
            uint8 Val = P->GetPropertyValue(ValuePtr);
            return MakeShareable(new FJsonValueString(Enum->GetNameStringByValue(Val)));
        }
        return MakeShareable(new FJsonValueNumber(P->GetPropertyValue(ValuePtr)));
    }

    if (FEnumProperty* P = CastField<FEnumProperty>(Property))
    {
        FNumericProperty* Underlying = P->GetUnderlyingProperty();
        int64 Val = Underlying->GetSignedIntPropertyValue(ValuePtr);
        UEnum* Enum = P->GetEnum();
        return MakeShareable(new FJsonValueString(Enum->GetNameStringByValue(Val)));
    }

    if (FStrProperty* P = CastField<FStrProperty>(Property))
        return MakeShareable(new FJsonValueString(P->GetPropertyValue(ValuePtr)));

    if (FNameProperty* P = CastField<FNameProperty>(Property))
        return MakeShareable(new FJsonValueString(P->GetPropertyValue(ValuePtr).ToString()));

    if (FTextProperty* P = CastField<FTextProperty>(Property))
        return MakeShareable(new FJsonValueString(P->GetPropertyValue(ValuePtr).ToString()));

    if (FStructProperty* P = CastField<FStructProperty>(Property))
    {
        const FString StructName = P->Struct->GetName();
        if (StructName == TEXT("Vector"))
        {
            const FVector* V = static_cast<const FVector*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Arr = { MakeShareable(new FJsonValueNumber(V->X)), MakeShareable(new FJsonValueNumber(V->Y)), MakeShareable(new FJsonValueNumber(V->Z)) };
            return MakeShareable(new FJsonValueArray(Arr));
        }
        if (StructName == TEXT("Rotator"))
        {
            const FRotator* R = static_cast<const FRotator*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Arr = { MakeShareable(new FJsonValueNumber(R->Pitch)), MakeShareable(new FJsonValueNumber(R->Yaw)), MakeShareable(new FJsonValueNumber(R->Roll)) };
            return MakeShareable(new FJsonValueArray(Arr));
        }
        if (StructName == TEXT("LinearColor"))
        {
            const FLinearColor* C = static_cast<const FLinearColor*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Arr = { MakeShareable(new FJsonValueNumber(C->R)), MakeShareable(new FJsonValueNumber(C->G)), MakeShareable(new FJsonValueNumber(C->B)), MakeShareable(new FJsonValueNumber(C->A)) };
            return MakeShareable(new FJsonValueArray(Arr));
        }
        if (StructName == TEXT("Color"))
        {
            const FColor* C = static_cast<const FColor*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Arr = { MakeShareable(new FJsonValueNumber(C->R)), MakeShareable(new FJsonValueNumber(C->G)), MakeShareable(new FJsonValueNumber(C->B)), MakeShareable(new FJsonValueNumber(C->A)) };
            return MakeShareable(new FJsonValueArray(Arr));
        }
        return MakeShareable(new FJsonValueString(FString::Printf(TEXT("<struct:%s, usar property_path anidado>"), *StructName)));
    }

    if (FObjectProperty* P = CastField<FObjectProperty>(Property))
    {
        UObject* Obj = P->GetPropertyValue(ValuePtr);
        return MakeShareable(new FJsonValueString(Obj ? Obj->GetPathName() : TEXT("None")));
    }

    return MakeShareable(new FJsonValueString(TEXT("<tipo no soportado>")));
}

bool FUnrealMCPComponentCommands::JsonToProperty(FProperty* Property, void* ValuePtr, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
{
    if (FBoolProperty* P = CastField<FBoolProperty>(Property))
    {
        P->SetPropertyValue(ValuePtr, JsonValue->AsBool());
        return true;
    }
    if (FFloatProperty* P = CastField<FFloatProperty>(Property))
    {
        P->SetPropertyValue(ValuePtr, (float)JsonValue->AsNumber());
        return true;
    }
    if (FDoubleProperty* P = CastField<FDoubleProperty>(Property))
    {
        P->SetPropertyValue(ValuePtr, JsonValue->AsNumber());
        return true;
    }
    if (FIntProperty* P = CastField<FIntProperty>(Property))
    {
        P->SetPropertyValue(ValuePtr, (int32)JsonValue->AsNumber());
        return true;
    }
    if (FByteProperty* P = CastField<FByteProperty>(Property))
    {
        if (UEnum* Enum = P->GetIntPropertyEnum())
        {
            int64 Val = Enum->GetValueByNameString(JsonValue->AsString());
            if (Val == INDEX_NONE) { OutError = TEXT("Valor de enum inválido"); return false; }
            P->SetPropertyValue(ValuePtr, (uint8)Val);
        }
        else
        {
            P->SetPropertyValue(ValuePtr, (uint8)JsonValue->AsNumber());
        }
        return true;
    }
    if (FEnumProperty* P = CastField<FEnumProperty>(Property))
    {
        UEnum* Enum = P->GetEnum();
        int64 Val = Enum->GetValueByNameString(JsonValue->AsString());
        if (Val == INDEX_NONE) { OutError = TEXT("Valor de enum inválido"); return false; }
        P->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Val);
        return true;
    }
    if (FStrProperty* P = CastField<FStrProperty>(Property))
    {
        P->SetPropertyValue(ValuePtr, JsonValue->AsString());
        return true;
    }
    if (FNameProperty* P = CastField<FNameProperty>(Property))
    {
        P->SetPropertyValue(ValuePtr, FName(*JsonValue->AsString()));
        return true;
    }
    if (FTextProperty* P = CastField<FTextProperty>(Property))
    {
        P->SetPropertyValue(ValuePtr, FText::FromString(JsonValue->AsString()));
        return true;
    }
    if (FStructProperty* P = CastField<FStructProperty>(Property))
    {
        const FString StructName = P->Struct->GetName();
        const TArray<TSharedPtr<FJsonValue>>* Arr;
        if (!JsonValue->TryGetArray(Arr))
        {
            OutError = FString::Printf(TEXT("Se esperaba un array para el struct '%s'"), *StructName);
            return false;
        }
        if (StructName == TEXT("Vector") && Arr->Num() >= 3)
        {
            FVector* V = static_cast<FVector*>(ValuePtr);
            *V = FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
            return true;
        }
        if (StructName == TEXT("Rotator") && Arr->Num() >= 3)
        {
            FRotator* R = static_cast<FRotator*>(ValuePtr);
            *R = FRotator((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
            return true;
        }
        if (StructName == TEXT("LinearColor") && Arr->Num() >= 3)
        {
            FLinearColor* C = static_cast<FLinearColor*>(ValuePtr);
            float A = Arr->Num() >= 4 ? (*Arr)[3]->AsNumber() : 1.0f;
            *C = FLinearColor((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber(), A);
            return true;
        }
        if (StructName == TEXT("Color") && Arr->Num() >= 3)
        {
            FColor* C = static_cast<FColor*>(ValuePtr);
            uint8 A = Arr->Num() >= 4 ? (uint8)(*Arr)[3]->AsNumber() : 255;
            *C = FColor((uint8)(*Arr)[0]->AsNumber(), (uint8)(*Arr)[1]->AsNumber(), (uint8)(*Arr)[2]->AsNumber(), A);
            return true;
        }
        OutError = FString::Printf(TEXT("Struct '%s' no soportado directamente, usá property_path anidado (ej: 'LightColor.R')"), *StructName);
        return false;
    }

    OutError = TEXT("Tipo de propiedad no soportado para escritura");
    return false;
}

TSharedPtr<FJsonObject> FUnrealMCPComponentCommands::HandleListComponents(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Falta 'actor_name'"));
        return Result;
    }

    AActor* Actor = FindActorByLabel(ActorName);
    if (!Actor)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' no encontrado"), *ActorName));
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> Components;
    for (UActorComponent* Comp : Actor->GetComponents())
    {
        if (!Comp) continue;
        TSharedPtr<FJsonObject> CompObj = MakeShareable(new FJsonObject);
        CompObj->SetStringField(TEXT("name"), Comp->GetName());
        CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
        Components.Add(MakeShareable(new FJsonValueObject(CompObj)));
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetArrayField(TEXT("components"), Components);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPComponentCommands::HandleGetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    FString ActorName, ComponentName, PropertyPath;
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    Params->TryGetStringField(TEXT("property_path"), PropertyPath);

    AActor* Actor = FindActorByLabel(ActorName);
    if (!Actor)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' no encontrado"), *ActorName));
        return Result;
    }
    UActorComponent* Comp = FindComponentByName(Actor, ComponentName);
    if (!Comp)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Componente '%s' no encontrado en '%s'"), *ComponentName, *ActorName));
        return Result;
    }

    FProperty* Prop = nullptr;
    void* ValuePtr = nullptr;
    FString Error;
    if (!ResolvePropertyPath(Comp, PropertyPath, Prop, ValuePtr, Error))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), Error);
        return Result;
    }

    Result->SetBoolField(TEXT("success"), true);
    Result->SetField(TEXT("value"), PropertyToJson(Prop, ValuePtr));
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPComponentCommands::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    FString ActorName, ComponentName, PropertyPath;
    Params->TryGetStringField(TEXT("actor_name"), ActorName);
    Params->TryGetStringField(TEXT("component_name"), ComponentName);
    Params->TryGetStringField(TEXT("property_path"), PropertyPath);

    TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value"));
    if (!ValueField.IsValid())
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), TEXT("Falta 'value'"));
        return Result;
    }

    AActor* Actor = FindActorByLabel(ActorName);
    if (!Actor)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Actor '%s' no encontrado"), *ActorName));
        return Result;
    }
    UActorComponent* Comp = FindComponentByName(Actor, ComponentName);
    if (!Comp)
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Componente '%s' no encontrado en '%s'"), *ComponentName, *ActorName));
        return Result;
    }

    FProperty* Prop = nullptr;
    void* ValuePtr = nullptr;
    FString Error;
    if (!ResolvePropertyPath(Comp, PropertyPath, Prop, ValuePtr, Error))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), Error);
        return Result;
    }

    Comp->Modify();

    if (!JsonToProperty(Prop, ValuePtr, ValueField, Error))
    {
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("message"), Error);
        return Result;
    }

    Comp->PostEditChange();
    Comp->MarkPackageDirty();

    Result->SetBoolField(TEXT("success"), true);
    return Result;
}

TSharedPtr<FJsonObject> FUnrealMCPComponentCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("list_components"))
        return HandleListComponents(Params);
    if (CommandType == TEXT("get_component_property"))
        return HandleGetComponentProperty(Params);
    if (CommandType == TEXT("set_actor_component_property"))
        return HandleSetComponentProperty(Params);

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Comando de componente desconocido: %s"), *CommandType));
    return Result;
}
