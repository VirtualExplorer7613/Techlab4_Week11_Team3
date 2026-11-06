#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "MovieScene/MovieSceneFrameTime.h"
#include "MovieSceneTracks/MovieSceneEventTrack.h"
#include <functional>

class UWorld;
class UObject;
class UCameraComponent;
class ULevelSequence;
class ULevelSequencePlayer;
class UMovieScene;
class UMovieSceneTrack;
class UMovieSceneSection;
struct FMovieSceneFloatChannel;

// UE Sequencer 에디터(FSequencer + SSequencer)의 축소판.
// 편집 중인 ULevelSequence 하나와 미리보기용 ULevelSequencePlayer 하나를 가진다.
class FSequencerPanel : public IEditorPanel
{
public:
	FSequencerPanel();
	~FSequencerPanel() override;

	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Sequencer"; }

	void SetWorld(UWorld* InWorld, bool bIsEditorWorld);
	// 월드와 UObject 일괄 삭제 전에 시퀀스와 플레이어를 지운다 (UEditorEngine::PreExit)
	void Shutdown();
	// 에디터 월드에서 Camera Cut 카메라를 뷰포트에 태우는 방법 (EditorEngine이 넣어 준다)
	void SetPilotCameraHandler(std::function<void(UCameraComponent*)> Handler) { PilotCamera = std::move(Handler); }

private:
	enum class ERowType : uint8 { Binding, Track, Channel };
	struct FRow
	{
		ERowType Type = ERowType::Track;
		FGuid Binding;                       // Binding 줄, 또는 바인딩 아래 트랙/채널
		UMovieSceneTrack* Track = nullptr;
		int32 Channel = -1;
	};

	void DrawToolbar();
	void DrawAddActorPopup();
	void DrawAddTrackPopup();
	void DrawTimeline();
	void DrawDetails();
	void DrawChannelDetails(UMovieSceneSection& Section, FMovieSceneFloatChannel& Channel);
	void DrawCurvePreview(const FMovieSceneFloatChannel& Channel);
	void DrawCameraCutDetails();
	void DrawEventDetails(UMovieSceneSection& Section);
	void DrawSectionDetails(UMovieSceneSection& Section);

	void BuildRows(TArray<FRow>& OutRows) const;
	void NewSequence();
	void ResetPlayer();
	void KeySelectedTrack();
	void ClearSelection();
	UObject* ResolveBinding(const FGuid& Guid) const;
	UMovieSceneSection* GetKeySection(UMovieSceneTrack* Track) const;
	UMovieScene* GetMovieScene() const;

	float TimeToX(float Seconds, float OriginX) const { return OriginX + (Seconds - ViewStart) * PixelsPerSecond; }
	float XToTime(float X, float OriginX) const { return ViewStart + (X - OriginX) / PixelsPerSecond; }
	float SnapSeconds(float Seconds) const;
	float FrameToSeconds(FFrameNumber Frame) const;
	FFrameNumber SecondsToFrame(float Seconds) const;
	float GetLengthSeconds() const;
	float GetPlayheadSeconds() const;

	ULevelSequence* Sequence = nullptr;
	ULevelSequencePlayer* Player = nullptr;
	UWorld* World = nullptr;
	bool bWorldIsEditor = true;
	std::function<void(UCameraComponent*)> PilotCamera;

	float PixelsPerSecond = 120.0f;
	float ViewStart = 0.0f;
	bool bSnap = true;

	UMovieSceneTrack* SelectedTrack = nullptr;
	FGuid SelectedBinding;
	int32 SelectedChannel = -1;
	int32 SelectedKey = -1;

	FString AddActorName;
	char PropertyNameBuffer[64] = "FieldOfView";
	FGuid NewCutCamera;
	FMovieSceneEvent NewEvent;
	char AssetPath[260] = "Assets/Sequences/Demo.sequence";
};
