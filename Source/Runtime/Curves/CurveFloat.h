#pragma once

#include "Containers/Array.h"
#include "UObject/Class.h"
#include "CurveFloat.generated.h"

// 두 키 사이의 값을 계산할 보간 방식을 선택합니다.
// Constant는 값 유지, Linear는 직선, Cubic은 접선을 이용한 곡선을 의미합니다.
enum class ECurveInterpMode : uint8
{
    Constant,
    Linear,
    Cubic,
};

// 커브의 한 키에 대한 시간·값·보간 방식·접선을 담습니다.
// 안정적인 ID를 함께 보관해 키가 정렬되거나 이동해도 같은 키를 선택할 수 있습니다.
struct FFloatCurveKey
{
    int32 KeyId = -1; // 배열 인덱스가 아니다. 드래그/정렬 후에도 유지한다.
    float Time = 0.0f;
    float Value = 0.0f;
    ECurveInterpMode InterpMode = ECurveInterpMode::Linear; // 이 키 -> 다음 키
    float ArriveTangent = 0.0f; // value / second
    float LeaveTangent = 0.0f;
};

UCLASS()
// 입력 시간에 해당하는 숫자를 키프레임과 보간 방식으로 계산합니다.
// 커브 자체는 시간을 진행하지 않으며 효과와 편집기가 같은 데이터를 사용합니다.
class UCurveFloat : public UObject
{
    GENERATED_BODY()

public:
    UCurveFloat();

    // 빈 커브·범위 밖을 처리한 뒤 upper_bound로 오른쪽 키를 찾고 EvalSegment에 위임합니다.
    float GetFloatValue(float InTime) const;

    // 유효한 시간/값인지 확인하고 같은 시각의 키를 갱신하거나 새 ID로 추가 후 정렬합니다.
    int32 AddKey(float InTime, float InValue);

    // 목록에서 ID가 같은 키를 찾아 제거합니다.
    bool RemoveKey(int32 KeyId);

    // 다른 키와 시각이 충돌하면 실패하고, 아니면 시간을 바꾼 뒤 다시 정렬합니다.
    bool SetKeyTime(int32 KeyId, float InTime);

    // ID를 찾아 값만 변경합니다.
    bool SetKeyValue(int32 KeyId, float InValue);

    // 유효한 enum인지 검사하고 해당 키의 모드를 변경합니다.
    // 왼쪽 키의 모드가 두 키 사이 구간의 형태를 결정합니다.
    bool SetKeyInterpMode(int32 KeyId, ECurveInterpMode InMode);

    // 도착/출발 접선을 값/초 단위로 저장합니다.
    // 구간 길이가 달라져도 같은 기울기 의미를 유지합니다.
    bool SetKeyTangents(int32 KeyId, float InArriveTangent, float InLeaveTangent);
    
    // 키 배열만 비우고 ID 발급기는 유지합니다.
    void Reset();

    // ID로 찾은 키 주소를 돌려줍니다. 없으면 nullptr을 반환합니다.
    const FFloatCurveKey* FindKey(int32 KeyId) const;

    // 시간순으로 관리되는 커브 키 목록을 읽기 전용으로 반환합니다.
    const TArray<FFloatCurveKey>& GetKeys() const { return Keys; }

    using UObject::Serialize;

    // 완전한 JSON 스키마를 CurveData 필드에 기록/로드하고 검증 실패를 Archive 오류로 전달합니다.
    virtual void Serialize(FStructuredArchive::FRecord Record) override;

    FString ExportToJson() const;

    bool ImportFromJson(const FString& Data);
    
    // 파일 크기와 읽기를 확인하고 ImportFromJson으로 위임합니다.
    bool LoadFromFile(const FString& InPath);

    // 임시 파일을 완전히 쓴 뒤 Windows 파일 교체 API로 대상 파일을 바꿉니다.
    bool SaveToFile(const FString& InPath) const;

    UPROPERTY(EditAnywhere)
    float DefaultValue = 0.0f;

private:

    // Time 기준 stable_sort로 순서를 유지합니다.
    void SortKeys();

    // Constant는 왼쪽 값, Linear는 직선, Cubic은 Hermite 기저와 접선을 사용합니다.
    float EvalSegment(const FFloatCurveKey& LeftKey, const FFloatCurveKey& RightKey, float InTime) const;

    // 현재 리플렉션에 구조체 배열 지원이 없으므로 Serialize에서 명시적으로 저장한다.
    TArray<FFloatCurveKey> Keys;
    int32 NextKeyId = 0;
};
