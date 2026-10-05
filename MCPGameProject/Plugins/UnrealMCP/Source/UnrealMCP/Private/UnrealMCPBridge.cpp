#include "UnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPProjectCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Commands/UnrealMCPUMGCommands.h"
#include "Commands/UnrealMCPAudioCommands.h"
#include "Commands/UnrealMCPNiagaraCommands.h"
#include "Commands/UnrealMCPSequencerCommands.h"
#include "Commands/UnrealMCPLandscapeCommands.h"
#include "Commands/UnrealMCPAICommands.h"
#include "Commands/UnrealMCPComponentCommands.h"
#include "Commands/UnrealMCPMaterialNodeCommands.h"
#include "Commands/UnrealMCPViewportCommands.h"
#include "Commands/UnrealMCPRenderingCommands.h"
#include "Commands/UnrealMCPFoliageCommands.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UUnrealMCPBridge::UUnrealMCPBridge()
{
    EditorCommands = MakeShared<FUnrealMCPEditorCommands>();
    BlueprintCommands = MakeShared<FUnrealMCPBlueprintCommands>();
    BlueprintNodeCommands = MakeShared<FUnrealMCPBlueprintNodeCommands>();
    ProjectCommands = MakeShared<FUnrealMCPProjectCommands>();
    UMGCommands = MakeShared<FUnrealMCPUMGCommands>();
    AudioCommands = MakeShared<FUnrealMCPAudioCommands>();
    NiagaraCommands = MakeShared<FUnrealMCPNiagaraCommands>();
    SequencerCommands = MakeShared<FUnrealMCPSequencerCommands>();
    LandscapeCommands = MakeShared<FUnrealMCPLandscapeCommands>();
    AICommands = MakeShared<FUnrealMCPAICommands>();
    ComponentCommands = MakeShared<FUnrealMCPComponentCommands>();
    MaterialNodeCommands = MakeShared<FUnrealMCPMaterialNodeCommands>();
    ViewportCommands = MakeShared<FUnrealMCPViewportCommands>();
    RenderingCommands = MakeShared<FUnrealMCPRenderingCommands>();
    FoliageCommands = MakeShared<FUnrealMCPFoliageCommands>();
}

UUnrealMCPBridge::~UUnrealMCPBridge()
{
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintNodeCommands.Reset();
    ProjectCommands.Reset();
    UMGCommands.Reset();
    AudioCommands.Reset();
    NiagaraCommands.Reset();
    SequencerCommands.Reset();
    LandscapeCommands.Reset();
    AICommands.Reset();
    ComponentCommands.Reset();
    MaterialNodeCommands.Reset();
    ViewportCommands.Reset();
    RenderingCommands.Reset();
    FoliageCommands.Reset();
}

// Initialize subsystem
void UUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Shutting down"));
    StopServer();
}

// Start the MCP server
void UUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("UnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Server stopped"));
}

// Execute a command received from a client
FString UUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge: Executing command: %s"), *CommandType);
    
    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
        
        try
        {
            TSharedPtr<FJsonObject> ResultJson;
            
            if (CommandType == TEXT("ping"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
            }
            // Editor Commands (including actor manipulation)
            else if (CommandType == TEXT("get_actors_in_level") || 
                     CommandType == TEXT("find_actors_by_name") ||
                     CommandType == TEXT("spawn_actor") ||
                     CommandType == TEXT("create_actor") ||
                     CommandType == TEXT("delete_actor") || 
                     CommandType == TEXT("set_actor_transform") ||
                     CommandType == TEXT("get_actor_properties") ||
                     CommandType == TEXT("set_actor_property") ||
                     CommandType == TEXT("spawn_blueprint_actor") ||
                     CommandType == TEXT("focus_viewport") || 
                     CommandType == TEXT("take_screenshot") ||
                     CommandType == TEXT("set_actor_material") ||
                     CommandType == TEXT("create_material") ||
                     CommandType == TEXT("import_texture") ||
                     CommandType == TEXT("create_pbr_material") ||
                     CommandType == TEXT("get_material_properties") ||
                     CommandType == TEXT("set_material_blend_mode") ||
                     CommandType == TEXT("create_moss_stone_material") ||
                     CommandType == TEXT("spawn_foliage_instances") ||
                     CommandType == TEXT("get_actor_material") ||
                     CommandType == TEXT("create_dynamic_material_instance") ||
                     CommandType == TEXT("set_material_scalar_parameter") ||
                     CommandType == TEXT("set_material_vector_parameter") ||
                     CommandType == TEXT("duplicate_actor") ||
                     CommandType == TEXT("get_actor_bounds") ||
                     CommandType == TEXT("attach_actor_to_actor") ||
                     CommandType == TEXT("line_trace") ||
                     CommandType == TEXT("get_gravity") ||
                     CommandType == TEXT("set_gravity") ||
                     CommandType == TEXT("apply_force") ||
                     CommandType == TEXT("execute_console_command") ||
                     CommandType == TEXT("play_start") ||
                     CommandType == TEXT("play_stop") ||
                     CommandType == TEXT("play_status") ||
                     CommandType == TEXT("simulate_input") ||
                     CommandType == TEXT("list_functions") ||
                     CommandType == TEXT("call_actor_function"))
            {
                ResultJson = EditorCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Commands
            else if (CommandType == TEXT("create_blueprint") || 
                     CommandType == TEXT("add_component_to_blueprint") || 
                     CommandType == TEXT("set_component_property") || 
                     CommandType == TEXT("set_physics_properties") || 
                     CommandType == TEXT("compile_blueprint") || 
                     CommandType == TEXT("set_blueprint_property") || 
                     CommandType == TEXT("set_static_mesh_properties") ||
                     CommandType == TEXT("set_pawn_properties"))
            {
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Node Commands
            else if (CommandType == TEXT("connect_blueprint_nodes") || 
                     CommandType == TEXT("add_blueprint_get_self_component_reference") ||
                     CommandType == TEXT("add_blueprint_self_reference") ||
                     CommandType == TEXT("find_blueprint_nodes") ||
                     CommandType == TEXT("add_blueprint_event_node") ||
                     CommandType == TEXT("add_blueprint_input_action_node") ||
                     CommandType == TEXT("add_blueprint_function_node") ||
                     CommandType == TEXT("add_blueprint_get_component_node") ||
                     CommandType == TEXT("add_blueprint_variable") ||
                     CommandType == TEXT("add_blueprint_get_variable_node") ||
                     CommandType == TEXT("add_blueprint_set_variable_node"))
            {
                ResultJson = BlueprintNodeCommands->HandleCommand(CommandType, Params);
            }
            // Project Commands
            else if (CommandType == TEXT("create_input_mapping"))
            {
                ResultJson = ProjectCommands->HandleCommand(CommandType, Params);
            }
            // UMG Commands
            else if (CommandType == TEXT("create_umg_widget_blueprint") ||
                     CommandType == TEXT("add_text_block_to_widget") ||
                     CommandType == TEXT("add_button_to_widget") ||
                     CommandType == TEXT("bind_widget_event") ||
                     CommandType == TEXT("set_text_block_binding") ||
                     CommandType == TEXT("add_widget_to_viewport"))
            {
                ResultJson = UMGCommands->HandleCommand(CommandType, Params);
            }
            // Audio Commands
            else if (CommandType == TEXT("create_sound_attenuation") ||
                     CommandType == TEXT("create_sound_class") ||
                     CommandType == TEXT("create_sound_cue") ||
                     CommandType == TEXT("spawn_ambient_sound") ||
                     CommandType == TEXT("set_ambient_sound_properties") ||
                     CommandType == TEXT("play_sound_2d"))
            {
                ResultJson = AudioCommands->HandleCommand(CommandType, Params);
            }
            // Niagara Commands
            else if (CommandType == TEXT("spawn_niagara_system") ||
                     CommandType == TEXT("set_niagara_float_parameter") ||
                     CommandType == TEXT("set_niagara_vector_parameter") ||
                     CommandType == TEXT("set_niagara_color_parameter") ||
                     CommandType == TEXT("activate_niagara_component") ||
                     CommandType == TEXT("deactivate_niagara_component") ||
                     CommandType == TEXT("add_niagara_user_parameter") ||
                     CommandType == TEXT("list_niagara_user_parameters") ||
                     CommandType == TEXT("create_niagara_emitter") ||
                     CommandType == TEXT("trigger_niagara_event"))
            {
                ResultJson = NiagaraCommands->HandleCommand(CommandType, Params);
            }
            // Sequencer Commands
            else if (CommandType == TEXT("create_level_sequence") ||
                     CommandType == TEXT("add_actor_to_sequence") ||
                     CommandType == TEXT("add_camera_cut_track") ||
                     CommandType == TEXT("set_playback_range") ||
                     CommandType == TEXT("open_level_sequence") ||
                     CommandType == TEXT("add_transform_keyframe") ||
                     CommandType == TEXT("add_property_keyframe"))
            {
                ResultJson = SequencerCommands->HandleCommand(CommandType, Params);
            }
                        else if (CommandType == TEXT("get_landscape_info") ||
                     CommandType == TEXT("sculpt_landscape_region") ||
                     CommandType == TEXT("paint_landscape_layer") ||
                     CommandType == TEXT("create_landscape_layer_info") ||
                     CommandType == TEXT("add_landscape_material_layer_blend_input") ||
                     CommandType == TEXT("create_landscape_material_with_layer_blend") ||
                     CommandType == TEXT("set_landscape_material"))
            {
                ResultJson = LandscapeCommands->HandleCommand(CommandType, Params);
            }
            // AI Commands (NavMesh + Behavior Tree)
            else if (CommandType == TEXT("list_components") ||
                     CommandType == TEXT("get_component_property") ||
                     CommandType == TEXT("set_actor_component_property"))
            {
                ResultJson = ComponentCommands->HandleCommand(CommandType, Params);
            }
            else if (CommandType == TEXT("add_material_expression") ||
                     CommandType == TEXT("connect_material_expressions") ||
                     CommandType == TEXT("set_material_output") ||
                     CommandType == TEXT("list_material_expressions") ||
                     CommandType == TEXT("set_material_expression_constant") ||
                     CommandType == TEXT("list_available_expression_types"))
            {
                ResultJson = MaterialNodeCommands->HandleCommand(CommandType, Params);
            }
            else if (CommandType == TEXT("get_viewport_camera_info") ||
                     CommandType == TEXT("set_viewport_camera") ||
                     CommandType == TEXT("set_viewport_fov") ||
                     CommandType == TEXT("set_viewport_view_mode"))
            {
                ResultJson = ViewportCommands->HandleCommand(CommandType, Params);
            }
            else if (CommandType == TEXT("get_cvar") ||
                     CommandType == TEXT("set_cvar"))
            {
                ResultJson = RenderingCommands->HandleCommand(CommandType, Params);
            }
            else if (CommandType == TEXT("create_foliage_type") ||
                     CommandType == TEXT("add_foliage_instances") ||
                     CommandType == TEXT("remove_foliage_instances") ||
                     CommandType == TEXT("list_foliage_types"))
            {
                ResultJson = FoliageCommands->HandleCommand(CommandType, Params);
            }
            else if (CommandType == TEXT("get_navmesh_info") ||
                     CommandType == TEXT("build_navigation") ||
                     CommandType == TEXT("find_path") ||
                     CommandType == TEXT("create_behavior_tree") ||
                     CommandType == TEXT("create_blackboard") ||
                     CommandType == TEXT("add_blackboard_key") ||
                     CommandType == TEXT("run_behavior_tree_on_actor"))
            {
                ResultJson = AICommands->HandleCommand(CommandType, Params);
            }

            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
                
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }
            
            // Check if the result contains an error
            bool bSuccess = true;
            FString ErrorMessage;
            
            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess)
                {
                    if (ResultJson->HasField(TEXT("error")))
                    {
                        ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                    }
                    else if (ResultJson->HasField(TEXT("message")))
                    {
                        ErrorMessage = ResultJson->GetStringField(TEXT("message"));
                    }
                }
            }
            
            if (bSuccess)
            {
                // Set success status and include the result
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                // Set error status and include the error message
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });
    
    const double CommandTimeoutSeconds = 10.0;
    if (Future.WaitFor(FTimespan::FromSeconds(CommandTimeoutSeconds)))
    {
        return Future.Get();
    }

    UE_LOG(LogTemp, Error, TEXT("UnrealMCPBridge: Command '%s' timed out waiting for GameThread after %.1fs (GameThread likely busy)"), *CommandType, CommandTimeoutSeconds);

    TSharedPtr<FJsonObject> TimeoutResponse = MakeShareable(new FJsonObject);
    TimeoutResponse->SetStringField(TEXT("status"), TEXT("error"));
    TimeoutResponse->SetStringField(TEXT("error"), FString::Printf(TEXT("Command '%s' timed out waiting for GameThread (busy with shader compilation, asset loading, or a modal dialog)"), *CommandType));
    FString TimeoutString;
    TSharedRef<TJsonWriter<>> TimeoutWriter = TJsonWriterFactory<>::Create(&TimeoutString);
    FJsonSerializer::Serialize(TimeoutResponse.ToSharedRef(), TimeoutWriter);
    return TimeoutString;
}
