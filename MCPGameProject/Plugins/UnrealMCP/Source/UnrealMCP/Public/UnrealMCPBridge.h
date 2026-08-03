#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPBlueprintCommands.h"
#include "Commands/UnrealMCPBlueprintNodeCommands.h"
#include "Commands/UnrealMCPProjectCommands.h"
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
#include "UnrealMCPBridge.generated.h"

class FMCPServerRunnable;

/**
 * Editor subsystem for MCP Bridge
 * Handles communication between external tools and the Unreal Editor
 * through a TCP socket connection. Commands are received as JSON and
 * routed to appropriate command handlers.
 */
UCLASS()
class UNREALMCP_API UUnrealMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UUnrealMCPBridge();
	virtual ~UUnrealMCPBridge();

	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server functions
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	// Command execution
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Command handler instances
	TSharedPtr<FUnrealMCPEditorCommands> EditorCommands;
	TSharedPtr<FUnrealMCPBlueprintCommands> BlueprintCommands;
	TSharedPtr<FUnrealMCPBlueprintNodeCommands> BlueprintNodeCommands;
	TSharedPtr<FUnrealMCPProjectCommands> ProjectCommands;
	TSharedPtr<FUnrealMCPUMGCommands> UMGCommands;
	TSharedPtr<FUnrealMCPAudioCommands> AudioCommands;
	TSharedPtr<FUnrealMCPNiagaraCommands> NiagaraCommands;
	TSharedPtr<FUnrealMCPSequencerCommands> SequencerCommands;
    TSharedPtr<FUnrealMCPLandscapeCommands> LandscapeCommands;
    TSharedPtr<FUnrealMCPAICommands> AICommands;
    TSharedPtr<FUnrealMCPComponentCommands> ComponentCommands;
    TSharedPtr<FUnrealMCPMaterialNodeCommands> MaterialNodeCommands;
    TSharedPtr<FUnrealMCPViewportCommands> ViewportCommands;
    TSharedPtr<FUnrealMCPRenderingCommands> RenderingCommands;
    TSharedPtr<FUnrealMCPFoliageCommands> FoliageCommands;
}; 