#include "Commands/UnrealMCPCommonUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Selection.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabase.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "UnrealClient.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "SEditorViewport.h"
#include "Slate/SceneViewport.h"
#include "Framework/Docking/TabManager.h"

// JSON Utilities
TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateErrorResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), false);
    ResponseObject->SetStringField(TEXT("error"), Message);
    return ResponseObject;
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
    TSharedPtr<FJsonObject> ResponseObject = MakeShared<FJsonObject>();
    ResponseObject->SetBoolField(TEXT("success"), true);
    
    if (Data.IsValid())
    {
        ResponseObject->SetObjectField(TEXT("data"), Data);
    }
    
    return ResponseObject;
}

void FUnrealMCPCommonUtils::GetIntArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<int32>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((int32)Value->AsNumber());
        }
    }
}

void FUnrealMCPCommonUtils::GetFloatArrayFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, TArray<float>& OutArray)
{
    OutArray.Reset();
    
    if (!JsonObject->HasField(FieldName))
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray))
    {
        for (const TSharedPtr<FJsonValue>& Value : *JsonArray)
        {
            OutArray.Add((float)Value->AsNumber());
        }
    }
}

FVector2D FUnrealMCPCommonUtils::GetVector2DFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector2D Result(0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 2)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
    }
    
    return Result;
}

FVector FUnrealMCPCommonUtils::GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FVector Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.X = (float)(*JsonArray)[0]->AsNumber();
        Result.Y = (float)(*JsonArray)[1]->AsNumber();
        Result.Z = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

FRotator FUnrealMCPCommonUtils::GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
{
    FRotator Result(0.0f, 0.0f, 0.0f);
    
    if (!JsonObject->HasField(FieldName))
    {
        return Result;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* JsonArray;
    if (JsonObject->TryGetArrayField(FieldName, JsonArray) && JsonArray->Num() >= 3)
    {
        Result.Pitch = (float)(*JsonArray)[0]->AsNumber();
        Result.Yaw = (float)(*JsonArray)[1]->AsNumber();
        Result.Roll = (float)(*JsonArray)[2]->AsNumber();
    }
    
    return Result;
}

// Blueprint Utilities
UBlueprint* FUnrealMCPCommonUtils::FindBlueprint(const FString& BlueprintName)
{
    return FindBlueprintByName(BlueprintName);
}

UBlueprint* FUnrealMCPCommonUtils::FindBlueprintByName(const FString& BlueprintName)
{
    if (BlueprintName.IsEmpty())
    {
        return nullptr;
    }

    // Full path provided by the caller: never prefix it (prefixing produced
    // paths like "/Game/Blueprints//Game/..." whose double slashes crash
    // CreatePackage with a fatal error).
    if (BlueprintName.StartsWith(TEXT("/")))
    {
        FString Path = BlueprintName;
        while (Path.Contains(TEXT("//")))
        {
            Path.ReplaceInline(TEXT("//"), TEXT("/"));
        }

        if (UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *Path))
        {
            return BP;
        }
        if (Path.Contains(TEXT(".")))
        {
            return nullptr;
        }
        const FString ObjectPath = Path + TEXT(".") + FPaths::GetCleanFilename(Path);
        return LoadObject<UBlueprint>(nullptr, *ObjectPath);
    }

    // Short name: find it anywhere under /Game via the asset registry.
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.bRecursivePaths = true;
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);
    for (const FAssetData& Asset : Assets)
    {
        if (Asset.AssetName.ToString() == BlueprintName)
        {
            return Cast<UBlueprint>(Asset.GetAsset());
        }
    }

    // Legacy default location as a final fallback.
    return LoadObject<UBlueprint>(nullptr, *(TEXT("/Game/Blueprints/") + BlueprintName));
}

UEdGraph* FUnrealMCPCommonUtils::FindOrCreateEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Try to find the event graph
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph->GetName().Contains(TEXT("EventGraph")))
        {
            return Graph;
        }
    }
    
    // Create a new event graph if none exists
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(TEXT("EventGraph")), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
    return NewGraph;
}

// Blueprint node utilities
UK2Node_Event* FUnrealMCPCommonUtils::CreateEventNode(UEdGraph* Graph, const FString& EventName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
    if (!Blueprint)
    {
        return nullptr;
    }
    
    // Check for existing event node with this exact name
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Using existing event node with name %s (ID: %s)"), 
                *EventName, *EventNode->NodeGuid.ToString());
            return EventNode;
        }
    }

    // No existing node found, create a new one
    UK2Node_Event* EventNode = nullptr;
    
    // Find the function to create the event
    UClass* BlueprintClass = Blueprint->GeneratedClass;
    UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
    
    if (EventFunction)
    {
        EventNode = NewObject<UK2Node_Event>(Graph);
        EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
        EventNode->NodePosX = Position.X;
        EventNode->NodePosY = Position.Y;
        Graph->AddNode(EventNode, true);
        EventNode->PostPlacedNewNode();
        EventNode->AllocateDefaultPins();
        UE_LOG(LogTemp, Display, TEXT("Created new event node with name %s (ID: %s)"), 
            *EventName, *EventNode->NodeGuid.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find function for event name: %s"), *EventName);
    }
    
    return EventNode;
}

UK2Node_CallFunction* FUnrealMCPCommonUtils::CreateFunctionCallNode(UEdGraph* Graph, UFunction* Function, const FVector2D& Position)
{
    if (!Graph || !Function)
    {
        return nullptr;
    }
    
    UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
    FunctionNode->SetFromFunction(Function);
    FunctionNode->NodePosX = Position.X;
    FunctionNode->NodePosY = Position.Y;
    Graph->AddNode(FunctionNode, true);
    FunctionNode->CreateNewGuid();
    FunctionNode->PostPlacedNewNode();
    FunctionNode->AllocateDefaultPins();
    
    return FunctionNode;
}

UK2Node_VariableGet* FUnrealMCPCommonUtils::CreateVariableGetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableGetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableGetNode->NodePosX = Position.X;
        VariableGetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableGetNode, true);
        VariableGetNode->PostPlacedNewNode();
        VariableGetNode->AllocateDefaultPins();
        
        return VariableGetNode;
    }
    
    return nullptr;
}

UK2Node_VariableSet* FUnrealMCPCommonUtils::CreateVariableSetNode(UEdGraph* Graph, UBlueprint* Blueprint, const FString& VariableName, const FVector2D& Position)
{
    if (!Graph || !Blueprint)
    {
        return nullptr;
    }
    
    UK2Node_VariableSet* VariableSetNode = NewObject<UK2Node_VariableSet>(Graph);
    
    FName VarName(*VariableName);
    FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, VarName);
    
    if (Property)
    {
        VariableSetNode->VariableReference.SetFromField<FProperty>(Property, false);
        VariableSetNode->NodePosX = Position.X;
        VariableSetNode->NodePosY = Position.Y;
        Graph->AddNode(VariableSetNode, true);
        VariableSetNode->PostPlacedNewNode();
        VariableSetNode->AllocateDefaultPins();
        
        return VariableSetNode;
    }
    
    return nullptr;
}

UK2Node_InputAction* FUnrealMCPCommonUtils::CreateInputActionNode(UEdGraph* Graph, const FString& ActionName, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(Graph);
    InputActionNode->InputActionName = FName(*ActionName);
    InputActionNode->NodePosX = Position.X;
    InputActionNode->NodePosY = Position.Y;
    Graph->AddNode(InputActionNode, true);
    InputActionNode->CreateNewGuid();
    InputActionNode->PostPlacedNewNode();
    InputActionNode->AllocateDefaultPins();
    
    return InputActionNode;
}

UK2Node_Self* FUnrealMCPCommonUtils::CreateSelfReferenceNode(UEdGraph* Graph, const FVector2D& Position)
{
    if (!Graph)
    {
        return nullptr;
    }
    
    UK2Node_Self* SelfNode = NewObject<UK2Node_Self>(Graph);
    SelfNode->NodePosX = Position.X;
    SelfNode->NodePosY = Position.Y;
    Graph->AddNode(SelfNode, true);
    SelfNode->CreateNewGuid();
    SelfNode->PostPlacedNewNode();
    SelfNode->AllocateDefaultPins();
    
    return SelfNode;
}

bool FUnrealMCPCommonUtils::ConnectGraphNodes(UEdGraph* Graph, UEdGraphNode* SourceNode, const FString& SourcePinName, 
                                           UEdGraphNode* TargetNode, const FString& TargetPinName)
{
    if (!Graph || !SourceNode || !TargetNode)
    {
        return false;
    }
    
    UEdGraphPin* SourcePin = FindPin(SourceNode, SourcePinName, EGPD_Output);
    UEdGraphPin* TargetPin = FindPin(TargetNode, TargetPinName, EGPD_Input);
    
    if (SourcePin && TargetPin)
    {
        SourcePin->MakeLinkTo(TargetPin);
        return true;
    }
    
    return false;
}

UEdGraphPin* FUnrealMCPCommonUtils::FindPin(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    
    // Log all pins for debugging
    UE_LOG(LogTemp, Display, TEXT("FindPin: Looking for pin '%s' (Direction: %d) in node '%s'"), 
           *PinName, (int32)Direction, *Node->GetName());
    
    for (UEdGraphPin* Pin : Node->Pins)
    {
        UE_LOG(LogTemp, Display, TEXT("  - Available pin: '%s', Direction: %d, Category: %s"), 
               *Pin->PinName.ToString(), (int32)Pin->Direction, *Pin->PinType.PinCategory.ToString());
    }
    
    // First try exact match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString() == PinName && (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found exact matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If no exact match and we're looking for a component reference, try case-insensitive match
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase) && 
            (Direction == EGPD_MAX || Pin->Direction == Direction))
        {
            UE_LOG(LogTemp, Display, TEXT("  - Found case-insensitive matching pin: '%s'"), *Pin->PinName.ToString());
            return Pin;
        }
    }
    
    // If we're looking for a component output and didn't find it by name, try to find the first data output pin
    if (Direction == EGPD_Output && Cast<UK2Node_VariableGet>(Node) != nullptr)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UE_LOG(LogTemp, Display, TEXT("  - Found fallback data output pin: '%s'"), *Pin->PinName.ToString());
                return Pin;
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("  - No matching pin found for '%s'"), *PinName);
    return nullptr;
}

// Actor utilities
TSharedPtr<FJsonValue> FUnrealMCPCommonUtils::ActorToJson(AActor* Actor)
{
    if (!Actor)
    {
        return MakeShared<FJsonValueNull>();
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);
    
    return MakeShared<FJsonValueObject>(ActorObject);
}

TSharedPtr<FJsonObject> FUnrealMCPCommonUtils::ActorToJsonObject(AActor* Actor, bool bDetailed)
{
    if (!Actor)
    {
        return nullptr;
    }
    
    TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
    ActorObject->SetStringField(TEXT("name"), Actor->GetName());
    ActorObject->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
    
    FVector Location = Actor->GetActorLocation();
    TArray<TSharedPtr<FJsonValue>> LocationArray;
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.X));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Y));
    LocationArray.Add(MakeShared<FJsonValueNumber>(Location.Z));
    ActorObject->SetArrayField(TEXT("location"), LocationArray);
    
    FRotator Rotation = Actor->GetActorRotation();
    TArray<TSharedPtr<FJsonValue>> RotationArray;
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
    RotationArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
    ActorObject->SetArrayField(TEXT("rotation"), RotationArray);
    
    FVector Scale = Actor->GetActorScale3D();
    TArray<TSharedPtr<FJsonValue>> ScaleArray;
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.X));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Y));
    ScaleArray.Add(MakeShared<FJsonValueNumber>(Scale.Z));
    ActorObject->SetArrayField(TEXT("scale"), ScaleArray);

    if (bDetailed)
    {
        TArray<UActorComponent*> Components;
        Actor->GetComponents(Components);
        TArray<TSharedPtr<FJsonValue>> ComponentsArray;

        static const TArray<FString> CommonProps = {
            TEXT("Intensity"), TEXT("LightColor"), TEXT("AttenuationRadius"),
            TEXT("Mobility"), TEXT("Visible"), TEXT("RelativeLocation"),
            TEXT("RelativeRotation"), TEXT("RelativeScale3D")
        };

        for (UActorComponent* Comp : Components)
        {
            if (!Comp)
            {
                continue;
            }

            TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
            CompObj->SetStringField(TEXT("name"), Comp->GetName());
            CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());

            TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
            for (const FString& PropName : CommonProps)
            {
                TSharedPtr<FJsonValue> PropValue;
                FString ErrMsg;
                if (FUnrealMCPCommonUtils::GetObjectProperty(Comp, PropName, PropValue, ErrMsg) && PropValue.IsValid())
                {
                    PropsObj->SetField(PropName, PropValue);
                }
            }
            CompObj->SetObjectField(TEXT("properties"), PropsObj);

            ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
        }

        ActorObject->SetArrayField(TEXT("components"), ComponentsArray);
    }

    return ActorObject;
}

UK2Node_Event* FUnrealMCPCommonUtils::FindExistingEventNode(UEdGraph* Graph, const FString& EventName)
{
    if (!Graph)
    {
        return nullptr;
    }

    // Look for existing event nodes
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
        {
            UE_LOG(LogTemp, Display, TEXT("Found existing event node with name: %s"), *EventName);
            return EventNode;
        }
    }

    return nullptr;
}

static bool SetObjectPropertyImpl(UObject* Object, const FString& PropertyName, 
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    // Dot-path syntax: "Component.Property" or "Component.SubComponent.Property"
    // lets callers reach properties on actor components, e.g. "LightComponent.Intensity".
    FString FirstSegment;
    FString Remainder;
    if (PropertyName.Split(TEXT("."), &FirstSegment, &Remainder))
    {
        UObject* SubObject = nullptr;

        // 1) Direct UPROPERTY on the object pointing to a UObject (e.g. LightComponent)
        FProperty* ObjProp = Object->GetClass()->FindPropertyByName(*FirstSegment);
        if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ObjProp))
        {
            SubObject = ObjectProperty->GetObjectPropertyValue(
                ObjectProperty->ContainerPtrToValuePtr<void>(Object));
        }

        // 1b) Or a nested struct on this object (e.g. RelativeLocation.X) - navigate directly, no UObject needed
        if (!SubObject)
        {
            if (FStructProperty* StructProp = CastField<FStructProperty>(ObjProp))
            {
                void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(Object);
                return FUnrealMCPCommonUtils::SetStructPropertyByPath(StructProp->Struct, StructAddr, Remainder, Value, OutErrorMessage);
            }
        }

        // 2) Fall back to searching actor components by name
        if (!SubObject)
        {
            if (AActor* Actor = Cast<AActor>(Object))
            {
                TArray<UActorComponent*> Components;
                Actor->GetComponents(Components);
                for (UActorComponent* Comp : Components)
                {
                    if (Comp && Comp->GetFName() == FName(*FirstSegment))
                    {
                        SubObject = Comp;
                        break;
                    }
                }

                // 3) Convenience aliases for common cases
                if (!SubObject && (FirstSegment == TEXT("Light") || FirstSegment == TEXT("LightComponent")))
                {
                    for (UActorComponent* Comp : Components)
                    {
                        if (Comp && Comp->IsA(ULightComponent::StaticClass()))
                        {
                            SubObject = Comp;
                            break;
                        }
                    }
                }
            }
        }

        if (!SubObject)
        {
            OutErrorMessage = FString::Printf(TEXT("Component not found: %s"), *FirstSegment);
            return false;
        }

        return FUnrealMCPCommonUtils::SetObjectProperty(SubObject, Remainder, Value, OutErrorMessage);
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        // Fallback: bare property name (no dot) didn't match directly on the object -
        // search its actor components for a single match before giving up.
        if (AActor* Actor = Cast<AActor>(Object))
        {
            TArray<UActorComponent*> Components;
            Actor->GetComponents(Components);

            UActorComponent* MatchedComp = nullptr;
            int32 MatchCount = 0;
            for (UActorComponent* Comp : Components)
            {
                if (Comp && Comp->GetClass()->FindPropertyByName(*PropertyName))
                {
                    MatchedComp = Comp;
                    MatchCount++;
                }
            }

            if (MatchCount == 1)
            {
                return FUnrealMCPCommonUtils::SetObjectProperty(MatchedComp, PropertyName, Value, OutErrorMessage);
            }
            else if (MatchCount > 1)
            {
                OutErrorMessage = FString::Printf(
                    TEXT("Property '%s' is ambiguous - found on %d components. Qualify it, e.g. \"ComponentName.%s\""),
                    *PropertyName, MatchCount, *PropertyName);
                return false;
            }
        }

        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    // Special-case scene component transform properties: writing RelativeLocation/
    // RelativeRotation/RelativeScale3D directly via reflection changes the raw struct
    // but never calls UpdateComponentToWorld(), so the cached world transform (what
    // GetActorRotation()/GetActorLocation() actually read) stays stale. Route these
    // through the real setters instead.
    if (USceneComponent* SceneComp = Cast<USceneComponent>(Object))
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr;
        if (PropertyName == TEXT("RelativeLocation") && Value->TryGetArray(Arr) && Arr->Num() >= 3)
        {
            SceneComp->SetRelativeLocation(FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()));
            return true;
        }
        else if (PropertyName == TEXT("RelativeRotation") && Value->TryGetArray(Arr) && Arr->Num() >= 3)
        {
            SceneComp->SetRelativeRotation(FRotator((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()));
            return true;
        }
        else if (PropertyName == TEXT("RelativeScale3D") && Value->TryGetArray(Arr) && Arr->Num() >= 3)
        {
            SceneComp->SetRelativeScale3D(FVector((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber()));
            return true;
        }
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);
    
    // Handle different property types
    if (Property->IsA<FBoolProperty>())
    {
        ((FBoolProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        int32 IntValue = static_cast<int32>(Value->AsNumber());
        FIntProperty* IntProperty = CastField<FIntProperty>(Property);
        if (IntProperty)
        {
            IntProperty->SetPropertyValue_InContainer(Object, IntValue);
            return true;
        }
    }
    else if (Property->IsA<FFloatProperty>())
    {
        ((FFloatProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        ((FStrProperty*)Property)->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (Property->IsA<FTextProperty>())
    {
        ((FTextProperty*)Property)->SetPropertyValue(PropertyAddr, FText::FromString(Value->AsString()));
        return true;
    }
    else if (Property->IsA<FNameProperty>())
    {
        ((FNameProperty*)Property)->SetPropertyValue(PropertyAddr, FName(*Value->AsString()));
        return true;
    }
    else if (Property->IsA<FStructProperty>())
    {
        FStructProperty* StructProp = CastField<FStructProperty>(Property);
        UScriptStruct* Struct = StructProp->Struct;
        const TArray<TSharedPtr<FJsonValue>>* Arr;

        if (Struct == TBaseStructure<FVector>::Get())
        {
            if (Value->TryGetArray(Arr) && Arr->Num() >= 3)
            {
                FVector* VecPtr = (FVector*)PropertyAddr;
                VecPtr->X = (*Arr)[0]->AsNumber();
                VecPtr->Y = (*Arr)[1]->AsNumber();
                VecPtr->Z = (*Arr)[2]->AsNumber();
                return true;
            }
            OutErrorMessage = TEXT("Expected array of 3 numbers for FVector property");
            return false;
        }
        else if (Struct == TBaseStructure<FRotator>::Get())
        {
            if (Value->TryGetArray(Arr) && Arr->Num() >= 3)
            {
                FRotator* RotPtr = (FRotator*)PropertyAddr;
                RotPtr->Pitch = (*Arr)[0]->AsNumber();
                RotPtr->Yaw = (*Arr)[1]->AsNumber();
                RotPtr->Roll = (*Arr)[2]->AsNumber();
                return true;
            }
            OutErrorMessage = TEXT("Expected array of 3 numbers for FRotator property");
            return false;
        }
        else if (Struct == TBaseStructure<FLinearColor>::Get())
        {
            if (Value->TryGetArray(Arr) && Arr->Num() >= 3)
            {
                FLinearColor* ColPtr = (FLinearColor*)PropertyAddr;
                ColPtr->R = (*Arr)[0]->AsNumber();
                ColPtr->G = (*Arr)[1]->AsNumber();
                ColPtr->B = (*Arr)[2]->AsNumber();
                ColPtr->A = Arr->Num() >= 4 ? (*Arr)[3]->AsNumber() : 1.0f;
                return true;
            }
            OutErrorMessage = TEXT("Expected array of 3-4 numbers for FLinearColor property");
            return false;
        }
        else if (Struct == TBaseStructure<FColor>::Get())
        {
            if (Value->TryGetArray(Arr) && Arr->Num() >= 3)
            {
                FColor* ColPtr = (FColor*)PropertyAddr;
                ColPtr->R = (uint8)(*Arr)[0]->AsNumber();
                ColPtr->G = (uint8)(*Arr)[1]->AsNumber();
                ColPtr->B = (uint8)(*Arr)[2]->AsNumber();
                ColPtr->A = Arr->Num() >= 4 ? (uint8)(*Arr)[3]->AsNumber() : 255;
                return true;
            }
            OutErrorMessage = TEXT("Expected array of 3-4 numbers for FColor property");
            return false;
        }

        OutErrorMessage = FString::Printf(TEXT("Unsupported struct type: %s for property %s"),
                                        *Struct->GetName(), *PropertyName);
        return false;
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp ? ByteProp->GetIntPropertyEnum() : nullptr;
        
        if (EnumDef)
        {
            if (Value->Type == EJson::Number)
            {
                uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
                ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                return true;
            }
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                if (EnumValueName.IsNumeric())
                {
                    uint8 ByteValue = FCString::Atoi(*EnumValueName);
                    ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
                    return true;
                }
                
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
                    return true;
                }
                else
                {
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
        else
        {
            uint8 ByteValue = static_cast<uint8>(Value->AsNumber());
            ByteProp->SetPropertyValue(PropertyAddr, ByteValue);
            return true;
        }
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp ? EnumProp->GetEnum() : nullptr;
        FNumericProperty* UnderlyingNumericProp = EnumProp ? EnumProp->GetUnderlyingProperty() : nullptr;
        
        if (EnumDef && UnderlyingNumericProp)
        {
            if (Value->Type == EJson::Number)
            {
                int64 EnumValue = static_cast<int64>(Value->AsNumber());
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                return true;
            }
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                
                if (EnumValueName.IsNumeric())
                {
                    int64 EnumValue = FCString::Atoi64(*EnumValueName);
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    return true;
                }
                
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    EnumValue = EnumDef->GetValueByNameString(Value->AsString());
                }
                
                if (EnumValue != INDEX_NONE)
                {
                    UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                    return true;
                }
                else
                {
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
            }
        }
    }
    
    else if (Property->IsA<FObjectProperty>())
    {
        FObjectProperty* ObjProperty = CastField<FObjectProperty>(Property);
        FString AssetPath = Value->AsString();
        UObject* Asset = StaticLoadObject(ObjProperty->PropertyClass, nullptr, *AssetPath);
        if (!Asset)
        {
            OutErrorMessage = FString::Printf(TEXT("Could not load asset at path: %s"), *AssetPath);
            return false;
        }
        ObjProperty->SetObjectPropertyValue(PropertyAddr, Asset);
        return true;
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"), 
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
}

bool FUnrealMCPCommonUtils::SetObjectProperty(UObject* Object, const FString& PropertyName, 
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    bool bSuccess = SetObjectPropertyImpl(Object, PropertyName, Value, OutErrorMessage);

    if (bSuccess && Object)
    {
        Object->Modify();
        FPropertyChangedEvent PropertyChangedEvent(nullptr, EPropertyChangeType::ValueSet);
        Object->PostEditChangeProperty(PropertyChangedEvent);

        if (UActorComponent* Comp = Cast<UActorComponent>(Object))
        {
            Comp->MarkRenderStateDirty();
        }

        if (AActor* Actor = Cast<AActor>(Object))
        {
            Actor->MarkComponentsRenderStateDirty();
        }
    }

    return bSuccess;
}

bool FUnrealMCPCommonUtils::GetStructPropertyByPath(UScriptStruct* Struct, const void* StructPtr, const FString& PropertyPath,
                                 TSharedPtr<FJsonValue>& OutValue, FString& OutErrorMessage)
{
    if (!Struct || !StructPtr)
    {
        OutErrorMessage = TEXT("Invalid struct");
        return false;
    }

    FString FirstSegment;
    FString Remainder;
    if (PropertyPath.Split(TEXT("."), &FirstSegment, &Remainder))
    {
        FProperty* Prop = Struct->FindPropertyByName(*FirstSegment);
        if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
        {
            const void* InnerAddr = StructProp->ContainerPtrToValuePtr<void>(StructPtr);
            return GetStructPropertyByPath(StructProp->Struct, InnerAddr, Remainder, OutValue, OutErrorMessage);
        }

        OutErrorMessage = FString::Printf(TEXT("Field '%s' is not a struct, cannot navigate further"), *FirstSegment);
        return false;
    }

    FProperty* Property = Struct->FindPropertyByName(*PropertyPath);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Struct field not found: %s"), *PropertyPath);
        return false;
    }

    const void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(StructPtr);

    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueString>(StrProp->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        UScriptStruct* InnerStruct = StructProp->Struct;
        TArray<TSharedPtr<FJsonValue>> Arr;

        if (InnerStruct == TBaseStructure<FVector>::Get())
        {
            const FVector* VecPtr = (const FVector*)PropertyAddr;
            Arr.Add(MakeShared<FJsonValueNumber>(VecPtr->X));
            Arr.Add(MakeShared<FJsonValueNumber>(VecPtr->Y));
            Arr.Add(MakeShared<FJsonValueNumber>(VecPtr->Z));
            OutValue = MakeShared<FJsonValueArray>(Arr);
            return true;
        }
        else if (InnerStruct == TBaseStructure<FRotator>::Get())
        {
            const FRotator* RotPtr = (const FRotator*)PropertyAddr;
            Arr.Add(MakeShared<FJsonValueNumber>(RotPtr->Pitch));
            Arr.Add(MakeShared<FJsonValueNumber>(RotPtr->Yaw));
            Arr.Add(MakeShared<FJsonValueNumber>(RotPtr->Roll));
            OutValue = MakeShared<FJsonValueArray>(Arr);
            return true;
        }
        else if (InnerStruct == TBaseStructure<FLinearColor>::Get())
        {
            const FLinearColor* ColPtr = (const FLinearColor*)PropertyAddr;
            Arr.Add(MakeShared<FJsonValueNumber>(ColPtr->R));
            Arr.Add(MakeShared<FJsonValueNumber>(ColPtr->G));
            Arr.Add(MakeShared<FJsonValueNumber>(ColPtr->B));
            Arr.Add(MakeShared<FJsonValueNumber>(ColPtr->A));
            OutValue = MakeShared<FJsonValueArray>(Arr);
            return true;
        }
        else if (InnerStruct == TBaseStructure<FColor>::Get())
        {
            const FColor* ColPtr = (const FColor*)PropertyAddr;
            Arr.Add(MakeShared<FJsonValueNumber>(ColPtr->R));
            Arr.Add(MakeShared<FJsonValueNumber>(ColPtr->G));
            Arr.Add(MakeShared<FJsonValueNumber>(ColPtr->B));
            Arr.Add(MakeShared<FJsonValueNumber>(ColPtr->A));
            OutValue = MakeShared<FJsonValueArray>(Arr);
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Unsupported nested struct type: %s for field %s"),
                                        *InnerStruct->GetName(), *PropertyPath);
        return false;
    }
    else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        OutValue = MakeShared<FJsonValueNumber>(ByteProp->GetPropertyValue(PropertyAddr));
        return true;
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported field type: %s for field %s"),
                                    *Property->GetClass()->GetName(), *PropertyPath);
    return false;
}

bool FUnrealMCPCommonUtils::GetObjectProperty(UObject* Object, const FString& PropertyName,
                                 TSharedPtr<FJsonValue>& OutValue, FString& OutErrorMessage)
{
    if (!Object)
    {
        OutErrorMessage = TEXT("Invalid object");
        return false;
    }

    FString FirstSegment;
    FString Remainder;
    if (PropertyName.Split(TEXT("."), &FirstSegment, &Remainder))
    {
        UObject* SubObject = nullptr;

        FProperty* ObjProp = Object->GetClass()->FindPropertyByName(*FirstSegment);
        if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(ObjProp))
        {
            SubObject = ObjectProperty->GetObjectPropertyValue(
                ObjectProperty->ContainerPtrToValuePtr<void>(Object));
        }

        if (!SubObject)
        {
            if (FStructProperty* StructProp = CastField<FStructProperty>(ObjProp))
            {
                const void* StructAddr = StructProp->ContainerPtrToValuePtr<void>(Object);
                return GetStructPropertyByPath(StructProp->Struct, StructAddr, Remainder, OutValue, OutErrorMessage);
            }
        }

        if (!SubObject)
        {
            if (AActor* Actor = Cast<AActor>(Object))
            {
                TArray<UActorComponent*> Components;
                Actor->GetComponents(Components);
                for (UActorComponent* Comp : Components)
                {
                    if (Comp && Comp->GetFName() == FName(*FirstSegment))
                    {
                        SubObject = Comp;
                        break;
                    }
                }

                if (!SubObject && (FirstSegment == TEXT("Light") || FirstSegment == TEXT("LightComponent")))
                {
                    for (UActorComponent* Comp : Components)
                    {
                        if (Comp && Comp->IsA(ULightComponent::StaticClass()))
                        {
                            SubObject = Comp;
                            break;
                        }
                    }
                }
            }
        }

        if (!SubObject)
        {
            OutErrorMessage = FString::Printf(TEXT("Component not found: %s"), *FirstSegment);
            return false;
        }

        return GetObjectProperty(SubObject, Remainder, OutValue, OutErrorMessage);
    }

    if (USceneComponent* SceneComp = Cast<USceneComponent>(Object))
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        if (PropertyName == TEXT("RelativeLocation"))
        {
            FVector Loc = SceneComp->GetRelativeLocation();
            Arr.Add(MakeShared<FJsonValueNumber>(Loc.X));
            Arr.Add(MakeShared<FJsonValueNumber>(Loc.Y));
            Arr.Add(MakeShared<FJsonValueNumber>(Loc.Z));
            OutValue = MakeShared<FJsonValueArray>(Arr);
            return true;
        }
        else if (PropertyName == TEXT("RelativeRotation"))
        {
            FRotator Rot = SceneComp->GetRelativeRotation();
            Arr.Add(MakeShared<FJsonValueNumber>(Rot.Pitch));
            Arr.Add(MakeShared<FJsonValueNumber>(Rot.Yaw));
            Arr.Add(MakeShared<FJsonValueNumber>(Rot.Roll));
            OutValue = MakeShared<FJsonValueArray>(Arr);
            return true;
        }
        else if (PropertyName == TEXT("RelativeScale3D"))
        {
            FVector Scale = SceneComp->GetRelativeScale3D();
            Arr.Add(MakeShared<FJsonValueNumber>(Scale.X));
            Arr.Add(MakeShared<FJsonValueNumber>(Scale.Y));
            Arr.Add(MakeShared<FJsonValueNumber>(Scale.Z));
            OutValue = MakeShared<FJsonValueArray>(Arr);
            return true;
        }
    }

    FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Property not found: %s"), *PropertyName);
        return false;
    }

    const void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(Object);

    if (Property->IsA<FBoolProperty>())
    {
        OutValue = MakeShared<FJsonValueBoolean>(((FBoolProperty*)Property)->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (Property->IsA<FIntProperty>())
    {
        OutValue = MakeShared<FJsonValueNumber>(((FIntProperty*)Property)->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (Property->IsA<FFloatProperty>())
    {
        OutValue = MakeShared<FJsonValueNumber>(((FFloatProperty*)Property)->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (Property->IsA<FDoubleProperty>())
    {
        OutValue = MakeShared<FJsonValueNumber>(((FDoubleProperty*)Property)->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (Property->IsA<FStrProperty>())
    {
        OutValue = MakeShared<FJsonValueString>(((FStrProperty*)Property)->GetPropertyValue(PropertyAddr));
        return true;
    }
    else if (Property->IsA<FTextProperty>())
    {
        OutValue = MakeShared<FJsonValueString>(((FTextProperty*)Property)->GetPropertyValue(PropertyAddr).ToString());
        return true;
    }
    else if (Property->IsA<FStructProperty>())
    {
        FStructProperty* StructProp = CastField<FStructProperty>(Property);
        UScriptStruct* InnerStruct = StructProp->Struct;
        if (InnerStruct == TBaseStructure<FLinearColor>::Get() || InnerStruct == TBaseStructure<FColor>::Get())
        {
            TArray<TSharedPtr<FJsonValue>> ColorArr;
            if (InnerStruct == TBaseStructure<FLinearColor>::Get())
            {
                const FLinearColor* ColPtr = (const FLinearColor*)PropertyAddr;
                ColorArr.Add(MakeShared<FJsonValueNumber>(ColPtr->R));
                ColorArr.Add(MakeShared<FJsonValueNumber>(ColPtr->G));
                ColorArr.Add(MakeShared<FJsonValueNumber>(ColPtr->B));
                ColorArr.Add(MakeShared<FJsonValueNumber>(ColPtr->A));
            }
            else
            {
                const FColor* ColPtr = (const FColor*)PropertyAddr;
                ColorArr.Add(MakeShared<FJsonValueNumber>(ColPtr->R));
                ColorArr.Add(MakeShared<FJsonValueNumber>(ColPtr->G));
                ColorArr.Add(MakeShared<FJsonValueNumber>(ColPtr->B));
                ColorArr.Add(MakeShared<FJsonValueNumber>(ColPtr->A));
            }
            OutValue = MakeShared<FJsonValueArray>(ColorArr);
            return true;
        }
        return GetStructPropertyByPath(StructProp->Struct, PropertyAddr, TEXT(""), OutValue, OutErrorMessage);
    }
    else if (Property->IsA<FByteProperty>())
    {
        FByteProperty* ByteProp = CastField<FByteProperty>(Property);
        UEnum* EnumDef = ByteProp->GetIntPropertyEnum();
        uint8 ByteValue = ByteProp->GetPropertyValue(PropertyAddr);
        if (EnumDef)
        {
            OutValue = MakeShared<FJsonValueString>(EnumDef->GetNameStringByValue(ByteValue));
        }
        else
        {
            OutValue = MakeShared<FJsonValueNumber>(ByteValue);
        }
        return true;
    }
    else if (Property->IsA<FEnumProperty>())
    {
        FEnumProperty* EnumProp = CastField<FEnumProperty>(Property);
        UEnum* EnumDef = EnumProp->GetEnum();
        FNumericProperty* UnderlyingNumericProp = EnumProp->GetUnderlyingProperty();
        int64 EnumValue = UnderlyingNumericProp->GetSignedIntPropertyValue(PropertyAddr);
        if (EnumDef)
        {
            OutValue = MakeShared<FJsonValueString>(EnumDef->GetNameStringByValue(EnumValue));
        }
        else
        {
            OutValue = MakeShared<FJsonValueNumber>(EnumValue);
        }
        return true;
    }
    else if (Property->IsA<FObjectProperty>())
    {
        FObjectProperty* ObjProperty = CastField<FObjectProperty>(Property);
        UObject* Asset = ObjProperty->GetObjectPropertyValue(PropertyAddr);
        if (Asset)
        {
            OutValue = MakeShared<FJsonValueString>(Asset->GetPathName());
        }
        else
        {
            OutValue = MakeShared<FJsonValueNull>();
        }
        return true;
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported property type: %s for property %s"),
                                    *Property->GetClass()->GetName(), *PropertyName);
    return false;
}

bool FUnrealMCPCommonUtils::SetStructPropertyByPath(UScriptStruct* Struct, void* StructPtr, const FString& PropertyPath,
                                     const TSharedPtr<FJsonValue>& Value, FString& OutErrorMessage)
{
    if (!Struct || !StructPtr)
    {
        OutErrorMessage = TEXT("Invalid struct");
        return false;
    }

    // Dot-path: navigate into nested structs or object references inside this struct
    FString FirstSegment;
    FString Remainder;
    if (PropertyPath.Split(TEXT("."), &FirstSegment, &Remainder))
    {
        FProperty* SubProp = Struct->FindPropertyByName(*FirstSegment);
        if (!SubProp)
        {
            OutErrorMessage = FString::Printf(TEXT("Struct field not found: %s"), *FirstSegment);
            return false;
        }

        void* SubAddr = SubProp->ContainerPtrToValuePtr<void>(StructPtr);

        if (FStructProperty* SubStructProp = CastField<FStructProperty>(SubProp))
        {
            return SetStructPropertyByPath(SubStructProp->Struct, SubAddr, Remainder, Value, OutErrorMessage);
        }
        else if (FObjectProperty* SubObjectProp = CastField<FObjectProperty>(SubProp))
        {
            UObject* SubObject = SubObjectProp->GetObjectPropertyValue(SubAddr);
            if (!SubObject)
            {
                OutErrorMessage = FString::Printf(TEXT("Object reference is null: %s"), *FirstSegment);
                return false;
            }
            return FUnrealMCPCommonUtils::SetObjectProperty(SubObject, Remainder, Value, OutErrorMessage);
        }

        OutErrorMessage = FString::Printf(TEXT("Field '%s' is not a struct or object, cannot navigate further"), *FirstSegment);
        return false;
    }

    // Final segment: set the value on this struct
    FProperty* Property = Struct->FindPropertyByName(*PropertyPath);
    if (!Property)
    {
        OutErrorMessage = FString::Printf(TEXT("Struct field not found: %s"), *PropertyPath);
        return false;
    }

    void* PropertyAddr = Property->ContainerPtrToValuePtr<void>(StructPtr);

    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        BoolProp->SetPropertyValue(PropertyAddr, Value->AsBool());
        return true;
    }
    else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        IntProp->SetPropertyValue(PropertyAddr, static_cast<int32>(Value->AsNumber()));
        return true;
    }
    else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        FloatProp->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        DoubleProp->SetPropertyValue(PropertyAddr, Value->AsNumber());
        return true;
    }
    else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        StrProp->SetPropertyValue(PropertyAddr, Value->AsString());
        return true;
    }
    else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        UScriptStruct* InnerStruct = StructProp->Struct;
        const TArray<TSharedPtr<FJsonValue>>* Arr;

        if (InnerStruct == TBaseStructure<FVector>::Get())
        {
            if (Value->TryGetArray(Arr) && Arr->Num() >= 3)
            {
                FVector* VecPtr = (FVector*)PropertyAddr;
                VecPtr->X = (*Arr)[0]->AsNumber();
                VecPtr->Y = (*Arr)[1]->AsNumber();
                VecPtr->Z = (*Arr)[2]->AsNumber();
                return true;
            }
            OutErrorMessage = TEXT("Expected array of 3 numbers for FVector property");
            return false;
        }
        else if (InnerStruct == TBaseStructure<FRotator>::Get())
        {
            if (Value->TryGetArray(Arr) && Arr->Num() >= 3)
            {
                FRotator* RotPtr = (FRotator*)PropertyAddr;
                RotPtr->Pitch = (*Arr)[0]->AsNumber();
                RotPtr->Yaw = (*Arr)[1]->AsNumber();
                RotPtr->Roll = (*Arr)[2]->AsNumber();
                return true;
            }
            OutErrorMessage = TEXT("Expected array of 3 numbers for FRotator property");
            return false;
        }
        else if (InnerStruct == TBaseStructure<FLinearColor>::Get())
        {
            if (Value->TryGetArray(Arr) && Arr->Num() >= 3)
            {
                FLinearColor* ColPtr = (FLinearColor*)PropertyAddr;
                ColPtr->R = (*Arr)[0]->AsNumber();
                ColPtr->G = (*Arr)[1]->AsNumber();
                ColPtr->B = (*Arr)[2]->AsNumber();
                ColPtr->A = Arr->Num() >= 4 ? (*Arr)[3]->AsNumber() : 1.0f;
                return true;
            }
            OutErrorMessage = TEXT("Expected array of 3-4 numbers for FLinearColor property");
            return false;
        }

        OutErrorMessage = FString::Printf(TEXT("Unsupported nested struct type: %s for field %s"),
                                        *InnerStruct->GetName(), *PropertyPath);
        return false;
    }
    else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        UEnum* EnumDef = ByteProp->GetIntPropertyEnum();
        if (EnumDef && Value->Type == EJson::String)
        {
            FString EnumValueName = Value->AsString();
            if (EnumValueName.Contains(TEXT("::")))
            {
                EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
            }
            int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
            if (EnumValue == INDEX_NONE)
            {
                OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                return false;
            }
            ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(EnumValue));
            return true;
        }
        ByteProp->SetPropertyValue(PropertyAddr, static_cast<uint8>(Value->AsNumber()));
        return true;
    }
    else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        UEnum* EnumDef = EnumProp->GetEnum();
        FNumericProperty* UnderlyingNumericProp = EnumProp->GetUnderlyingProperty();
        if (EnumDef && UnderlyingNumericProp)
        {
            if (Value->Type == EJson::Number)
            {
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, static_cast<int64>(Value->AsNumber()));
                return true;
            }
            else if (Value->Type == EJson::String)
            {
                FString EnumValueName = Value->AsString();
                if (EnumValueName.Contains(TEXT("::")))
                {
                    EnumValueName.Split(TEXT("::"), nullptr, &EnumValueName);
                }
                int64 EnumValue = EnumDef->GetValueByNameString(EnumValueName);
                if (EnumValue == INDEX_NONE)
                {
                    OutErrorMessage = FString::Printf(TEXT("Could not find enum value for '%s'"), *EnumValueName);
                    return false;
                }
                UnderlyingNumericProp->SetIntPropertyValue(PropertyAddr, EnumValue);
                return true;
            }
        }
    }

    OutErrorMessage = FString::Printf(TEXT("Unsupported field type: %s for field %s"),
                                    *Property->GetClass()->GetName(), *PropertyPath);
    return false;
}

// =====================================================================
// Viewport utilities (crash #6: GetActiveViewport() == nullptr en
// instancia fresca -> deref de nullptr+0x40)
// =====================================================================
FLevelEditorViewportClient* FUnrealMCPCommonUtils::FindAnyLevelEditorViewportClient()
{
    auto TryGet = []() -> FLevelEditorViewportClient*
    {
        if (!GEditor)
        {
            return nullptr;
        }
        if (FViewport* ActiveVP = GEditor->GetActiveViewport())
        {
            if (FLevelEditorViewportClient* ActiveClient =
                    (FLevelEditorViewportClient*)ActiveVP->GetClient())
            {
                return ActiveClient;
            }
        }
        for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
        {
            if (Client)
            {
                return Client;
            }
        }
        return nullptr;
    };

    if (FLevelEditorViewportClient* Client = TryGet())
    {
        return Client;
    }

    // CRASH #7: NO invocar TryInvokeTab aqui. Llamarlo mientras el editor aun
    // esta inicializando (swapchain/layout en curso) provoca
    // "FlushRenderingCommands called recursively" -> assert SharedPointer.h:1128
    // -> SIGSEGV. El tab LevelEditor se instancia solo al terminar el arranque;
    // el caller debe reintentar hasta que aparezca.
    return nullptr;
}

FViewport* FUnrealMCPCommonUtils::GetAnyLevelEditorFViewport()
{
    if (GEditor)
    {
        if (FViewport* ActiveVP = GEditor->GetActiveViewport())
        {
            return ActiveVP;
        }
    }
    FLevelEditorViewportClient* Client = FindAnyLevelEditorViewportClient();
    if (Client)
    {
        TSharedPtr<SEditorViewport> Widget = Client->GetEditorViewportWidget();
        if (Widget.IsValid())
        {
            TSharedPtr<FSceneViewport> SceneVP = Widget->GetSceneViewport();
            if (SceneVP.IsValid())
            {
                return SceneVP.Get();
            }
        }
    }
    return nullptr;
}
