#pragma once

#include "MovieScene/MovieSceneTrack.h"
#include "MovieSceneCameraCutTrack.generated.h"

// UE FMovieSceneObjectBindingID 축소판 (같은 시퀀스 안의 바인딩만)
struct FMovieSceneObjectBindingID
{
	FGuid Guid;

	FMovieSceneObjectBindingID() = default;
	explicit FMovieSceneObjectBindingID(const FGuid& InGuid) : Guid(InGuid) {}
	bool IsValid() const { return Guid.IsValid(); }
	const FGuid& GetGuid() const { return Guid; }
};

// UE UMovieSceneCameraCutSection: 구간 동안 보여 줄 카메라 바인딩
UCLASS()
class UMovieSceneCameraCutSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneCameraCutSection() = default;

	const FMovieSceneObjectBindingID& GetCameraBindingID() const { return CameraBindingID; }
	void SetCameraBindingID(const FMovieSceneObjectBindingID& InCameraBindingID) { CameraBindingID = InCameraBindingID; }

	void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject) override;
	void SaveJson(nlohmann::json& Out) const override;
	void LoadJson(const nlohmann::json& In) override;

private:
	FMovieSceneObjectBindingID CameraBindingID;
};

// UE UMovieSceneCameraCutTrack. 섹션은 겹치지 않고 이어 붙는다 (bAutoArrangeSections)
UCLASS()
class UMovieSceneCameraCutTrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	UMovieSceneCameraCutTrack() = default;

	const char* GetDisplayName() const override { return "Camera Cuts"; }
	bool SupportsMultipleSections() const override { return true; }
	UMovieSceneSection* CreateNewSection() override;

	// UE AddNewCameraCut: StartTime부터 다음 컷(또는 재생 범위 끝)까지
	UMovieSceneCameraCutSection* AddNewCameraCut(const FMovieSceneObjectBindingID& CameraBindingID, FFrameNumber StartTime);
	// 시작 시간순 정렬 후 각 섹션 끝을 다음 섹션 시작에 맞춘다
	void RearrangeAllSections();

	void SaveJson(nlohmann::json& Out) const override;
	void LoadJson(const nlohmann::json& In) override;

	// UE bCanBlend 기본 false. 켜면 컷 사이 블렌드 시간을 플레이어에 넘긴다 (PCM 연결 후 사용)
	bool bCanBlend = false;
	float BlendTime = 0.5f;
};
