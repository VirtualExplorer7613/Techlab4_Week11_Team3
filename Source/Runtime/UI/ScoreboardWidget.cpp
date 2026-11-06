#include "EnginePCH.h"
#include "ScoreboardWidget.h"

#include "UI/TextBlock.h"

#include <format>

void UScoreboardWidget::NativeConstruct()
{
	ScoreText = GetWidget<UTextBlock>("ScoreText");
	TimeText = GetWidget<UTextBlock>("TimeText");
	if (!ScoreText || !TimeText)
		HTR_LOG(Warning, "Scoreboard widget asset {} needs TextBlocks named ScoreText and TimeText", AssetPath);
	SetScore(0);
	SetTimeLeft(0.0f);
}

void UScoreboardWidget::SetScore(int32 InScore)
{
	if (ScoreText) ScoreText->SetText(std::format("{:05}", InScore));
}

void UScoreboardWidget::SetTimeLeft(float Seconds)
{
	const int32 Total = static_cast<int32>(Seconds);
	if (TimeText) TimeText->SetText(std::format("{:02}:{:02}", Total / 60, Total % 60));
}