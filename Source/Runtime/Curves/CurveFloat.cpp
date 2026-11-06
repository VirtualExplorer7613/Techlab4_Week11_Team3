#include "EnginePCH.h"
#include "Curves/CurveFloat.h"
#include <set>

namespace
{
	constexpr float KeyTimeTolerance = 1e-6f; // 같은 시간으로 간주하는 오차. 
	constexpr int32 MaxCurveKeys = 10000; // 커브 키 개수 제한. UI/메모리/성능을 고려한 임시 상수.
    bool SameTime(float A, float B){ return std::abs(A - B) < KeyTimeTolerance; }
	bool ValidMode(ECurveInterpMode Mode) 
    { return Mode == ECurveInterpMode::Constant || Mode == ECurveInterpMode::Linear || Mode == ECurveInterpMode::Cubic; }
}


UCurveFloat::UCurveFloat() = default;

// 빈 커브, 범위 밖을 처리한 뒤 upper_bound로 오른쪽 키를 찾고 EvalSegment에 위임합니다.
float UCurveFloat::GetFloatValue(float InTime) const
{
	if (Keys.IsEmpty()) return DefaultValue;
    if (InTime <= Keys[0].Time) return Keys[0].Value;
    if (InTime >= Keys.Last().Time) return Keys.Last().Value;
	const auto Right = std::upper_bound(Keys.begin(), Keys.end(), InTime, [](float Time, const FFloatCurveKey& Key) { return Time < Key.Time; });
	return EvalSegment(*(Right - 1), *Right, InTime);
}

// 유효한 시간/값인지 확인하고 같은 시각의 키를 갱신하거나 새 ID로 추가 후 정렬합니다.
int32 UCurveFloat::AddKey(float InTime, float InValue)
{
	if (!std::isfinite(InTime) || !std::isfinite(InValue)) return -1;
    
	for (FFloatCurveKey& Key : Keys)
		if (SameTime(Key.Time, InTime)){ Key.Value = InValue; return Key.KeyId; }

	if (Keys.Num() >= MaxCurveKeys || NextKeyId == std::numeric_limits<int32>::max()) return -1;

	FFloatCurveKey NewKey;
	NewKey.Time = InTime;
	NewKey.Value = InValue;
	NewKey.KeyId = NextKeyId++;
    Keys.Add(NewKey);
	SortKeys();
    return NewKey.KeyId;
}

// 목록에서 ID가 같은 키를 찾아 제거합니다.
bool UCurveFloat::RemoveKey(int32 KeyId)
{
    for (int32 Index = 0; Index < Keys.Num(); ++Index)
        if (Keys[Index].KeyId == KeyId) { Keys.RemoveAt(Index); return true; }
    return false;
}

// 다른 키와 시각이 충돌하면 실패하고, 아니면 시간을 바꾼 뒤 다시 정렬합니다.

bool UCurveFloat::SetKeyTime(int32 KeyId, float InTime)
{
    if (!std::isfinite(InTime)) return false;

    // 드래그에서는 충돌한 키를 합치지 않는다. 선택한 ID와 다른 키를 함께 보존한다.

	for (const FFloatCurveKey& Key : Keys)
		if (Key.KeyId != KeyId && SameTime(Key.Time, InTime)) return false;
	for (FFloatCurveKey& Key : Keys)
		if (Key.KeyId == KeyId) { Key.Time = InTime; SortKeys(); return true; }
    return false;
}

// ID를 찾아 값만 변경하며 NaN/Inf 입력은 거부합니다.
bool UCurveFloat::SetKeyValue(int32 KeyId, float InValue)
{
	if (!std::isfinite(InValue)) return false;
	for (FFloatCurveKey& Key : Keys)
		if (Key.KeyId == KeyId) { Key.Value = InValue; return true; }
    return false;
}

// 유효한 enum인지 검사하고 해당 키의 모드를 변경합니다.
// 왼쪽 키의 모드가 두 키 사이의 구간의 형태를 결정합니다.
bool UCurveFloat::SetKeyInterpMode(int32 KeyId, ECurveInterpMode InMode)
{
    if (!ValidMode(InMode)) return false;
    for (FFloatCurveKey& Key : Keys)
        if (Key.KeyId == KeyId) { Key.InterpMode = InMode; return true; }
    return false;
}

// 도착/출발 접선을 값/초 단위로 저장합니다.
bool UCurveFloat::SetKeyTangents(int32 KeyId, float InArriveTangent, float InLeaveTangent)
{
    if (!std::isfinite(InArriveTangent) || !std::isfinite(InLeaveTangent)) return false;
    for (FFloatCurveKey& Key : Keys)
        if (Key.KeyId == KeyId) { Key.ArriveTangent = InArriveTangent; Key.LeaveTangent = InLeaveTangent; return true; }
    return false;
}

// 키 배열만 비우고 ID 발급기는 유지합니다.
// 삭제된 키의 오래된 ID가 새 키를 잘못 선택하지 않게 합니다.
void UCurveFloat::Reset()
{
    Keys.Reset(); // NextKeyId는 유지한다. 삭제된 키의 오래된 ID를 새 키에 재사용하지 않는다.
}

// ID로 찾은 키 주소를 돌려줍니다.
const FFloatCurveKey* UCurveFloat::FindKey(int32 KeyId) const
{
    for (const FFloatCurveKey& Key : Keys)
        if (Key.KeyId == KeyId) return &Key;
    return nullptr;
}

// 완전한 JSON 스키마를 CurveData 필드에 기록/로드하고 검증 실패를 Archive 오류로 전달합니다.
void UCurveFloat::Serialize(FStructuredArchive::FRecord Record)
{
    // DefaultValue까지 포함하는 명시적 스키마. 지원되지 않는 구조체 배열 리플렉션에 의존하지 않는다.
    FArchive& Archive = Record.GetUnderlyingArchive();
    FString Data = Archive.IsSaving() ? ExportToJson() : FString();
    Record.EnterField("CurveData") << Data;
    if (Archive.IsLoading() && !Archive.IsError() && !ImportFromJson(Data)) Archive.SetError();
}

// 역할: 타입·버전·기본값·키 ID/시간/값/모드/접선을 JSON 문자열로 만듭니다.
// 설명: 파일 저장과 Undo가 같은 데이터 형식을 쓰도록 합니다.
FString UCurveFloat::ExportToJson() const
{
    json Data = { {"Type", "CurveFloat"}, {"Version", 1}, {"DefaultValue", DefaultValue}, {"Keys", json::array()} };
    for (const FFloatCurveKey& Key : Keys)
        Data["Keys"].push_back({ {"Id", Key.KeyId}, {"Time", Key.Time}, {"Value", Key.Value},
            {"InterpMode", static_cast<int>(Key.InterpMode)}, {"ArriveTangent", Key.ArriveTangent}, {"LeaveTangent", Key.LeaveTangent} });
    return Data.dump(2);
}

// 임시 배열에 파싱·형식·범위·ID·시간 중복 검증을 끝내고 나서 현재 커브를 교체합니다.
bool UCurveFloat::ImportFromJson(const FString& Data)
{
    try
    {
        const json Node = json::parse(Data, nullptr, false);
        if (!Node.is_object() || Node.value("Type", FString()) != "CurveFloat" ||
            Node.value("Version", 0) != 1 || !Node.contains("Keys") || !Node["Keys"].is_array() ||
            Node["Keys"].size() > MaxCurveKeys || !Node.contains("DefaultValue") || !Node["DefaultValue"].is_number()) return false;
        const float NewDefault = Node["DefaultValue"].get<float>();
        if (!std::isfinite(NewDefault)) return false;
        TArray<FFloatCurveKey> NewKeys;
        std::set<int32> SeenIds;
        int32 NewNextId = NextKeyId;
        for (const json& Entry : Node["Keys"])
        {
            if (!Entry.is_object() || !Entry.contains("Id") || !Entry["Id"].is_number_integer() ||
                !Entry.contains("InterpMode") || !Entry["InterpMode"].is_number_integer()) return false;
            for (const char* Name : { "Time", "Value", "ArriveTangent", "LeaveTangent" })
                if (!Entry.contains(Name) || !Entry[Name].is_number()) return false;
            FFloatCurveKey Key;
            const int64 Id = Entry["Id"].get<int64>();
            const int64 Mode = Entry["InterpMode"].get<int64>();
            if (Id < 0 || Id >= std::numeric_limits<int32>::max() || Mode < 0 || Mode > 2) return false;
            Key.KeyId = static_cast<int32>(Id);
            Key.InterpMode = static_cast<ECurveInterpMode>(Mode);
            Key.Time = Entry["Time"].get<float>();
            Key.Value = Entry["Value"].get<float>();
            Key.ArriveTangent = Entry["ArriveTangent"].get<float>();
            Key.LeaveTangent = Entry["LeaveTangent"].get<float>();
            if (!std::isfinite(Key.Time) || !std::isfinite(Key.Value) || !std::isfinite(Key.ArriveTangent) ||
                !std::isfinite(Key.LeaveTangent) || !SeenIds.insert(Key.KeyId).second) return false;
            NewNextId = std::max(NewNextId, Key.KeyId + 1);
            NewKeys.Add(Key);
        }
        std::stable_sort(NewKeys.begin(), NewKeys.end(), [](const auto& A, const auto& B) { return A.Time < B.Time; });
        for (int32 Index = 1; Index < NewKeys.Num(); ++Index)
            if (SameTime(NewKeys[Index - 1].Time, NewKeys[Index].Time)) return false;
        // 모든 검증이 끝난 후에만 현재 데이터를 교체한다. 실패한 로드/Undo는 원본을 보존한다.
        Keys = std::move(NewKeys);
        DefaultValue = NewDefault;
        NextKeyId = NewNextId;
        return true;
    }
    catch (const json::exception&) { return false; }
}

// 파일 크기와 읽기를 확인하고 ImportFromJson으로 위임합니다.
bool UCurveFloat::LoadFromFile(const FString& InPath)
{
    std::ifstream Input(std::filesystem::u8path(InPath), std::ios::binary);
    if (!Input) return false;
    Input.seekg(0, std::ios::end);
    const auto Size = Input.tellg();
    if (Size < 0 || Size > 4 * 1024 * 1024) return false;
    Input.seekg(0);
    const FString Data((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
    return !Input.bad() && ImportFromJson(Data);
}

// 임시 파일을 완전히 쓴 뒤 Windows 파일 교체 API로 대상 파일을 바꿉니다.
bool UCurveFloat::SaveToFile(const FString& InPath) const
{
    if (InPath.empty() || !std::isfinite(DefaultValue)) return false;
    try
    {
        const auto Path = std::filesystem::u8path(InPath);
        auto Temporary = Path;
        Temporary += L".camera-curve.tmp";
        std::ofstream Output(Temporary, std::ios::binary | std::ios::trunc);
        if (!Output) return false;
        Output << ExportToJson() << '\n';
        Output.flush();
        const bool bWritten = Output.good();
        Output.close();
        if (!bWritten || Output.fail()) { std::filesystem::remove(Temporary); return false; }
        if (!::MoveFileExW(Temporary.c_str(), Path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(Temporary); return false;
        }
        return true;
    }
    catch (const std::exception&) { return false; }
}

// Time 기준 stable_sort로 순서를 유지합니다.
void UCurveFloat::SortKeys()
{
    std::stable_sort(Keys.begin(), Keys.end(), [](const auto& A, const auto& B) { return A.Time < B.Time; });
}

// Constant는 왼쪽 값, Linear는 직선, Cubic은 Hermite 기저와 접선을 사용합니다.
float UCurveFloat::EvalSegment(const FFloatCurveKey& LeftKey, const FFloatCurveKey& RightKey, float InTime) const
{
    const float Length = RightKey.Time - LeftKey.Time;
    if (Length <= KeyTimeTolerance) return LeftKey.Value;
    const float T = std::clamp((InTime - LeftKey.Time) / Length, 0.0f, 1.0f);
    if (LeftKey.InterpMode == ECurveInterpMode::Constant) return LeftKey.Value;
    if (LeftKey.InterpMode == ECurveInterpMode::Linear) return LeftKey.Value + (RightKey.Value - LeftKey.Value) * T;
    const float T2 = T * T, T3 = T2 * T;
    return (2 * T3 - 3 * T2 + 1) * LeftKey.Value + (T3 - 2 * T2 + T) * LeftKey.LeaveTangent * Length +
        (-2 * T3 + 3 * T2) * RightKey.Value + (T3 - T2) * RightKey.ArriveTangent * Length;
}