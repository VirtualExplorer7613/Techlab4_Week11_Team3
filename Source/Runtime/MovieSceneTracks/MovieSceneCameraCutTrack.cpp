#include "EnginePCH.h"
#include "MovieSceneTracks/MovieSceneCameraCutTrack.h"

#include "MovieScene/MovieScene.h"
#include "UObject/UObjectGlobals.h"
#include <algorithm>

void UMovieSceneCameraCutSection::Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject)
{
	if (!CameraBindingID.IsValid()) return;
	const UMovieSceneCameraCutTrack* Track = Cast<UMovieSceneCameraCutTrack>(GetOuter());
	const float Blend = (Track && Track->bCanBlend) ? Track->BlendTime : 0.0f;
	Player.SetCameraCutCandidate(Player.FindBoundObject(CameraBindingID.GetGuid()), Blend);
}

void UMovieSceneCameraCutSection::SaveJson(nlohmann::json& Out) const
{
	Super::SaveJson(Out);
	Out["Camera"] = CameraBindingID.GetGuid().ToString();
}

void UMovieSceneCameraCutSection::LoadJson(const nlohmann::json& In)
{
	Super::LoadJson(In);
	FGuid Guid;
	FGuid::Parse(In.value("Camera", FString()), Guid);
	CameraBindingID = FMovieSceneObjectBindingID(Guid);
}

UMovieSceneSection* UMovieSceneCameraCutTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCameraCutSection>(this);
}

UMovieSceneCameraCutSection* UMovieSceneCameraCutTrack::AddNewCameraCut(const FMovieSceneObjectBindingID& CameraBindingID, FFrameNumber StartTime)
{
	// 같은 시작 프레임 컷이 있으면 카메라만 바꾼다
	for (UMovieSceneSection* Section : Sections)
	{
		UMovieSceneCameraCutSection* Cut = static_cast<UMovieSceneCameraCutSection*>(Section);
		if (Cut->GetInclusiveStartFrame() == StartTime)
		{
			Cut->SetCameraBindingID(CameraBindingID);
			return Cut;
		}
	}

	UMovieSceneCameraCutSection* NewSection = static_cast<UMovieSceneCameraCutSection*>(CreateNewSection());
	NewSection->SetCameraBindingID(CameraBindingID);
	NewSection->SetStartFrame(StartTime);
	AddSection(NewSection);
	RearrangeAllSections();
	return NewSection;
}

void UMovieSceneCameraCutTrack::RearrangeAllSections()
{
	std::sort(Sections.begin(), Sections.end(), [](const UMovieSceneSection* A, const UMovieSceneSection* B)
	{
		return A->GetInclusiveStartFrame() < B->GetInclusiveStartFrame();
	});

	// 마지막 컷은 끝을 열어 둬서 재생 범위 마지막 프레임에서도 유지되게 한다
	for (int32 Index = 0; Index < Sections.Num(); ++Index)
	{
		if (Index + 1 == Sections.Num())
		{
			Sections[Index]->SetEndFrameOpen();
			continue;
		}
		FFrameNumber End = Sections[Index + 1]->GetInclusiveStartFrame();
		if (End <= Sections[Index]->GetInclusiveStartFrame()) End = Sections[Index]->GetInclusiveStartFrame() + FFrameNumber(1);
		Sections[Index]->SetEndFrame(End);
	}
}

void UMovieSceneCameraCutTrack::SaveJson(nlohmann::json& Out) const
{
	Super::SaveJson(Out);
	Out["CanBlend"] = bCanBlend;
	Out["BlendTime"] = BlendTime;
}

void UMovieSceneCameraCutTrack::LoadJson(const nlohmann::json& In)
{
	Super::LoadJson(In);
	bCanBlend = In.value("CanBlend", false);
	BlendTime = In.value("BlendTime", 0.5f);
}
