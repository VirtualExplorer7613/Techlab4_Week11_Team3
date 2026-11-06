#pragma once

#include "UI/UserWidget.h"
#include "ScoreboardWidget.generated.h"

class UTextBlock;

UCLASS()
class UScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetScore(int32 InScore);
	void SetTimeLeft(float Seconds);

protected:
	// 배치는 Assets/UI/Scoreboard.widget에서 온다. 여기서는 이름으로 위젯만 찾는다.
	virtual void NativeConstruct() override;

private:
	UTextBlock* ScoreText = nullptr;
	UTextBlock* TimeText = nullptr;
};
