#pragma once

#include "Core/EngineString.h"
#include <thread>

// 에디터에서 Package.bat을 자식 프로세스로 돌리고 출력을 Output Log로 옮긴다.
// UE 에디터가 Package Project에서 UAT를 별도 프로세스로 띄우는 것과 같은 구조다.
class FGamePackager
{
public:
	~FGamePackager();

	bool Start(const FString& Config);
	// 매 프레임 부른다. 파이프에 이미 쌓인 출력만 읽고 바로 돌아오므로 에디터가 멈추지 않는다.
	void Tick();
	bool IsRunning() const { return Process != nullptr || CopyThread.joinable(); }
private:
	bool StartBuildAndPackage(const std::filesystem::path& Root, const FString& Config); // 기존 Start 본문 (Package.bat 실행)
	bool StartCopyOnly(const std::filesystem::path& Root);                               // 배포본: 복사만

	std::thread CopyThread;
	std::atomic<bool> bCopyDone{ false };
	FString CopyResult;   // 복사 스레드가 쓰고, bCopyDone이 true가 된 뒤 메인 스레드가 읽는다
	bool bCopyFailed = false;
	
	void DrainOutput();
	void Finish();

	HANDLE Process = nullptr;
	HANDLE OutputRead = nullptr;
	FString Pending;   // 아직 줄바꿈이 오지 않은 출력
};
