#include "EnginePCH.h"

#include "Launch/EntryPoint.h"
#include "Engine/GameEngine.h"

UClass* GetEngineClass()
{
	return UGameEngine::StaticClass();
}