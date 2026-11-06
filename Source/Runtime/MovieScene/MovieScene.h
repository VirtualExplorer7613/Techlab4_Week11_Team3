#pragma once

#include "UObject/Object.h"
#include "MovieScene/MovieSceneTrack.h"
#include "MovieScene.generated.h"

class UClass;

// UE FMovieScenePossessable: 레벨에 이미 있는 오브젝트를 시퀀스가 "소유하지 않고" 움직이는 바인딩
struct FMovieScenePossessable
{
	FGuid Guid;
	FString Name;
	FString PossessedObjectClass;   // UE는 UClass 포인터. 표시용 클래스 이름
};

// UE FMovieSceneBinding: 바인딩 하나와 그 아래 트랙들
struct FMovieSceneBinding
{
	FGuid ObjectGuid;
	TArray<UMovieSceneTrack*> Tracks;

	const FGuid& GetObjectGuid() const { return ObjectGuid; }
	const TArray<UMovieSceneTrack*>& GetTracks() const { return Tracks; }
};

// UE UMovieScene: 시퀀스의 실제 데이터 (바인딩, 트랙, 재생 범위, 프레임레이트)
UCLASS()
class UMovieScene : public UObject
{
	GENERATED_BODY()

public:
	UMovieScene();
	~UMovieScene() override;

	// 바인딩
	FGuid AddPossessable(const FString& Name, UClass* Class);
	bool RemovePossessable(const FGuid& PossessableGuid);
	FMovieScenePossessable* FindPossessable(const FGuid& Guid);
	const TArray<FMovieScenePossessable>& GetPossessables() const { return Possessables; }
	FMovieSceneBinding* FindBinding(const FGuid& ForGuid);
	const TArray<FMovieSceneBinding>& GetBindings() const { return ObjectBindings; }

	// 오브젝트 바인딩 트랙 (UE AddTrack(TrackClass, ObjectGuid))
	UMovieSceneTrack* AddTrack(UClass* TrackClass, const FGuid& ObjectGuid);
	template <typename TrackClass>
	TrackClass* AddTrack(const FGuid& ObjectGuid) { return static_cast<TrackClass*>(AddTrack(TrackClass::StaticClass(), ObjectGuid)); }
	bool RemoveTrack(UMovieSceneTrack* Track);
	// 트랙 소멸자가 부른다. 목록에서만 뺀다
	void OnTrackDestroyed(UMovieSceneTrack* Track);

	// 전역 트랙 (UE5 AddTrack(TrackClass), 예전 이름 AddMasterTrack)
	UMovieSceneTrack* AddTrack(UClass* TrackClass);
	template <typename TrackClass>
	TrackClass* AddTrack() { return static_cast<TrackClass*>(AddTrack(TrackClass::StaticClass())); }
	const TArray<UMovieSceneTrack*>& GetTracks() const { return Tracks; }
	UMovieSceneTrack* FindTrack(UClass* TrackClass) const;

	// 카메라 컷 트랙은 따로 하나만 (UE CameraCutTrack)
	UMovieSceneTrack* AddCameraCutTrack(UClass* TrackClass);
	UMovieSceneTrack* GetCameraCutTrack() const { return CameraCutTrack; }
	void RemoveCameraCutTrack();

	const FMovieSceneFrameRange& GetPlaybackRange() const { return PlaybackRange; }
	void SetPlaybackRange(FFrameNumber Start, int32 Duration);
	FFrameRate GetTickResolution() const { return TickResolution; }
	FFrameRate GetDisplayRate() const { return DisplayRate; }
	void SetDisplayRate(FFrameRate InDisplayRate) { DisplayRate = InDisplayRate; }

	void SaveJson(nlohmann::json& Out) const;
	void LoadJson(const nlohmann::json& In);
	void Reset();

private:
	TArray<FMovieScenePossessable> Possessables;
	TArray<FMovieSceneBinding> ObjectBindings;
	TArray<UMovieSceneTrack*> Tracks;
	UMovieSceneTrack* CameraCutTrack = nullptr;

	FMovieSceneFrameRange PlaybackRange;
	// UE LevelSequenceProjectSettings 기본값: TickResolution 24000fps, DisplayRate 30fps
	FFrameRate TickResolution{ 24000, 1 };
	FFrameRate DisplayRate{ 30, 1 };
};
