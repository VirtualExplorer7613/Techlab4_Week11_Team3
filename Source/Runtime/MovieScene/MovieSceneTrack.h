#pragma once

#include "UObject/Object.h"
#include "MovieScene/MovieSceneSection.h"
#include "MovieSceneTrack.generated.h"

// UE UMovieSceneTrack. 섹션 목록을 가지고, 현재 시간이 들어간 섹션을 평가한다.
UCLASS()
class UMovieSceneTrack : public UObject
{
	GENERATED_BODY()

public:
	UMovieSceneTrack() = default;
	~UMovieSceneTrack() override;

	virtual const char* GetDisplayName() const { return GetClass()->Name.c_str(); }
	// 이 트랙이 바인딩 아래에 붙는지(Transform, Property) 전역인지(CameraCut, Fade...)
	virtual bool IsObjectBindingTrack() const { return false; }
	// 같은 시간에 여러 섹션이 겹쳐도 되는지. CameraCut은 섹션이 이어 붙는다
	virtual bool SupportsMultipleSections() const { return false; }

	// UE UMovieSceneTrack::CreateNewSection. 트랙 종류에 맞는 섹션 하나를 만든다 (아직 추가하지 않음)
	virtual UMovieSceneSection* CreateNewSection() { return nullptr; }

	void AddSection(UMovieSceneSection* Section);
	void RemoveSection(UMovieSceneSection* Section);
	// 섹션 소멸자가 부른다. 목록에서만 뺀다
	void OnSectionDestroyed(UMovieSceneSection* Section);
	const TArray<UMovieSceneSection*>& GetAllSections() const { return Sections; }
	bool IsEmpty() const { return Sections.IsEmpty(); }

	// 기본: 범위에 들어간 활성 섹션 전부 평가
	virtual void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject);

	virtual void SaveJson(nlohmann::json& Out) const;
	virtual void LoadJson(const nlohmann::json& In);

	// UE bIsEvalDisabled (시퀀서의 Mute)
	bool bIsEvalDisabled = false;
	// 패널에서 채널 줄 펼침
	bool bExpanded = true;

protected:
	TArray<UMovieSceneSection*> Sections;
};

// 이름(UClass::Name)으로 트랙/섹션 생성. JSON 로드에 사용
UMovieSceneTrack* NewMovieSceneTrack(UObject* Outer, const FString& ClassName);
