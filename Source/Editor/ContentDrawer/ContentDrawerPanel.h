#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

class USkeletalMesh;

class FContentDrawerPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Content Drawer"; }
	void RenderFolderTree(const fs::path& Path, fs::path& SelectedPath);

	// .widget 파일을 더블클릭하면 불린다 (UI 에디터가 연다).
	void SetOpenWidgetCallback(std::function<void(const FString&)> InCallback) { OnOpenWidget = std::move(InCallback); }
	void SetCreateWidgetCallback(std::function<void(const FString&)> InCallback) { OnCreateWidget = std::move(InCallback); }
	// 뼈가 있는 FBX(= USkeletalMesh로 등록된 파일)를 더블클릭하면 불린다 (스켈레탈 메시 뷰어가 연다)
	void SetOpenSkeletalMeshCallback(std::function<void(USkeletalMesh*)> InCallback) { OnOpenSkeletalMesh = std::move(InCallback); }

private:
	std::function<void(const FString&)> OnOpenWidget;
	std::function<void(USkeletalMesh*)> OnOpenSkeletalMesh;
	std::function<void(const FString&)> OnCreateWidget;
	char NewAssetName[64] = "NewWidget";
	bool bOpenCreateWidget = false;

	fs::path AssetRootPath = "Assets";
	fs::path CurrentPath = "Assets";

	const FString FolderIconPath = "Assets/Icons/Folder.png";
	const FString FileIconPath = "Assets/Icons/File.png";
	
};