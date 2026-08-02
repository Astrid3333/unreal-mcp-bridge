#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class UActorComponent;

class UNREALMCP_API FUnrealMCPComponentCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    TSharedPtr<FJsonObject> HandleListComponents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetComponentProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params);

    UActorComponent* FindComponentByName(AActor* Actor, const FString& ComponentName);

    // Resuelve una ruta tipo "LightColor.R" caminando por FStructProperty anidados.
    bool ResolvePropertyPath(UObject* Container, const FString& PropertyPath, FProperty*& OutProperty, void*& OutValuePtr, FString& OutError);

    TSharedPtr<FJsonValue> PropertyToJson(FProperty* Property, const void* ValuePtr);
    bool JsonToProperty(FProperty* Property, void* ValuePtr, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError);
};
