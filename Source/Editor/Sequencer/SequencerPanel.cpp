#include "EnginePCH.h"
#include "Editor/Sequencer/SequencerPanel.h"

#include "LevelSequence/LevelSequence.h"
#include "LevelSequence/LevelSequencePlayer.h"
#include "LevelSequence/LevelSequenceActor.h"
#include "MovieScene/MovieScene.h"
#include "MovieSceneTracks/MovieScene3DTransformTrack.h"
#include "MovieSceneTracks/MovieSceneFloatTrack.h"
#include "MovieSceneTracks/MovieSceneFadeTrack.h"
#include "MovieSceneTracks/MovieSceneCameraCutTrack.h"
#include "MovieSceneTracks/MovieSceneEventTrack.h"
#include "MovieSceneTracks/MovieSceneSlomoTrack.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
	constexpr float LabelWidth = 240.0f;
	constexpr float RulerHeight = 24.0f;
	constexpr float RowHeight = 22.0f;
	constexpr float KeyRadius = 6.0f;
	constexpr float DetailsHeight = 190.0f;

	const ImU32 ColorKey = IM_COL32(205, 205, 205, 255);
	const ImU32 ColorKeySelected = IM_COL32(255, 200, 80, 255);
	const ImU32 ColorPlayhead = IM_COL32(230, 80, 60, 255);

	void DrawDiamond(ImDrawList* Draw, ImVec2 Center, float Radius, ImU32 Color)
	{
		Draw->AddQuadFilled({ Center.x, Center.y - Radius }, { Center.x + Radius, Center.y },
			{ Center.x, Center.y + Radius }, { Center.x - Radius, Center.y }, Color);
	}

	ULevel* GetActiveLevel(UWorld* World)
	{
		if (!World) return nullptr;
		return World->GetCurrentLevel() ? World->GetCurrentLevel() : World->GetPersistentLevel();
	}

	// 레벨 액터 이름 콤보
	bool ActorCombo(const char* Label, UWorld* World, FString& InOutName)
	{
		bool bChanged = false;
		if (ImGui::BeginCombo(Label, InOutName.empty() ? "(none)" : InOutName.c_str()))
		{
			if (ULevel* Level = GetActiveLevel(World))
			{
				for (AActor* Actor : Level->GetActors())
				{
					if (!Actor || Cast<ALevelSequenceActor>(Actor)) continue;
					const FString Name = Actor->GetName();
					if (ImGui::Selectable(Name.c_str(), Name == InOutName))
					{
						InOutName = Name;
						bChanged = true;
					}
				}
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	// 시퀀스 바인딩 콤보 (Camera Cut 대상)
	bool BindingCombo(const char* Label, UMovieScene* MovieScene, FGuid& InOutGuid)
	{
		bool bChanged = false;
		const FMovieScenePossessable* Current = MovieScene ? MovieScene->FindPossessable(InOutGuid) : nullptr;
		if (ImGui::BeginCombo(Label, Current ? Current->Name.c_str() : "(none)"))
		{
			if (MovieScene)
			{
				for (const FMovieScenePossessable& Possessable : MovieScene->GetPossessables())
				{
					ImGui::PushID(Possessable.Guid.ToString().c_str());
					if (ImGui::Selectable(Possessable.Name.c_str(), Possessable.Guid == InOutGuid))
					{
						InOutGuid = Possessable.Guid;
						bChanged = true;
					}
					ImGui::PopID();
				}
			}
			ImGui::EndCombo();
		}
		return bChanged;
	}

	struct FGlobalTrackType
	{
		const char* Label;
		UClass* (*GetClass)();
	};

	const FGlobalTrackType GlobalTrackTypes[] = {
		{ "Fade",           []() { return UMovieSceneFadeTrack::StaticClass(); } },
		{ "Camera Overlay", []() { return UMovieSceneCameraOverlayTrack::StaticClass(); } },
		{ "Event",          []() { return UMovieSceneEventTrack::StaticClass(); } },
		{ "Play Rate",      []() { return UMovieSceneSlomoTrack::StaticClass(); } },
	};
}

FSequencerPanel::FSequencerPanel()
{
	Player = NewObject<ULevelSequencePlayer>();
	NewSequence();
}

FSequencerPanel::~FSequencerPanel()
{
	Shutdown();
}

void FSequencerPanel::Shutdown()
{
	// 종료 중이라 액터를 되돌리거나 카메라를 돌려줄 필요가 없다
	if (Player) Player->DiscardStateAndStop();
	delete Player;
	Player = nullptr;
	delete Sequence;
	Sequence = nullptr;
	ClearSelection();
	World = nullptr;
}

bool FSequencerPanel::Init()
{
	return true;
}

UMovieScene* FSequencerPanel::GetMovieScene() const
{
	return Sequence ? Sequence->GetMovieScene() : nullptr;
}

void FSequencerPanel::NewSequence()
{
	if (Player) Player->Stop();
	delete Sequence;
	Sequence = NewObject<ULevelSequence>();
	Sequence->Initialize();
	ClearSelection();
	ResetPlayer();
}

void FSequencerPanel::ResetPlayer()
{
	if (!Player) return;
	const FMovieSceneSequencePlaybackSettings Settings = Player->GetPlaybackSettings();
	Player->Initialize(Sequence, World, Settings);
	// 에디터 미리보기는 Stop하면 항상 원래 값으로 (UE 시퀀서를 닫을 때와 같은 결과)
	Player->bRestoreStateByDefault = true;
}

void FSequencerPanel::SetWorld(UWorld* InWorld, bool bIsEditorWorld)
{
	if (World == InWorld) return;
	// 에디터 월드는 계속 살아 있으니 되돌린다. PIE 월드는 이미 지워졌을 수 있어 건드리지 않는다
	if (Player)
	{
		if (World && bWorldIsEditor) Player->Stop();
		else Player->DiscardStateAndStop();
	}
	World = InWorld;
	bWorldIsEditor = bIsEditorWorld;
	ResetPlayer();
}

void FSequencerPanel::ClearSelection()
{
	SelectedTrack = nullptr;
	SelectedBinding.Invalidate();
	SelectedChannel = SelectedKey = -1;
}

UObject* FSequencerPanel::ResolveBinding(const FGuid& Guid) const
{
	return Sequence ? Sequence->LocateBoundObject(Guid, World) : nullptr;
}

UMovieSceneSection* FSequencerPanel::GetKeySection(UMovieSceneTrack* Track) const
{
	if (!Track || Track->GetAllSections().IsEmpty()) return nullptr;
	return Track->GetAllSections()[0];
}

float FSequencerPanel::FrameToSeconds(FFrameNumber Frame) const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return 0.0f;
	return static_cast<float>(MovieScene->GetTickResolution().AsSeconds(FFrameTime(Frame - MovieScene->GetPlaybackRange().GetLowerBoundValue())));
}

FFrameNumber FSequencerPanel::SecondsToFrame(float Seconds) const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return FFrameNumber(0);
	return MovieScene->GetPlaybackRange().GetLowerBoundValue() +
		FFrameNumber(static_cast<int32>(std::lround(Seconds * MovieScene->GetTickResolution().AsDecimal())));
}

float FSequencerPanel::SnapSeconds(float Seconds) const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!bSnap || !MovieScene) return Seconds;
	const double Rate = MovieScene->GetDisplayRate().AsDecimal();
	return static_cast<float>(std::round(Seconds * Rate) / Rate);
}

float FSequencerPanel::GetLengthSeconds() const
{
	const UMovieScene* MovieScene = GetMovieScene();
	return MovieScene ? static_cast<float>(MovieScene->GetTickResolution().AsSeconds(FFrameTime(FFrameNumber(MovieScene->GetPlaybackRange().Size())))) : 0.0f;
}

float FSequencerPanel::GetPlayheadSeconds() const
{
	return Player ? static_cast<float>(Player->GetCurrentTimeSeconds()) : 0.0f;
}

void FSequencerPanel::Tick(float DeltaTime)
{
	if (!Player || !World) return;
	// UE 시퀀스 플레이어처럼 월드 시간 배율을 따른다 (Play Rate 트랙, HitStop 포함)
	if (Player->IsPlaying()) Player->Update(DeltaTime * World->GetEffectiveTimeDilation());

	// 에디터 월드: Camera Cut 카메라를 뷰포트가 따라간다 (PIE는 플레이어가 게임 뷰포트를 바꾼다)
	if (bWorldIsEditor && PilotCamera && Player->GetPlaybackStatus() != EMovieScenePlayerStatus::Stopped)
		if (UCameraComponent* Camera = Player->GetActiveCameraComponent()) PilotCamera(Camera);
}

void FSequencerPanel::OnRender()
{
	if (!ImGui::Begin("Sequencer"))
	{
		ImGui::End();
		return;
	}
	DrawToolbar();
	ImGui::Separator();

	ImGui::BeginChild("##Timeline", ImVec2(0.0f, -DetailsHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollWithMouse);
	DrawTimeline();
	ImGui::EndChild();

	ImGui::BeginChild("##Details", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
	DrawDetails();
	ImGui::EndChild();
	ImGui::End();
}

void FSequencerPanel::DrawToolbar()
{
	UMovieScene* MovieScene = GetMovieScene();
	FMovieSceneSequencePlaybackSettings& Settings = Player->GetPlaybackSettings();

	const bool bPlaying = Player->IsPlaying();
	if (ImGui::Button(bPlaying ? "Pause" : "Play", ImVec2(60.0f, 0.0f)))
	{
		if (bPlaying) Player->Pause();
		else Player->Play();
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop", ImVec2(60.0f, 0.0f))) Player->Stop();
	ImGui::SameLine();
	const FQualifiedFrameTime Current = Player->GetCurrentTime();
	ImGui::Text("%6.2f s  |  frame %d", GetPlayheadSeconds(), Current.Time.FloorToFrame().Value);
	ImGui::SameLine();

	float Length = GetLengthSeconds();
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::DragFloat("Length", &Length, 0.1f, 0.5f, 600.0f, "%.1f s") && MovieScene)
		MovieScene->SetPlaybackRange(MovieScene->GetPlaybackRange().GetLowerBoundValue(), SecondsToFrame(Length).Value - MovieScene->GetPlaybackRange().GetLowerBoundValue().Value);
	ImGui::SameLine();
	bool bLoop = Settings.LoopCount.Value != 0;
	if (ImGui::Checkbox("Loop", &bLoop)) Settings.LoopCount.Value = bLoop ? -1 : 0;
	ImGui::SameLine();
	ImGui::Checkbox("Hold End", &Settings.bPauseAtEnd);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(70.0f);
	ImGui::DragFloat("Rate", &Settings.PlayRate, 0.05f, 0.1f, 4.0f, "%.2f");
	ImGui::SameLine();
	ImGui::Checkbox("Snap", &bSnap);

	if (ImGui::Button("+ Actor")) ImGui::OpenPopup("AddActor");
	DrawAddActorPopup();
	ImGui::SameLine();
	if (ImGui::Button("+ Track")) ImGui::OpenPopup("AddTrack");
	DrawAddTrackPopup();
	ImGui::SameLine();
	ImGui::BeginDisabled(!SelectedTrack);
	if (ImGui::Button("Key")) KeySelectedTrack();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (ImGui::Button("New")) NewSequence();
	ImGui::SameLine();
	ImGui::SetNextItemWidth(260.0f);
	ImGui::InputText("##AssetPath", AssetPath, sizeof(AssetPath));
	ImGui::SameLine();
	if (ImGui::Button("Save") && !Sequence->SaveToFile(AssetPath))
		HTR_LOG(Warning, "Sequence save failed: {}", AssetPath);
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		Player->Stop();
		ULevelSequence* Loaded = ULevelSequence::LoadLevelSequence(AssetPath);
		if (Loaded)
		{
			delete Sequence;
			Sequence = Loaded;
			ClearSelection();
			ResetPlayer();
		}
	}
	ImGui::SameLine();
	// 저장한 시퀀스를 재생하는 ALevelSequenceActor를 에디터 월드에 놓는다 (PIE 시작 때 자동 재생)
	ImGui::BeginDisabled(!bWorldIsEditor || !World);
	if (ImGui::Button("Place Actor"))
	{
		Sequence->SaveToFile(AssetPath);
		if (ALevelSequenceActor* Actor = World->SpawnActor<ALevelSequenceActor>())
			Actor->LevelSequenceAsset = AssetPath;
	}
	ImGui::EndDisabled();
}

void FSequencerPanel::DrawAddActorPopup()
{
	if (!ImGui::BeginPopup("AddActor")) return;
	ImGui::SetNextItemWidth(200.0f);
	ActorCombo("Actor", World, AddActorName);
	ImGui::BeginDisabled(AddActorName.empty());
	if (ImGui::Button("Add to Sequence"))
	{
		// UE "Actor To Sequencer": Possessable 바인딩 + 기본 Transform 트랙
		UMovieScene* MovieScene = GetMovieScene();
		AActor* Actor = nullptr;
		if (ULevel* Level = GetActiveLevel(World))
			for (AActor* Candidate : Level->GetActors())
				if (Candidate && Candidate->GetName() == AddActorName) { Actor = Candidate; break; }
		if (MovieScene && Actor)
		{
			const FGuid Guid = MovieScene->AddPossessable(AddActorName, Actor->GetClass());
			Sequence->BindPossessableObject(Guid, *Actor, World);
			MovieScene->AddTrack<UMovieScene3DTransformTrack>(Guid);
			SelectedBinding = Guid;
		}
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::EndPopup();
}

void FSequencerPanel::DrawAddTrackPopup()
{
	if (!ImGui::BeginPopup("AddTrack")) return;
	UMovieScene* MovieScene = GetMovieScene();
	ImGui::BeginDisabled(MovieScene && MovieScene->GetCameraCutTrack());
	if (ImGui::MenuItem("Camera Cut") && MovieScene)
		SelectedTrack = MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
	ImGui::EndDisabled();
	for (const FGlobalTrackType& Type : GlobalTrackTypes)
	{
		if (ImGui::MenuItem(Type.Label) && MovieScene)
		{
			SelectedTrack = MovieScene->AddTrack(Type.GetClass());
			SelectedBinding.Invalidate();
			SelectedChannel = SelectedKey = -1;
		}
	}
	ImGui::EndPopup();
}

void FSequencerPanel::KeySelectedTrack()
{
	UMovieSceneSection* Section = GetKeySection(SelectedTrack);
	if (!Section) return;
	UObject* BoundObject = SelectedBinding.IsValid() ? ResolveBinding(SelectedBinding) : nullptr;
	Section->KeyCurrentValues(SecondsToFrame(SnapSeconds(GetPlayheadSeconds())), *Player, BoundObject);
	Player->Refresh();
}

void FSequencerPanel::BuildRows(TArray<FRow>& OutRows) const
{
	const UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return;
	auto AddTrackRows = [&OutRows, this](UMovieSceneTrack* Track, const FGuid& Binding)
	{
		FRow Header;
		Header.Type = ERowType::Track;
		Header.Track = Track;
		Header.Binding = Binding;
		OutRows.Add(Header);
		UMovieSceneSection* Section = GetKeySection(Track);
		if (!Track->bExpanded || !Section) return;
		for (int32 Channel = 0; Channel < Section->GetNumChannels(); ++Channel)
		{
			FRow Row = Header;
			Row.Type = ERowType::Channel;
			Row.Channel = Channel;
			OutRows.Add(Row);
		}
	};

	if (UMovieSceneTrack* Cut = MovieScene->GetCameraCutTrack()) AddTrackRows(Cut, FGuid());
	for (UMovieSceneTrack* Track : MovieScene->GetTracks()) AddTrackRows(Track, FGuid());
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		FRow Row;
		Row.Type = ERowType::Binding;
		Row.Binding = Binding.GetObjectGuid();
		OutRows.Add(Row);
		for (UMovieSceneTrack* Track : Binding.GetTracks()) AddTrackRows(Track, Binding.GetObjectGuid());
	}
}

void FSequencerPanel::DrawTimeline()
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene) return;

	ImDrawList* Draw = ImGui::GetWindowDrawList();
	const ImGuiIO& IO = ImGui::GetIO();
	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const float Width = ImGui::GetContentRegionAvail().x;
	const float TimelineX = Origin.x + LabelWidth;
	const float EndX = Origin.x + std::max(Width, LabelWidth + 50.0f);
	const float Length = GetLengthSeconds();

	// 줌(Ctrl+휠, 마우스 아래 시간 고정) · 팬(Shift+휠)
	if (ImGui::IsWindowHovered() && IO.MouseWheel != 0.0f)
	{
		if (IO.KeyCtrl)
		{
			const float MouseTime = XToTime(IO.MousePos.x, TimelineX);
			PixelsPerSecond = std::clamp(PixelsPerSecond * (IO.MouseWheel > 0.0f ? 1.15f : 1.0f / 1.15f), 10.0f, 2000.0f);
			ViewStart = MouseTime - (IO.MousePos.x - TimelineX) / PixelsPerSecond;
		}
		else if (IO.KeyShift)
		{
			ViewStart -= IO.MouseWheel * 40.0f / PixelsPerSecond;
		}
		ViewStart = std::max(ViewStart, 0.0f);
	}

	TArray<FRow> Rows;
	BuildRows(Rows);

	// 눈금자
	Draw->AddRectFilled({ TimelineX, Origin.y }, { EndX, Origin.y + RulerHeight }, IM_COL32(40, 44, 48, 255));
	const float Steps[] = { 1.0f / 30.0f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 30.0f };
	float Step = Steps[IM_ARRAYSIZE(Steps) - 1];
	for (float Candidate : Steps)
		if (Candidate * PixelsPerSecond >= 60.0f) { Step = Candidate; break; }
	for (float T = std::floor(ViewStart / Step) * Step; TimeToX(T, TimelineX) < EndX; T += Step)
	{
		const float X = TimeToX(T, TimelineX);
		if (X < TimelineX) continue;
		Draw->AddLine({ X, Origin.y + RulerHeight - 6.0f }, { X, Origin.y + RulerHeight }, IM_COL32(150, 150, 150, 255));
		char Label[16];
		std::snprintf(Label, sizeof(Label), "%.2f", T);
		Draw->AddText({ X + 3.0f, Origin.y + 3.0f }, IM_COL32(190, 190, 190, 255), Label);
	}
	ImGui::SetCursorScreenPos({ TimelineX, Origin.y });
	ImGui::InvisibleButton("##Ruler", { EndX - TimelineX, RulerHeight });
	if (ImGui::IsItemActive())
	{
		const float Seconds = SnapSeconds(std::clamp(XToTime(IO.MousePos.x, TimelineX), 0.0f, Length));
		Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(Seconds, EUpdatePositionMethod::Scrub));
	}

	const float RowsTop = Origin.y + RulerHeight;
	UMovieSceneTrack* PendingDeleteTrack = nullptr;
	FGuid PendingDeleteBinding;
	FGuid PendingTrackBinding;
	UClass* PendingTrackClass = nullptr;

	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		const FRow& Row = Rows[RowIndex];
		const float Y = RowsTop + RowIndex * RowHeight;
		const bool bRowSelected = Row.Type != ERowType::Binding && Row.Track == SelectedTrack && Row.Channel == SelectedChannel;
		const ImU32 Background = bRowSelected ? IM_COL32(45, 70, 90, 255)
			: Row.Type == ERowType::Binding ? IM_COL32(44, 40, 34, 255)
			: Row.Type == ERowType::Track ? IM_COL32(34, 37, 40, 255) : IM_COL32(26, 28, 30, 255);
		Draw->AddRectFilled({ Origin.x, Y }, { EndX, Y + RowHeight }, Background);

		ImGui::PushID(RowIndex);
		ImGui::SetCursorScreenPos({ Origin.x, Y });
		const bool bLabelClicked = ImGui::InvisibleButton("##Label", { LabelWidth, RowHeight });

		char Label[160];
		if (Row.Type == ERowType::Binding)
		{
			const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(Row.Binding);
			const bool bResolved = ResolveBinding(Row.Binding) != nullptr;
			std::snprintf(Label, sizeof(Label), "%s%s", Possessable ? Possessable->Name.c_str() : "?", bResolved ? "" : "  (missing)");
			Draw->AddText({ Origin.x + 6.0f, Y + 4.0f }, bResolved ? IM_COL32(240, 220, 170, 255) : IM_COL32(230, 110, 90, 255), Label);
			if (bLabelClicked) { ClearSelection(); SelectedBinding = Row.Binding; }
			if (ImGui::BeginPopupContextItem("##BindingMenu"))
			{
				if (ImGui::MenuItem("Add Transform Track")) { PendingTrackBinding = Row.Binding; PendingTrackClass = UMovieScene3DTransformTrack::StaticClass(); }
				ImGui::SetNextItemWidth(140.0f);
				ImGui::InputText("##Property", PropertyNameBuffer, sizeof(PropertyNameBuffer));
				ImGui::SameLine();
				if (ImGui::MenuItem("Add Float Track")) { PendingTrackBinding = Row.Binding; PendingTrackClass = UMovieSceneFloatTrack::StaticClass(); }
				ImGui::Separator();
				if (ImGui::MenuItem("Remove Binding")) PendingDeleteBinding = Row.Binding;
				ImGui::EndPopup();
			}
			ImGui::PopID();
			continue;
		}

		UMovieSceneTrack* Track = Row.Track;
		UMovieSceneSection* KeySection = GetKeySection(Track);
		const float Indent = Row.Binding.IsValid() ? 14.0f : 0.0f;
		if (bLabelClicked)
		{
			SelectedTrack = Track;
			SelectedBinding = Row.Binding;
			SelectedChannel = Row.Channel;
			SelectedKey = -1;
			if (Row.Type == ERowType::Track && IO.MousePos.x < Origin.x + Indent + 18.0f) Track->bExpanded = !Track->bExpanded;
		}
		if (Row.Type == ERowType::Track)
		{
			const bool bHasChannels = KeySection && KeySection->GetNumChannels() > 0;
			std::snprintf(Label, sizeof(Label), "%s %s%s", bHasChannels ? (Track->bExpanded ? "v" : ">") : " ",
				Track->GetDisplayName(), Track->bIsEvalDisabled ? "  [M]" : "");
			if (ImGui::BeginPopupContextItem("##TrackMenu"))
			{
				ImGui::MenuItem("Mute", nullptr, &Track->bIsEvalDisabled);
				if (ImGui::MenuItem("Delete Track")) PendingDeleteTrack = Track;
				ImGui::EndPopup();
			}
		}
		else
		{
			std::snprintf(Label, sizeof(Label), "      %s", KeySection ? KeySection->GetChannelName(Row.Channel) : "");
		}
		Draw->AddText({ Origin.x + 6.0f + Indent, Y + 4.0f }, Track->bIsEvalDisabled ? IM_COL32(120, 120, 120, 255) : IM_COL32(220, 220, 220, 255), Label);

		Draw->PushClipRect({ TimelineX, Y }, { EndX, Y + RowHeight }, true);

		if (Row.Type == ERowType::Track)
		{
			// 섹션 막대 (열린 쪽은 재생 범위 끝까지)
			int32 SectionIndex = 0;
			for (UMovieSceneSection* Section : Track->GetAllSections())
			{
				const float Start = Section->HasStartFrame() ? FrameToSeconds(Section->GetInclusiveStartFrame()) : 0.0f;
				const float End = Section->HasEndFrame() ? FrameToSeconds(Section->GetExclusiveEndFrame()) : Length;
				const ImVec2 Min{ TimeToX(Start, TimelineX) + 1.0f, Y + 3.0f };
				const ImVec2 Max{ TimeToX(End, TimelineX) - 1.0f, Y + RowHeight - 3.0f };
				const ImU32 Fill = (SectionIndex++ % 2) ? IM_COL32(107, 79, 47, 200) : IM_COL32(47, 93, 107, 200);
				Draw->AddRectFilled(Min, Max, Section->IsActive() ? Fill : IM_COL32(60, 60, 60, 200), 2.0f);
				if (const UMovieSceneCameraCutSection* Cut = Cast<UMovieSceneCameraCutSection>(Section))
				{
					const FMovieScenePossessable* Camera = MovieScene->FindPossessable(Cut->GetCameraBindingID().GetGuid());
					Draw->AddText({ Min.x + 4.0f, Y + 4.0f }, IM_COL32(235, 235, 235, 255), Camera ? Camera->Name.c_str() : "(no camera)");
				}
				if (const UMovieSceneEventTriggerSection* Events = Cast<UMovieSceneEventTriggerSection>(Section))
				{
					for (const FFrameNumber& Time : Events->EventChannel.Times)
					{
						const float X = TimeToX(FrameToSeconds(Time), TimelineX);
						Draw->AddRectFilled({ X - 2.0f, Y + 4.0f }, { X + 2.0f, Y + RowHeight - 4.0f }, IM_COL32(226, 194, 125, 255));
					}
				}
			}
		}
		else if (KeySection)
		{
			FMovieSceneFloatChannel* Channel = KeySection->GetChannel(Row.Channel);
			const int32 NumKeys = Channel ? Channel->GetNumKeys() : 0;
			for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
			{
				const float KeySeconds = FrameToSeconds(Channel->GetTimes()[KeyIndex]);
				const float X = TimeToX(KeySeconds, TimelineX);
				if (X < TimelineX - KeyRadius || X > EndX + KeyRadius) continue;
				const ImVec2 Center{ X, Y + RowHeight * 0.5f };

				ImGui::PushID(KeyIndex);
				ImGui::SetCursorScreenPos({ X - KeyRadius, Center.y - KeyRadius });
				ImGui::InvisibleButton("##Key", { KeyRadius * 2.0f, KeyRadius * 2.0f });
				if (ImGui::IsItemActivated())
				{
					SelectedTrack = Track;
					SelectedBinding = Row.Binding;
					SelectedChannel = Row.Channel;
					SelectedKey = KeyIndex;
				}
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					// 이웃 키를 넘지 못하게 막아 정렬과 인덱스를 유지한다
					const TArray<FFrameNumber>& Times = Channel->GetTimes();
					const int32 MinFrame = KeyIndex > 0 ? Times[KeyIndex - 1].Value + 1 : SecondsToFrame(0.0f).Value;
					const int32 MaxFrame = KeyIndex + 1 < NumKeys ? Times[KeyIndex + 1].Value - 1 : SecondsToFrame(Length).Value;
					const int32 NewFrame = std::clamp(SecondsToFrame(SnapSeconds(XToTime(IO.MousePos.x, TimelineX))).Value, MinFrame, std::max(MinFrame, MaxFrame));
					SelectedKey = Channel->SetKeyTime(KeyIndex, FFrameNumber(NewFrame));
					Player->Refresh();
				}
				const bool bKeySelected = bRowSelected && SelectedKey == KeyIndex;
				DrawDiamond(Draw, Center, KeyRadius, bKeySelected ? ColorKeySelected : ColorKey);
				ImGui::PopID();
			}

			// 빈 곳 더블클릭: 현재 평가 값으로 키 추가
			const bool bInRow = IO.MousePos.x >= TimelineX && IO.MousePos.x < EndX && IO.MousePos.y >= Y && IO.MousePos.y < Y + RowHeight;
			if (Channel && bInRow && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				const FFrameNumber Frame = SecondsToFrame(SnapSeconds(std::clamp(XToTime(IO.MousePos.x, TimelineX), 0.0f, Length)));
				float Value = 0.0f;
				Channel->Evaluate(FFrameTime(Frame), Value);
				SelectedTrack = Track;
				SelectedBinding = Row.Binding;
				SelectedChannel = Row.Channel;
				SelectedKey = Channel->UpdateOrAddKey(Frame, Value);
				Player->Refresh();
			}
		}

		Draw->PopClipRect();
		ImGui::PopID();
	}

	const float Bottom = RowsTop + Rows.Num() * RowHeight;
	const float LengthX = TimeToX(Length, TimelineX);
	if (LengthX < EndX) Draw->AddRectFilled({ std::max(LengthX, TimelineX), RowsTop }, { EndX, std::max(Bottom, RowsTop + 40.0f) }, IM_COL32(0, 0, 0, 90));
	const float PlayX = TimeToX(GetPlayheadSeconds(), TimelineX);
	if (PlayX >= TimelineX && PlayX <= EndX)
		Draw->AddLine({ PlayX, Origin.y }, { PlayX, std::max(Bottom, RowsTop + 40.0f) }, ColorPlayhead, 2.0f);

	// 선택한 키 Delete
	if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && SelectedKey >= 0)
	{
		if (UMovieSceneSection* Section = GetKeySection(SelectedTrack))
			if (FMovieSceneFloatChannel* Channel = Section->GetChannel(SelectedChannel))
			{
				Channel->RemoveKey(SelectedKey);
				SelectedKey = -1;
				Player->Refresh();
			}
	}

	if (PendingTrackClass)
	{
		UMovieSceneTrack* Track = MovieScene->AddTrack(PendingTrackClass, PendingTrackBinding);
		if (UMovieScenePropertyTrack* Property = Cast<UMovieScenePropertyTrack>(Track))
			Property->SetPropertyNameAndPath(PropertyNameBuffer);
	}
	if (PendingDeleteTrack)
	{
		Player->Stop();
		MovieScene->RemoveTrack(PendingDeleteTrack);
		ClearSelection();
	}
	if (PendingDeleteBinding.IsValid())
	{
		Player->Stop();
		MovieScene->RemovePossessable(PendingDeleteBinding);
		Sequence->UnbindPossessableObjects(PendingDeleteBinding);
		ClearSelection();
	}

	ImGui::SetCursorScreenPos({ Origin.x, std::max(Bottom, RowsTop + 40.0f) });
	ImGui::Dummy({ 1.0f, 4.0f });
}

void FSequencerPanel::DrawDetails()
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene || !SelectedTrack)
	{
		ImGui::TextDisabled("+ Actor binds a level actor. Right-click a binding to add tracks. Ctrl+Wheel zoom, Shift+Wheel pan.");
		ImGui::TextDisabled("Double-click a channel row to add a key. Key records the bound actor's current values at the playhead.");
		return;
	}
	if (SelectedTrack == MovieScene->GetCameraCutTrack())
	{
		DrawCameraCutDetails();
		return;
	}

	UMovieSceneSection* Section = GetKeySection(SelectedTrack);
	if (!Section) return;
	if (Cast<UMovieSceneEventTriggerSection>(Section))
	{
		DrawEventDetails(*Section);
		return;
	}

	FMovieSceneFloatChannel* Channel = SelectedChannel >= 0 ? Section->GetChannel(SelectedChannel) : nullptr;
	if (!Channel)
	{
		DrawSectionDetails(*Section);
		return;
	}
	DrawChannelDetails(*Section, *Channel);
	ImGui::SameLine();
	DrawCurvePreview(*Channel);
}

void FSequencerPanel::DrawSectionDetails(UMovieSceneSection& Section)
{
	const char* CompletionNames[] = { "Keep State", "Restore State", "Project Default" };
	int32 Completion = static_cast<int32>(Section.EvalOptions.CompletionMode);
	ImGui::SetNextItemWidth(160.0f);
	if (ImGui::Combo("When Finished", &Completion, CompletionNames, IM_ARRAYSIZE(CompletionNames)))
		Section.EvalOptions.CompletionMode = static_cast<EMovieSceneCompletionMode>(Completion);
	bool bActive = Section.IsActive();
	if (ImGui::Checkbox("Active", &bActive)) { Section.SetIsActive(bActive); Player->Refresh(); }

	if (UMovieSceneFadeSection* Fade = Cast<UMovieSceneFadeSection>(&Section))
	{
		if (ImGui::ColorEdit3("Fade Color", Fade->FadeColor.RGBA)) Player->Refresh();
	}
	if (UMovieSceneCameraOverlaySection* Overlay = Cast<UMovieSceneCameraOverlaySection>(&Section))
	{
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::DragFloat("Letterbox Aspect", &Overlay->LetterboxAspect, 0.01f, 1.0f, 4.0f, "%.2f")) Player->Refresh();
	}
	if (UMovieScenePropertyTrack* Property = Cast<UMovieScenePropertyTrack>(SelectedTrack))
		ImGui::Text("Property: %s", Property->GetPropertyBinding().PropertyName.c_str());
	ImGui::TextDisabled("Select a channel row to edit keys");
}

void FSequencerPanel::DrawChannelDetails(UMovieSceneSection& Section, FMovieSceneFloatChannel& Channel)
{
	ImGui::BeginGroup();
	if (SelectedKey < 0 || SelectedKey >= Channel.GetNumKeys())
	{
		ImGui::TextDisabled("No key selected");
		ImGui::EndGroup();
		return;
	}

	const double TickRate = GetMovieScene()->GetTickResolution().AsDecimal();
	FMovieSceneFloatValue& Key = Channel.GetValues()[SelectedKey];
	bool bChanged = false;

	float Seconds = FrameToSeconds(Channel.GetTimes()[SelectedKey]);
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::DragFloat("Time", &Seconds, 0.01f, 0.0f, GetLengthSeconds(), "%.3f s"))
	{
		SelectedKey = Channel.SetKeyTime(SelectedKey, SecondsToFrame(Seconds));
		Player->Refresh();
		ImGui::EndGroup();
		return;
	}
	ImGui::SetNextItemWidth(120.0f);
	bChanged |= ImGui::DragFloat("Value", &Key.Value, 0.05f);

	const char* InterpNames[] = { "Linear", "Constant", "Cubic" };
	int32 Interp = static_cast<int32>(Key.InterpMode);
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::Combo("Interp", &Interp, InterpNames, IM_ARRAYSIZE(InterpNames)))
	{
		Key.InterpMode = static_cast<ERichCurveInterpMode>(Interp);
		bChanged = true;
	}
	const char* TangentNames[] = { "Auto", "User", "Break" };
	int32 TangentMode = static_cast<int32>(Key.TangentMode);
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::Combo("Tangent", &TangentMode, TangentNames, IM_ARRAYSIZE(TangentNames)))
	{
		Key.TangentMode = static_cast<ERichCurveTangentMode>(TangentMode);
		bChanged = true;
	}

	// 저장은 값/틱(UE와 같음). 편집은 값/초
	ImGui::BeginDisabled(Key.TangentMode == RCTM_Auto);
	float Arrive = static_cast<float>(Key.Tangent.ArriveTangent * TickRate);
	float Leave = static_cast<float>(Key.Tangent.LeaveTangent * TickRate);
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::DragFloat("Arrive /s", &Arrive, 0.05f))
	{
		Key.Tangent.ArriveTangent = static_cast<float>(Arrive / TickRate);
		if (Key.TangentMode == RCTM_User) Key.Tangent.LeaveTangent = Key.Tangent.ArriveTangent;
		bChanged = true;
	}
	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::DragFloat("Leave /s", &Leave, 0.05f))
	{
		Key.Tangent.LeaveTangent = static_cast<float>(Leave / TickRate);
		if (Key.TangentMode == RCTM_User) Key.Tangent.ArriveTangent = Key.Tangent.LeaveTangent;
		bChanged = true;
	}
	ImGui::EndDisabled();

	const bool bDelete = ImGui::Button("Delete Key");
	ImGui::EndGroup();

	if (bDelete)
	{
		Channel.RemoveKey(SelectedKey);
		SelectedKey = -1;
		Player->Refresh();
	}
	else if (bChanged)
	{
		Channel.AutoSetTangents();
		Player->Refresh();
	}
}

void FSequencerPanel::DrawCurvePreview(const FMovieSceneFloatChannel& Channel)
{
	const ImVec2 Size{ std::max(ImGui::GetContentRegionAvail().x, 100.0f), 160.0f };
	const ImVec2 P = ImGui::GetCursorScreenPos();
	ImDrawList* Draw = ImGui::GetWindowDrawList();
	Draw->AddRectFilled(P, { P.x + Size.x, P.y + Size.y }, IM_COL32(22, 24, 26, 255));
	ImGui::Dummy(Size);
	const float Length = GetLengthSeconds();
	if (Channel.GetNumKeys() == 0 || Length <= 0.0f) return;

	constexpr int32 Samples = 160;
	float Values[Samples + 1];
	float MinValue = Channel.GetValues()[0].Value;
	float MaxValue = MinValue;
	for (int32 Index = 0; Index <= Samples; ++Index)
	{
		Values[Index] = 0.0f;
		Channel.Evaluate(FFrameTime(SecondsToFrame(Length * Index / Samples)), Values[Index]);
		MinValue = std::min(MinValue, Values[Index]);
		MaxValue = std::max(MaxValue, Values[Index]);
	}
	const float Range = std::max(MaxValue - MinValue, 1e-3f);
	auto ToScreen = [&](float Seconds, float Value)
	{
		return ImVec2{ P.x + Size.x * Seconds / Length, P.y + Size.y - 8.0f - (Size.y - 16.0f) * (Value - MinValue) / Range };
	};
	for (int32 Index = 0; Index < Samples; ++Index)
		Draw->AddLine(ToScreen(Length * Index / Samples, Values[Index]), ToScreen(Length * (Index + 1) / Samples, Values[Index + 1]),
			IM_COL32(120, 200, 230, 255), 1.5f);
	for (int32 Index = 0; Index < Channel.GetNumKeys(); ++Index)
		DrawDiamond(Draw, ToScreen(FrameToSeconds(Channel.GetTimes()[Index]), Channel.GetValues()[Index].Value), 4.0f,
			Index == SelectedKey ? ColorKeySelected : ColorKey);
	char RangeText[48];
	std::snprintf(RangeText, sizeof(RangeText), "%.2f .. %.2f", MinValue, MaxValue);
	Draw->AddText({ P.x + 4.0f, P.y + 2.0f }, IM_COL32(150, 150, 150, 255), RangeText);
	const float PlayX = P.x + Size.x * GetPlayheadSeconds() / Length;
	Draw->AddLine({ PlayX, P.y }, { PlayX, P.y + Size.y }, ColorPlayhead);
}

void FSequencerPanel::DrawCameraCutDetails()
{
	UMovieScene* MovieScene = GetMovieScene();
	UMovieSceneCameraCutTrack* Track = Cast<UMovieSceneCameraCutTrack>(MovieScene->GetCameraCutTrack());
	if (!Track) return;

	ImGui::SetNextItemWidth(180.0f);
	BindingCombo("Camera", MovieScene, NewCutCamera);
	ImGui::SameLine();
	ImGui::BeginDisabled(!NewCutCamera.IsValid());
	if (ImGui::Button("Add Cut at Playhead"))
	{
		Track->AddNewCameraCut(FMovieSceneObjectBindingID(NewCutCamera), SecondsToFrame(SnapSeconds(GetPlayheadSeconds())));
		Player->Refresh();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::Checkbox("Can Blend", &Track->bCanBlend);

	UMovieSceneSection* RemoveSection = nullptr;
	int32 Index = 0;
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		UMovieSceneCameraCutSection* Cut = static_cast<UMovieSceneCameraCutSection*>(Section);
		ImGui::PushID(Index++);
		float Start = FrameToSeconds(Cut->GetInclusiveStartFrame());
		ImGui::SetNextItemWidth(90.0f);
		ImGui::DragFloat("##Start", &Start, 0.01f, 0.0f, GetLengthSeconds(), "%.2f s");
		if (ImGui::IsItemEdited()) Cut->SetStartFrame(SecondsToFrame(Start));
		if (ImGui::IsItemDeactivatedAfterEdit()) { Track->RearrangeAllSections(); Player->Refresh(); }
		ImGui::SameLine();
		FGuid Camera = Cut->GetCameraBindingID().GetGuid();
		ImGui::SetNextItemWidth(180.0f);
		if (BindingCombo("##Camera", MovieScene, Camera))
		{
			Cut->SetCameraBindingID(FMovieSceneObjectBindingID(Camera));
			Player->Refresh();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("X")) RemoveSection = Section;
		ImGui::PopID();
	}
	if (RemoveSection)
	{
		Track->RemoveSection(RemoveSection);
		Track->RearrangeAllSections();
		Player->Refresh();
	}
}

void FSequencerPanel::DrawEventDetails(UMovieSceneSection& Section)
{
	UMovieSceneEventTriggerSection& Events = static_cast<UMovieSceneEventTriggerSection&>(Section);
	const char* KindNames[] = { "HitStop", "Log" };
	int32 Kind = static_cast<int32>(NewEvent.Kind);
	ImGui::SetNextItemWidth(100.0f);
	if (ImGui::Combo("Kind", &Kind, KindNames, IM_ARRAYSIZE(KindNames))) NewEvent.Kind = static_cast<EMovieSceneEventKind>(Kind);
	ImGui::SameLine();
	if (NewEvent.Kind == EMovieSceneEventKind::HitStop)
	{
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragFloat("Duration", &NewEvent.Duration, 0.01f, 0.01f, 2.0f, "%.2f");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragFloat("Dilation", &NewEvent.Dilation, 0.01f, 0.0001f, 1.0f, "%.3f");
	}
	else
	{
		char Buffer[128];
		std::snprintf(Buffer, sizeof(Buffer), "%s", NewEvent.Message.c_str());
		ImGui::SetNextItemWidth(200.0f);
		if (ImGui::InputText("Message", Buffer, sizeof(Buffer))) NewEvent.Message = Buffer;
	}
	ImGui::SameLine();
	if (ImGui::Button("Add at Playhead"))
		Events.EventChannel.AddKey(SecondsToFrame(SnapSeconds(GetPlayheadSeconds())), NewEvent);

	int32 RemoveIndex = -1;
	for (int32 Index = 0; Index < Events.EventChannel.GetNumKeys(); ++Index)
	{
		const FMovieSceneEvent& Event = Events.EventChannel.Events[Index];
		ImGui::PushID(Index);
		float Seconds = FrameToSeconds(Events.EventChannel.Times[Index]);
		ImGui::SetNextItemWidth(90.0f);
		ImGui::DragFloat("##Time", &Seconds, 0.01f, 0.0f, GetLengthSeconds(), "%.2f s");
		if (ImGui::IsItemDeactivatedAfterEdit()) Events.EventChannel.SetKeyTime(Index, SecondsToFrame(SnapSeconds(Seconds)));
		ImGui::SameLine();
		if (Event.Kind == EMovieSceneEventKind::HitStop) ImGui::Text("HitStop %.2fs x%.3f", Event.Duration, Event.Dilation);
		else ImGui::Text("Log \"%s\"", Event.Message.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("X")) RemoveIndex = Index;
		ImGui::PopID();
	}
	if (RemoveIndex >= 0) Events.EventChannel.RemoveKey(RemoveIndex);
}
