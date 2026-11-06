#pragma once

struct ImFont;
class UObject;
struct FProperty;

namespace FPropertyEditor
{
	// DetailsPanel.cpp의 DrawProperty를 그대로 옮긴다 (값이 바뀌면 PostEditChangeProperty 호출).
	void DrawProperty(UObject* Object, const FProperty& Property, ImFont* CustomFont = nullptr);
	// 기반 클래스부터 CPF_Edit 속성을 CollapsingHeader로 묶어 그린다 (DetailsPanel의 루프를 옮김).
	void DrawObjectProperties(UObject* Object, ImFont* CustomFont = nullptr);
}
// DetailsPanel.cpp는 이 헤더를 include하고 원래 함수 대신 FPropertyEditor::를 부르도록 바꾼다.