#pragma once

#include "GameFramework/Actor.h"

#include <functional>
#include "ScoreboardManager.generated.h"

class UScoreboardWidget;

// 점수와 제한 시간을 관리하고 점수판 HUD에 표시한다.
// 레벨에 하나 배치해 두면 PIE 시작 시 점수판을 띄운다.
UCLASS()
class AScoreboardManager : public AActor
{
	GENERATED_BODY()


public:
	AScoreboardManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay() override;

	// 같은 월드에 배치된 매니저를 찾는다. 점수를 올리는 액터가 쓴다.
	static AScoreboardManager* Find(const UWorld* InWorld);

	void AddScore(int32 Amount);
	void SetScore(int32 InScore);
	void ResetScore() { SetScore(0); }
	int32 GetScore() const { return Score; }

	// TimeLimit > 0이면 남은 시간을 세고, 0 이하면 경과 시간을 센다.
	void StartTimer();
	void StopTimer() { bTimerRunning = false; }
	bool IsTimerRunning() const { return bTimerRunning; }
	bool IsTimeUp() const { return bTimeUp; }
	float GetTimeLeft() const { return TimeLeft; }

	// 제한 시간이 끝나는 순간 한 번 불린다.
	std::function<void()> OnTimeUp;

	// UI 에디터에서 만든 점수판 에셋. ParentClass가 UScoreboardWidget이어야 한다.
	UPROPERTY(EditAnywhere)
	FString WidgetAssetPath = "Assets/UI/Scoreboard.widget";
	UPROPERTY(EditAnywhere)
	float TimeLimit = 120.0f;                // 초
	UPROPERTY(EditAnywhere)
	bool bStartTimerOnBeginPlay = true;

private:
	void UpdateTimeText();

	UPROPERTY(VisibleAnywhere, Transient)
	int32 Score = 0;
	UPROPERTY(VisibleAnywhere, Transient)
	float TimeLeft = 0.0f;                   // 카운트다운이면 남은 시간, 아니면 경과 시간
	bool bTimerRunning = false;
	bool bTimeUp = false;
	int32 LastShownSeconds = -1;             // 초가 바뀔 때만 글자를 갱신한다

	UScoreboardWidget* Scoreboard = nullptr;
};
