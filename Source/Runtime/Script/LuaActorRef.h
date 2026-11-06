#pragma once

#include "Core/Types.h"

class AActor;
class UWorld;
class UActorComponent;

struct FLuaActorRef
{
	UWorld* World = nullptr;
	AActor* Ptr = nullptr;
	uint32 UUID = 0;

	static FLuaActorRef Make(AActor* Actor);

	// 살아 있으면 액터, 파괴됐으면 nullptr
	AActor* Get() const;
};

struct FLuaComponentRef
{
	UWorld* World = nullptr;
	AActor* Owner = nullptr;
	UActorComponent* Ptr = nullptr;
	uint32 UUID = 0;
	static FLuaComponentRef Make(UActorComponent* Component);
	// 살아 있으면 컴포넌트, 파괴됐으면 nullptr
	UActorComponent* Get() const;

};

AActor& Deref(const FLuaActorRef& Ref);
UActorComponent& Deref(const FLuaComponentRef& Ref);