#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UFoliageType;
class UStaticMesh;
class AInstancedFoliageActor;

// =====================================================================
// FUnrealMCPFoliageCommands
//
// Pintado/gestion de foliage (instancias de UFoliageType_InstancedStaticMesh)
// sobre el AInstancedFoliageActor del nivel actual. No usa el brush
// interactivo del FoliageEdMode -- las transforms (location/rotation/scale)
// las das vos directamente via los parametros del comando.
//
// create_foliage_type crea y guarda a disco un asset UFoliageType_InstancedStaticMesh
// que envuelve un UStaticMesh existente.
// add_foliage_instances agrega instancias de ese tipo en el
// AInstancedFoliageActor del nivel actual (se crea el actor si no existe).
// remove_foliage_instances borra instancias por indice.
// list_foliage_types enumera los tipos ya pintados en el nivel actual con
// su conteo de instancias.
// =====================================================================
class UNREALMCP_API FUnrealMCPFoliageCommands
{
public:
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

    // Stateless: accesible desde otros comandos (p. ej. spawn_foliage_instances
    // en UnrealMCPEditorCommands) para evitar el ensure de IsLevelPartition().
    static AInstancedFoliageActor* GetOrCreateFoliageActor(FString& OutError);

private:
    TSharedPtr<FJsonObject> HandleCreateFoliageType(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddFoliageInstances(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveFoliageInstances(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListFoliageTypes(const TSharedPtr<FJsonObject>& Params);

    UFoliageType* LoadFoliageTypeAsset(const FString& FoliageTypePath, FString& OutError);
};
