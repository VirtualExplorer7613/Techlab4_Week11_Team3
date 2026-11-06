#include "EnginePCH.h"
#include "ScoreboardManager.h"

#include "UI/ScoreboardWidget.h"
#include "UObject/UObjectHash.h"

#include <cmath>

AScoreboardManager::AScoreboardManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

AScoreboardManager* AScoreboardManager::Find(const UWorld* InWorld)
{
	// 에디터 월드와 PIE 월드에 각각 사본이 있으므로 월드로 구분한다.
	TArray<UObject*> Managers;
	GetObjectsOfClass(AScoreboardManager::StaticClass(), Managers);
	for (UObject* Object : Managers)
	{
		AScoreboardManager* Manager = static_cast<AScoreboardManager*>(Object);
		if (Manager->GetWorld() == InWorld)
			return Manager;
	}
	return nullptr;
}

void AScoreboardManager::BeginPlay()
{
	Super::BeginPlay();
	if (!HasBegunPlay()) return;   // 에디터 월드에서는 HUD를 만들지 않는다

	UUserWidget* Widget = CreateWidget(GetWorld(), WidgetAssetPath);
	Scoreboard = Cast<UScoreboardWidget>(Widget);
	if (!Scoreboard)
	{
		HTR_LOG(Error, "Scoreboard: '{}' is missing or its ParentClass is not UScoreboardWidget", WidgetAssetPath);
		if (Widget) Widget->Destroy();
		return;
	}
	// PIE를 끝낼 때 뷰포트가 위젯을 먼저 지운다. 그때 포인터를 비워 EndPlay에서 다시 지우지 않게 한다.
	Scoreboard->OnDestroyed = [this] { Scoreboard = nullptr; };
	Scoreboard->AddToViewport();

	Score = 0;
	bTimeUp = false;
	TimeLeft = TimeLimit > 0.0f ? TimeLimit : 0.0f;
	LastShownSeconds = -1;
	Scoreboard->SetScore(Score);
	UpdateTimeText();

	if (bStartTimerOnBeginPlay)
		StartTimer();
}

void AScoreboardManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bTimerRunning) return;

	if (TimeLimit > 0.0f)
	{
		TimeLeft = std::max(0.0f, TimeLeft - DeltaTime);
		if (TimeLeft <= 0.0f)
		{
			bTimerRunning = false;
			bTimeUp = true;
			UpdateTimeText();
			HTR_LOG(Info, "Scoreboard: time up, final score {}", Score);
			if (OnTimeUp) OnTimeUp();
			return;
		}
	}
	else
	{
		TimeLeft += DeltaTime;
	}
	UpdateTimeText();
}

void AScoreboardManager::EndPlay()
{
	Super::EndPlay();
	if (Scoreboard)
	{
		// 게임 중에 매니저만 지워지는 경우: 위젯도 함께 내린다.
		Scoreboard->OnDestroyed = nullptr;
		Scoreboard->Destroy();
		Scoreboard = nullptr;
	}
	bTimerRunning = false;
}

void AScoreboardManager::AddScore(int32 Amount)
{
	if (bTimeUp) return;   // 시간이 끝난 뒤의 점수는 받지 않는다
	SetScore(Score + Amount);
}

void AScoreboardManager::SetScore(int32 InScore)
{
	Score = std::max(0, InScore);
	if (Scoreboard)
		Scoreboard->SetScore(Score);
}

void AScoreboardManager::StartTimer()
{
	if (bTimeUp) return;
	bTimerRunning = true;
}

void AScoreboardManager::UpdateTimeText()
{
	// 카운트다운은 올림(2:00 → 1:59 순서), 경과 시간은 내림으로 표시한다.
	const int32 Seconds = TimeLimit > 0.0f
		? static_cast<int32>(std::ceil(TimeLeft))
		: static_cast<int32>(TimeLeft);
	if (Seconds == LastShownSeconds) return;
	LastShownSeconds = Seconds;
	if (Scoreboard)
		Scoreboard->SetTimeLeft(static_cast<float>(Seconds));
}
