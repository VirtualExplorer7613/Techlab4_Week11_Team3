#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "GameInstance.generated.h"

class UWorld;
class UEngine;
struct FWorldContext;

UCLASS()
class UGameInstance : public UObject
{
	GENERATED_BODY()
public:
	virtual void Init() {}
	virtual void Shutdown();
	UWorld* GetWorld() const;
	FWorldContext* GetWorldContext() const { return WorldContext; }
	
	void SetWorldContext(FWorldContext* Context);
	virtual bool InitializeForPlayInEditor(int32 PIEInstance);
	virtual bool StartPlayInEditorGameInstance();

	// 패키징된 게임: 에디터 월드를 복제하지 않고 씬 파일에서 Game 월드를 만든다.
	virtual bool InitializeStandalone(const FString& MapPath);
	// 게임 뷰포트를 만들고 월드 BeginPlay를 호출한다. PIE와 패키징 게임이 같이 쓴다.
	virtual bool StartGameInstance();

	void SetValue(const FString& Key, double Value) { Values.FindOrAdd(Key) = Value; }
	double GetValue(const FString& Key, double Default = 0.0) const
	{
		const double* Found = Values.Find(Key);
		return Found ? *Found : Default;
	}
	
	UEngine* GetEngine() const;
private:
	FWorldContext* WorldContext = nullptr;
	TMap<FString, double> Values;
};
