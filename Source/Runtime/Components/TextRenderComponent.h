#pragma once

#include "BillboardComponent.h"
#include "Text/Font.h"
#include "TextRenderComponent.generated.h"

UCLASS()
class UTextRenderComponent : public UPrimitiveComponent
{
	GENERATED_BODY()


public:
	UTextRenderComponent();
	virtual ~UTextRenderComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	virtual const FStaticMeshData* GetMeshData() const override;


	// 텍스트는 별도 렌더러에서 처리하므로 렌더 패킷을 생성하지 않음
	virtual void SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext) override {}

	const FString& GetText() const { return Text; }
	void SetText(const FString& InText) { Text = InText; }

	UFont* GetFont() const { return Font; }
	void SetFont(UFont* InFont) { Font = InFont; }

	float GetTextSize() const { return TextSize < 0 ? 0.1f : TextSize; }
	void SetTextSize(float InTextSize) { TextSize = InTextSize; }

private:
	bool RebuildPickingMesh() const;

	UPROPERTY(EditAnywhere)
	FString Text = "Text";
	UPROPERTY(EditAnywhere)
	UFont* Font = nullptr;
	UPROPERTY(EditAnywhere)
	float TextSize = 1.0f;

	mutable FStaticMeshData PickingMesh;
	mutable FString CachedText;
	mutable float CachedTextSize = -1.0f;
	mutable UFont* CachedFont = nullptr;
	mutable bool bHasPickingMesh = false;
};
