// Copyright 2018-2023 - Roberto De Ioris

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/GCObject.h"
#include "LuaState.h"
#include "HAL/IConsoleManager.h"

DECLARE_MULTICAST_DELEGATE(FOnRegisteredLuaStatesChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewLuaState, ULuaState*);

class LUAMACHINE_API FLuaMachineModule : public IModuleInterface, public FGCObject, public FSelfRegisteringExec
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FLuaMachineModule& Get();

	void CleanupLuaStates(bool bIsSimulating);
	void UnregisterLuaState(ULuaState* LuaState);
	/**
	 * Select an existing state instance for class-based Blueprint dispatch.
	 *
	 * LuaMachine historically creates one state per class. Runtime systems that
	 * retain multiple isolated instances (for example, persistent game boards)
	 * can use this to switch which instance class-based nodes address without
	 * destroying the inactive states.
	 */
	FORCEINLINE bool ActivateLuaState(ULuaState* LuaState)
	{
		if (!IsValid(LuaState) || LuaState->HasAnyFlags(RF_ClassDefaultObject)
			|| LuaState->GetClass() == ULuaState::StaticClass())
		{
			return false;
		}

		const TSubclassOf<ULuaState> LuaStateClass = LuaState->GetClass();
		if (LuaStates.FindRef(LuaStateClass) == LuaState)
		{
			return true;
		}
		LuaStates.Add(LuaStateClass, LuaState);
		OnRegisteredLuaStatesChanged.Broadcast();
		return true;
	}

	ULuaState* GetLuaState(TSubclassOf<ULuaState> LuaStateClass, UWorld* InWorld, bool bCheckOnly=false);

	TArray<ULuaState*> GetRegisteredLuaStates();

	FOnNewLuaState OnNewLuaState;
	FOnRegisteredLuaStatesChanged OnRegisteredLuaStatesChanged;

	void LuaLevelAddedToWorld(ULevel* Level, UWorld* World);
	void LuaLevelRemovedFromWorld(ULevel* Level, UWorld* World);

	void AddReferencedObjects(FReferenceCollector& Collector) override;

	void RegisterLuaConsoleCommand(const FString& CommandName, const FLuaValue& LuaConsoleCommand);
	void UnregisterLuaConsoleCommand(const FString& CommandName);

	void RunLuaConsoleCommand(const TArray<FString>& Args, FLuaValue LuaConsoleCommand);

#if ENGINE_MAJOR_VERSION > 4
	virtual FString GetReferencerName() const override
	{
		return TEXT("FLuaMachineModule");
	}
#endif

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar);

private:
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
	TMap<TSubclassOf<ULuaState>, TObjectPtr<ULuaState>> LuaStates;
#else
	TMap<TSubclassOf<ULuaState>, ULuaState*> LuaStates;
#endif
	TSet<FString> LuaConsoleCommands;
};
