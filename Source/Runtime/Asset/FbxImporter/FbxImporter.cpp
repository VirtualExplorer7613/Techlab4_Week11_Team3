#include "EnginePCH.h"
#include "FbxImporter.h"

#include <fbxsdk.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <utility>

#include "Core/EngineLog.h"
#include "FbxBake.h"
#include "Render/SkeletalMeshData.h"
#include "Animation/AnimTypes.h"
#include "Math/EngineMath.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimData/AnimDataController.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	struct FFbxContext
	{
		FbxManager* Manager = nullptr;
		FbxScene* Scene = nullptr;

		~FFbxContext()
		{
			if (Manager)
			{
				Manager->Destroy();
			}
		}
	};

	const char* GetAttributeTypeName(FbxNode* Node)
	{
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
		if (!Attribute)
		{
			return "None";
		}

		switch (Attribute->GetAttributeType())
		{
		case FbxNodeAttribute::eNull:     return "Null";
		case FbxNodeAttribute::eSkeleton: return "Skeleton";
		case FbxNodeAttribute::eMesh:     return "Mesh";
		case FbxNodeAttribute::eCamera:   return "Camera";
		case FbxNodeAttribute::eLight:    return "Light";
		case FbxNodeAttribute::eMarker:   return "Marker";
		default:                          return "Other";
		}
	}

	// 디버그용: 노드 트리를 깊이만큼 들여쓰기해서 출력한다
	void PrintNodeTree(FbxNode* Node, int32 Depth)
	{
		const FbxMesh* Mesh = Node->GetMesh();
		HTR_LOG(Info, "{}{} [{}]{}", FString(static_cast<size_t>(Depth) * 2, ' '), Node->GetName(), GetAttributeTypeName(Node), Mesh && !Mesh->IsTriangleMesh() ? " (NOT TRIANGLE)" : "");

		for (int32 i = 0; i < Node->GetChildCount(); ++i)
		{
			PrintNodeTree(Node->GetChild(i), Depth + 1);
		}
	}

	// 재귀적으로 특정 노드에서 시작하여 자식 노드들을 dfs로 탐색하여 Bone을 모은다.
	void CollectBones(FbxNode* Node, int32 ParentIndex, TArray<FBoneInfo>& OutBones, TArray<FbxNode*>& OutBoneNodes)
	{
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
		if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			FBoneInfo Bone;
			Bone.Name = Node->GetName();
			Bone.ParentIndex = ParentIndex;

			ParentIndex = OutBones.Num();
			OutBones.Add(Bone);
			OutBoneNodes.Add(Node);
		}

		for (int32 i = 0; i < Node->GetChildCount(); ++i)
		{
			CollectBones(Node->GetChild(i), ParentIndex, OutBones, OutBoneNodes);
		}
	}
	struct FFbxVertexKey
	{
		int32 CP = -1;
		float NX = 0.0f, NY = 0.0f, NZ = 0.0f;
		float U = 0.0f, V = 0.0f;
		float TX = 0.0f, TY = 0.0f, TZ = 0.0f, TW = 0.0f; // Tangent만 다른 정점이 합쳐지지 않게

		bool operator==(const FFbxVertexKey&) const = default;
	};

	// 키 해시
	struct FFbxVertexKeyHash
	{
		size_t operator()(const FFbxVertexKey& Key) const
		{
			size_t Hash = std::hash<int32>{}(Key.CP);
			for (float F : { Key.NX, Key.NY, Key.NZ, Key.U, Key.V, Key.TX, Key.TY, Key.TZ, Key.TW })
			{
				Hash = Hash * 31 + std::hash<float>{}(F);
			}
			return Hash;
		}
	};

	// Layer Element(Tangent, Binormal 등)에서 코너 하나의 값을 꺼낸다. Mapping/Reference Mode를 직접 분기한다.
	template <typename TElement>
	bool GetElementValue(const TElement* Element, int32 ControlPoint, int32 PolygonVertex, int32 Polygon, FbxVector4& Out)
	{
		if (!Element)
		{
			return false;
		}

		int32 Index = 0;
		switch (Element->GetMappingMode())
		{
		case FbxGeometryElement::eByControlPoint:  Index = ControlPoint; break;
		case FbxGeometryElement::eByPolygonVertex: Index = PolygonVertex; break;
		case FbxGeometryElement::eByPolygon:       Index = Polygon; break;
		case FbxGeometryElement::eAllSame:         Index = 0; break;
		default:                                   return false;
		}

		if (Element->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
		{
			if (Index >= Element->GetIndexArray().GetCount())
			{
				return false;
			}
			Index = Element->GetIndexArray().GetAt(Index);
		}
		if (Index < 0 || Index >= Element->GetDirectArray().GetCount())
		{
			return false;
		}

		Out = Element->GetDirectArray().GetAt(Index);
		return true;
	}

	// FBX 방향 벡터 → 엔진 방향 벡터 (노드 회전 반영 후 Y 반전)
	FVector ToEngineDirection(const FbxAMatrix& NormalMatrix, const FbxVector4& Direction)
	{
		const FbxVector4 D = NormalMatrix.MultT(FbxVector4(Direction[0], Direction[1], Direction[2], 0.0));
		return FVector(static_cast<float>(D[0]), -static_cast<float>(D[1]), static_cast<float>(D[2]));
	}
	void CollectMeshNodes(FbxNode* Node, TArray<FbxNode*>& OutMeshNodes)
	{
		const FbxNodeAttribute* Attribute = Node->GetNodeAttribute();
		if (Attribute && Attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			OutMeshNodes.Add(Node);
		}
		for (int32 i = 0; i < Node->GetChildCount(); ++i)
		{
			CollectMeshNodes(Node->GetChild(i), OutMeshNodes);
		}
	}
	// 오른손 좌표계를 엔진의 왼손 좌표계로 변환한다. 
	FMatrix ToEngineMatrix(const FbxMatrix& Source)
	{
		FMatrix Out;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				Out.M[Row][Col] = static_cast<float>(Source.Get(Row, Col));
			}
		}
		for (int32 i = 0; i < 4; ++i)
		{
			Out.M[1][i] = -Out.M[1][i];
			Out.M[i][1] = -Out.M[i][1];
		}
		return Out;
	}

	// 각 Bone의 Global 행렬을 구한 뒤, 부모 기준의 Local 행렬로 바꿔 RefLocal에 저장한다.
	void ExtractRefLocals(TArray<FBoneInfo>& Bones, const TArray<FbxNode*>& BoneNodes)
	{
		TArray<FMatrix> Globals;
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			const FMatrix Global = ToEngineMatrix(BoneNodes[i]->EvaluateGlobalTransform());
			Globals.Add(Global);

			const int32 Parent = Bones[i].ParentIndex;
			Bones[i].RefLocal = Parent >= 0 ? Global * Globals[Parent].Inverse() : Global;
			// 기본값: 스킨 가중치가 없는 뼈는 기본 자세를 바인드 자세로 사용
			Bones[i].InvBindPose = Global.Inverse();
		}
	}

	// 디버그용: Scene의 축계·단위를 로그로 남긴다
	void LogSceneSpace(const char* Label, FbxScene* Scene)
	{
		FbxGlobalSettings& Settings = Scene->GetGlobalSettings();
		const FbxAxisSystem Axis = Settings.GetAxisSystem();

		int UpSign = 0, FrontSign = 0;
		const FbxAxisSystem::EUpVector Up = Axis.GetUpVector(UpSign);
		const FbxAxisSystem::EFrontVector Front = Axis.GetFrontVector(FrontSign);
		const bool bRightHanded = Axis.GetCoorSystem() == FbxAxisSystem::eRightHanded;

		HTR_LOG(Info, "FBX space [{}]: Up={}{}, FrontParity={}{}, {}, Unit={}cm",
			Label,
			UpSign > 0 ? "+" : "-", "?XYZ"[Up],
			FrontSign > 0 ? "+" : "-", Front == FbxAxisSystem::eParityEven ? "Even" : "Odd",
			bRightHanded ? "RH" : "LH",
			Settings.GetSystemUnit().GetScaleFactor());
	}

	// 오른손 Z-Up 좌표계와 미터 단위로 변환한다. 왼손 좌표계로의 변환은 나중에 한다.
	void ConvertToEngineSpace(FbxScene* Scene, FbxManager* Manager, bool bTriangulateMeshes)
	{
		LogSceneSpace("before", Scene);

		// AxisSystem 설정값 선언
		const FbxAxisSystem TargetAxis(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eRightHanded);
		if (Scene->GetGlobalSettings().GetAxisSystem() != TargetAxis)
		{
			// AxisSystem이 다르면 변환
			TargetAxis.ConvertScene(Scene);
		}

		if (Scene->GetGlobalSettings().GetSystemUnit() != FbxSystemUnit::m)
		{
			// 길이 단위가 미터가 아닌 경우 미터로 변환
			FbxSystemUnit::m.ConvertScene(Scene);
		}

		if (bTriangulateMeshes)
		{
			FbxGeometryConverter Converter(Manager);
			if (!Converter.Triangulate(Scene, true))
			{
				HTR_LOG(Warning, "FBX triangulate failed on some meshes");
			}
		}

		LogSceneSpace("after", Scene);
	}

	// FBX 씬을 읽어 좌표축과 단위를 변환하고, 요청한 경우 메시를 삼각형화한다.
	bool LoadScene(const FString& Path, FFbxContext& Context, bool bTriangulateMeshes = true)
	{
		Context.Manager = FbxManager::Create();

		FbxIOSettings* IOSettings = FbxIOSettings::Create(Context.Manager, IOSROOT);
		Context.Manager->SetIOSettings(IOSettings);

		FbxImporter* Importer = FbxImporter::Create(Context.Manager, "");
		if (!Importer->Initialize(Path.c_str(), -1, IOSettings))
		{
			// 파일 열기 실패
			HTR_LOG(Error, "FBX open failed: {} ({})", Path, Importer->GetStatus().GetErrorString());
			Importer->Destroy();
			return false;
		}

		Context.Scene = FbxScene::Create(Context.Manager, "ImportScene");
		// 파일을 읽어 Scene에 채운다
		const bool bImported = Importer->Import(Context.Scene);
		if (!bImported)
		{
			// import 실패
			HTR_LOG(Error, "FBX import failed: {} ({})", Path, Importer->GetStatus().GetErrorString());
		}

		Importer->Destroy();

		if (bImported)
		{
			ConvertToEngineSpace(Context.Scene, Context.Manager, bTriangulateMeshes);
		}

		return bImported;
	}
	// FBX 폴더와 textures 하위 폴더에서 이름(Stem)이 같은 이미지를 찾는다.
	// TGA를 PNG·DDS로 바꿔 넣는 경우가 많아 확장자가 달라도 찾는다. PreferredExtension을 먼저 본다.
	FString FindTextureByStem(const std::filesystem::path& Dir, const std::filesystem::path& Stem, const std::filesystem::path& PreferredExtension = {})
	{
		namespace fs = std::filesystem;
		std::error_code Error;
		const fs::path Extensions[] = { PreferredExtension, ".png", ".tga", ".dds", ".jpg" };
		for (const fs::path& Folder : { Dir, Dir / "textures", Dir / "Textures" })
		{
			for (const fs::path& Extension : Extensions)
			{
				if (Extension.empty())
				{
					continue;
				}
				fs::path Candidate = Folder / Stem;
				Candidate += Extension;
				if (fs::is_regular_file(Candidate, Error))
				{
					return Candidate.lexically_normal().generic_string();
				}
			}
		}
		return FString();
	}

	// 머티리얼 속성에 연결된 텍스처 파일 경로 찾기 (Step 7)
	FString FindTexturePath(FbxSurfaceMaterial* Material, const char* PropertyName, const FString& FbxPath)
	{
		const FbxProperty Property = Material->FindProperty(PropertyName);
		if (!Property.IsValid())
		{
			return FString();
		}
		const FbxFileTexture* Texture = Property.GetSrcObject<FbxFileTexture>(0);
		if (!Texture || FString(Texture->GetFileName()).empty())
		{
			return FString(); // 텍스처가 연결만 되고 파일이 지정되지 않은 경우
		}

		namespace fs = std::filesystem;
		const fs::path Dir = fs::path(FbxPath).parent_path();
		const fs::path FileName = fs::path(Texture->GetFileName()).filename();

		std::error_code Error;
		// 1. 기록된 경로 그대로: FBX 기준 상대 경로, 저장 당시 절대 경로
		for (const fs::path& Candidate : { Dir / Texture->GetRelativeFileName(), fs::path(Texture->GetFileName()) })
		{
			if (fs::is_regular_file(Candidate, Error))
			{
				return Candidate.lexically_normal().generic_string();
			}
		}

		// 2. 파일 이름으로 FBX 폴더와 textures 하위 폴더에서 찾기
		const FString Found = FindTextureByStem(Dir, FileName.stem(), FileName.extension());
		if (!Found.empty())
		{
			return Found;
		}

		HTR_LOG(Warning, "FBX texture not found: {} (put {} next to the FBX or in its textures folder)", Texture->GetFileName(), FileName.generic_string());
		return FString();
	}

	// 바인드 순간 메시 노드의 Global. Skin이 있으면 Cluster에 저장된 값을, 없으면 지금 Global을 쓴다.
	// 정점을 이 행렬로 구워야 InvBindPose(바인드 순간 뼈의 역)와 같은 시점이 된다.
	FbxAMatrix GetMeshBindGlobal(FbxNode* Node)
	{
		FbxMesh* Mesh = Node->GetMesh();
		if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
		{
			FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(0, FbxDeformer::eSkin));
			if (Skin->GetClusterCount() > 0)
			{
				FbxAMatrix MeshBind;
				Skin->GetCluster(0)->GetTransformMatrix(MeshBind);
				return MeshBind;
			}
		}
		return Node->EvaluateGlobalTransform();
	}

	void ExtractMesh(const TArray<FbxNode*>& MeshNodes, const FString& Path,
		const TMap<FbxNode*, FbxAMatrix>& RigidMeshBindGlobals, FStaticMeshData& MeshData)
	{
		// 노드별 정점·인덱스 생성
		int32 CPOffset = 0;
		TMap<FFbxVertexKey, uint32, FFbxVertexKeyHash> VertexCache;
		TArray<TArray<uint32>> SlotIndices;
		int32 TangentMeshCount = 0;
		for (FbxNode* Node : MeshNodes)
		{
			FbxMesh* Mesh = Node->GetMesh();
			const FbxAMatrix* RigidBind = RigidMeshBindGlobals.Find(Node);
			const FbxAMatrix Global = RigidBind ? *RigidBind : GetMeshBindGlobal(Node);
			FbxAMatrix NormalMatrix = Global;
			NormalMatrix.SetT(FbxVector4(0.0, 0.0, 0.0, 0.0)); // NormalMatrix에서 이동에 대한 성분을 뺌

			FbxStringList UVSetNames;
			Mesh->GetUVSetNames(UVSetNames);
			const char* UVSetName = UVSetNames.GetCount() > 0 ? UVSetNames[0].Buffer() : nullptr;

			// Tangent가 저장된 메시면 그대로 쓴다. 없으면 나중에 MikkTSpace로 계산한다.
			const FbxGeometryElementTangent* TangentElement = Mesh->GetElementTangent(0);
			const FbxGeometryElementBinormal* BinormalElement = Mesh->GetElementBinormal(0);
			if (TangentElement)
			{
				++TangentMeshCount;
			}

			// 머티리얼 슬롯 등록. 여러 노드가 같은 머티리얼을 쓰면 슬롯 하나를 공유한다.
			const int32 MaterialCount = Node->GetMaterialCount();
			TArray<int32> SlotOf; // 노드의 머티리얼 번호 → 전체 슬롯 번호
			for (int32 m = 0; m < std::max(MaterialCount, 1); ++m)
			{
				//Material은 메시가 아니라 노드에 붙어있음. 없는 노드는 Default
				FbxSurfaceMaterial* Material = MaterialCount > 0 ? Node->GetMaterial(m) : nullptr;
				const FString Name = Material ? Material->GetName() : "Default";

				int32 SlotIndex = -1;
				for (int32 s = 0; s < MeshData.MaterialSlots.Num(); ++s)
				{
					if (MeshData.MaterialSlots[s].Name == Name)
					{
						SlotIndex = s;
						break;
					}
				}
				if (SlotIndex < 0)
				{
					FStaticMaterialSlot Slot;
					Slot.Name = Name;
					if (Material)
					{
						// 텍스처 경로 (Step 7)
						Slot.DiffuseTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sDiffuse, Path);
						Slot.NormalTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sNormalMap, Path);
						if (Slot.NormalTexturePath.empty())
						{
							Slot.NormalTexturePath = FindTexturePath(Material, FbxSurfaceMaterial::sBump, Path);
						}
					}
					SlotIndex = MeshData.MaterialSlots.Num();
					MeshData.MaterialSlots.Add(Slot);
				}
				SlotOf.Add(SlotIndex);
			}
			SlotIndices.SetNum(MeshData.MaterialSlots.Num());
			FbxGeometryElementMaterial* MaterialElement = Mesh->GetElementMaterial();

			for (int32 p = 0; p < Mesh->GetPolygonCount(); ++p)
			{
				// 2-1. 삼각형 아닌 폴리곤 제외
				if (Mesh->GetPolygonSize(p) != 3)
				{
					continue;
				}

				// 2-2. 코너 3개 → 정점 3개
				uint32 Corner[3];
				for (int32 c = 0; c < 3; ++c)
				{
					//p번 삼각형의 c번째 코너(a,b,c)는 몇번 control Point인가?
					const int32 CP = Mesh->GetPolygonVertex(p, c);
					const FbxVector4 P = Global.MultT(Mesh->GetControlPointAt(CP));

					FVertex Vertex;
					Vertex.Position = FVector(static_cast<float>(P[0]), -static_cast<float>(P[1]), static_cast<float>(P[2]));
					
					//Normal 추출
					Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
					FbxVector4 N;
					// p번 삼각형의 c번째 코너의 노멀을 N에 받음.
					if (Mesh->GetPolygonVertexNormal(p, c, N))
					{
						N = NormalMatrix.MultT(N); //노드의 회전을 반영해준 노말값이됨
						Vertex.Normal = FVector(static_cast<float>(N[0]), -static_cast<float>(N[1]), static_cast<float>(N[2]))
							.GetSafeNormal(SMALL_NUMBER, FVector(0.0f, 0.0f, 1.0f));

					}
					FbxVector2 UV;
					bool bUnmapped = false;
					if (UVSetName && Mesh->GetPolygonVertexUV(p, c, UVSetName, UV, bUnmapped) && !bUnmapped)
					{
						Vertex.UV.X = static_cast<float>(UV[0]);
						Vertex.UV.Y = 1.0f - static_cast<float>(UV[1]);
						// FBX는 텍스처 이미지의 왼쪽 아래가 원점, D3D는 왼쪽 위가 원점
					}

					// Tangent. W = 0은 "임포트한 값 없음" 표시로, BuildTangentSpace가 이 정점만 계산한다.
					Vertex.Tangent = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
					const int32 PolygonVertex = Mesh->GetPolygonVertexIndex(p) + c;
					FbxVector4 T;
					if (GetElementValue(TangentElement, CP, PolygonVertex, p, T))
					{
						const FVector Tangent = ToEngineDirection(NormalMatrix, T).GetSafeNormal();
						float Handedness = 0.0f;
						FbxVector4 B;
						if (GetElementValue(BinormalElement, CP, PolygonVertex, p, B))
						{
							// Binormal은 FBX의 +V 방향이다. 엔진은 V를 뒤집었으므로(1 - V) 엔진 기준 Bitangent는 그 반대
							const FVector Bitangent = -ToEngineDirection(NormalMatrix, B);
							Handedness = Vertex.Normal.Cross(Tangent).Dot(Bitangent) < 0.0f ? -1.0f : 1.0f;
						}
						else if (T[3] != 0.0)
						{
							// Y 반전(Cross 부호 반전)과 V 반전(Bitangent 반전)이 서로 상쇄되어 FBX의 W를 그대로 쓴다
							Handedness = T[3] < 0.0 ? -1.0f : 1.0f;
						}
						if (Handedness != 0.0f && Tangent.SizeSquared() > 0.0f)
						{
							Vertex.Tangent = FVector4(Tangent, Handedness);
						}
					}

					// 2-3. 같은 정점이면 재사용, 없으면 추가
					const FFbxVertexKey Key{ CPOffset + CP, Vertex.Normal.X, Vertex.Normal.Y, Vertex.Normal.Z, Vertex.UV.X, Vertex.UV.Y,
						Vertex.Tangent.X, Vertex.Tangent.Y, Vertex.Tangent.Z, Vertex.Tangent.W };
					if (const uint32* Found = VertexCache.Find(Key))
					{
						Corner[c] = *Found;
					}
					else
					{
						Corner[c] = static_cast<uint32>(MeshData.Vertices.Num());
						VertexCache.Add(Key, Corner[c]);
						MeshData.Vertices.Add(Vertex);
						MeshData.LODSourceVertices.Add({ CPOffset + CP, -1, -1 });
					}
				}
				// 2-4. 이 삼각형의 슬롯 바구니에 인덱스 추가
				int32 LocalSlot = 0;
				if (MaterialElement)
				{
					const int32 Lookup = MaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon ? p : 0;
					LocalSlot = MaterialElement->GetIndexArray().GetAt(Lookup);
				}
				if (LocalSlot < 0 || LocalSlot >= std::max(MaterialCount, 1))
				{
					LocalSlot = 0;
				}
				TArray<uint32>& Bucket = SlotIndices[SlotOf[LocalSlot]];
				// 인덱스(FBX는 반시계기준.엔진에 맞게 시계방향으로 보정)
				Bucket.Add(Corner[0]);
				Bucket.Add(Corner[2]);
				Bucket.Add(Corner[1]);
			}

			CPOffset += Mesh->GetControlPointsCount();
		}

		// 3. 바구니 → Section
		for (int32 s = 0; s < SlotIndices.Num(); ++s)
		{
			if (SlotIndices[s].IsEmpty())
			{
				continue;
			}
			FStaticMeshSection Section;
			Section.StartIndex = static_cast<uint32>(MeshData.Indices.Num());
			Section.IndexCount = static_cast<uint32>(SlotIndices[s].Num());
			Section.MaterialSlotIndex = static_cast<uint32>(s);
			MeshData.Indices.Append(SlotIndices[s]);
			MeshData.Sections.Add(Section);
		}

		// 3-1. 임포트한 Tangent가 없는 정점만 MikkTSpace로 계산.
		// Skin 추출(LODSourceVertices로 Control Point를 찾음)보다 먼저 해야 나뉜 정점도 가중치를 받는다.
		const int32 VertexCountBeforeTangent = MeshData.Vertices.Num();
		MeshData.BuildTangentSpace(true);
		HTR_LOG(Info, "FBX tangent: imported meshes {}/{}, split verts {}",
			TangentMeshCount, MeshNodes.Num(), MeshData.Vertices.Num() - VertexCountBeforeTangent);

		// 4. AABB
		if (!MeshData.Vertices.IsEmpty())
		{
			FVector Min = MeshData.Vertices[0].Position;
			FVector Max = Min;
			for (const FVertex& Vertex : MeshData.Vertices)
			{
				Min.X = std::min(Min.X, Vertex.Position.X);
				Min.Y = std::min(Min.Y, Vertex.Position.Y);
				Min.Z = std::min(Min.Z, Vertex.Position.Z);
				Max.X = std::max(Max.X, Vertex.Position.X);
				Max.Y = std::max(Max.Y, Vertex.Position.Y);
				Max.Z = std::max(Max.Z, Vertex.Position.Z);
			}
			MeshData.AABB.Min = Min;
			MeshData.AABB.Max = Max;
		}

		// 5. 결과 로그
		HTR_LOG(Info, "FBX mesh: nodes {}, verts {}, tris {}, sections {}",
			MeshNodes.Num(), MeshData.Vertices.Num(), MeshData.Indices.Num() / 3, MeshData.Sections.Num());

		// 6. 상자 검사
		FString Error;
		if (!MeshData.Validate(Error))
		{
			HTR_LOG(Error, "FBX mesh invalid: {}", Error);
		}
	}

	// Skin(가중치) + InvBindPose 추출 (Step 6)
	// 스킨 디포머가 붙은 메시가 하나라도 있으면 SkeletalMesh로 본다.
	// 언리얼 FFbxImporter::GetSceneInfo/GetImportType(FbxMainImport.cpp)과 같은 기준: 뼈 노드 유무가 아니라 메시의 eSkin 디포머
	bool HasSkinnedMesh(FbxScene* Scene)
	{
		for (int32 i = 0; i < Scene->GetGeometryCount(); ++i)
		{
			FbxGeometry* Geometry = Scene->GetGeometry(i);
			if (Geometry->GetAttributeType() == FbxNodeAttribute::eMesh &&
				static_cast<FbxMesh*>(Geometry)->GetDeformerCount(FbxDeformer::eSkin) > 0)
			{
				return true;
			}
		}
		return false;
	}

	struct FFbxSkinData
	{
		TArray<FSkinWeight> ControlPointWeights;
		TMap<FbxNode*, FbxAMatrix> RigidMeshBindGlobals;
	};

	FFbxSkinData ExtractSkin(const TArray<FbxNode*>& MeshNodes, const TArray<FbxNode*>& BoneNodes, TArray<FBoneInfo>& Bones)
	{
		FFbxSkinData Result;
		if (Bones.IsEmpty())
		{
			return Result;
		}

		// 1. 뼈 노드 → 뼈 번호
		TMap<FbxNode*, int32> BoneIndexOf;
		for (int32 i = 0; i < BoneNodes.Num(); ++i)
		{
			BoneIndexOf.Add(BoneNodes[i], i);
		}

		// 2. 기본 자세 Global (바인드 자세와 비교용)
		TArray<FMatrix> RefGlobals;
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			const int32 Parent = Bones[i].ParentIndex;
			RefGlobals.Add(Parent >= 0 ? Bones[i].RefLocal * RefGlobals[Parent] : Bones[i].RefLocal);
		}

		// 3. Control Point별 스킨 가중치와 본 하위의 강체 부품을 수집한다.
		TArray<TArray<std::pair<int32, float>>> CPInfluences;
		TMap<FbxNode*, int32> RigidMeshBones;
		TMap<int32, FbxAMatrix> BoneBindGlobals;
		int32 CPOffset = 0;
		int32 ClusterCount = 0;
		int32 RigidControlPointCount = 0;
		float MaxBindDiff = 0.0f;
		for (FbxNode* Node : MeshNodes)
		{
			FbxMesh* Mesh = Node->GetMesh();
			const int32 CPCount = Mesh->GetControlPointsCount();
			const int32 CPEnd = CPOffset + CPCount;
			CPInfluences.SetNum(CPEnd);
			Result.ControlPointWeights.SetNum(CPEnd);

			const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
			if (SkinCount == 0)
			{
				for (FbxNode* Parent = Node->GetParent(); Parent; Parent = Parent->GetParent())
				{
					if (const int32* BoneIndex = BoneIndexOf.Find(Parent))
					{
						RigidMeshBones.Add(Node, *BoneIndex);
						for (int32 CP = CPOffset; CP < CPEnd; ++CP)
						{
							Result.ControlPointWeights[CP].BoneIndices[0] = *BoneIndex;
							Result.ControlPointWeights[CP].Weights[0] = 1.0f;
						}
						RigidControlPointCount += CPEnd - CPOffset;
						break;
					}
				}
			}

			for (int32 d = 0; d < SkinCount; ++d)
			{
				FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(d, FbxDeformer::eSkin));
				for (int32 c = 0; c < Skin->GetClusterCount(); ++c)
				{
					FbxCluster* Cluster = Skin->GetCluster(c);
					const int32* BoneFound = BoneIndexOf.Find(Cluster->GetLink());
					if (!BoneFound)
					{
						continue;
					}
					const int32 Bone = *BoneFound;
					const int32 Count = Cluster->GetControlPointIndicesCount();
					const int* Indices = Cluster->GetControlPointIndices();
					const double* Weights = Cluster->GetControlPointWeights();
					bool bHasInfluence = false;
					for (int32 k = 0; k < Count; ++k)
					{
						const int32 CP = Indices[k];
						if (Weights[k] > 0.0 && CP >= 0 && CP < CPCount)
						{
							CPInfluences[CPOffset + CP].Add({ Bone, static_cast<float>(Weights[k]) });
							bHasInfluence = true;
						}
					}
					if (!bHasInfluence)
					{
						continue;
					}
					++ClusterCount;

					// 3-1. InvBindPose = 바인드 순간 뼈 행렬의 역
					FbxAMatrix BindMatrix;
					Cluster->GetTransformLinkMatrix(BindMatrix);
					const FMatrix Bind = ToEngineMatrix(BindMatrix);
					Bones[Bone].InvBindPose = Bind.Inverse();
					BoneBindGlobals.Add(Bone, BindMatrix);

					// 3-2. 바인드 자세 vs 기본 자세 차이 (T-Pose 검증용)
					for (int32 r = 0; r < 4; ++r)
					{
						for (int32 k = 0; k < 4; ++k)
						{
							MaxBindDiff = std::max(MaxBindDiff, std::abs(Bind.M[r][k] - RefGlobals[Bone].M[r][k]));
						}
					}
				}
			}

			CPOffset = CPEnd;
		}

		for (const auto& [Node, BoneIndex] : RigidMeshBones)
		{
			if (const FbxAMatrix* BindGlobal = BoneBindGlobals.Find(BoneIndex))
			{
				// 클러스터와 같은 본을 쓰는 부품의 정점도 그 본의 바인드 공간에 맞춘다.
				const FbxAMatrix BoneRefGlobal = BoneNodes[BoneIndex]->EvaluateGlobalTransform();
				Result.RigidMeshBindGlobals.Add(Node, *BindGlobal * BoneRefGlobal.Inverse() * Node->EvaluateGlobalTransform());
			}
		}

		// 4. Control Point별 상위 4개 + 정규화
		int32 Unweighted = 0;
		for (int32 cp = 0; cp < CPInfluences.Num(); ++cp)
		{
			FSkinWeight& Out = Result.ControlPointWeights[cp];
			if (Out.Weights[0] > 0.0f)
			{
				continue; // 부모 본에 이미 강체로 연결된 점
			}

			TArray<std::pair<int32, float>>& List = CPInfluences[cp];
			std::sort(List.begin(), List.end(), [](const std::pair<int32, float>& A, const std::pair<int32, float>& B) { return A.second > B.second; });

			const int32 Keep = std::min(List.Num(), 4);
			float Sum = 0.0f;
			for (int32 k = 0; k < Keep; ++k)
			{
				Sum += List[k].second;
			}

			if (Sum <= 0.0f)
			{
				Out.Weights[0] = 1.0f; // 영향 없는 점은 0번(루트) 뼈를 따라감
				++Unweighted;
				continue;
			}
			for (int32 k = 0; k < Keep; ++k)
			{
				Out.BoneIndices[k] = List[k].first;
				Out.Weights[k] = List[k].second / Sum;
			}
		}

		HTR_LOG(Info, "FBX skin: clusters {}, rigid meshes {} ({} CPs), unweighted CPs {}, bind-vs-ref max diff {}",
			ClusterCount, RigidMeshBones.Num(), RigidControlPointCount, Unweighted, MaxBindDiff);
		return Result;
	}

	void ApplyControlPointWeights(const TArray<FSkinWeight>& CPWeights, FSkeletalMeshData& Data)
	{
		if (CPWeights.IsEmpty())
		{
			return;
		}

		// Tangent 계산으로 분리된 정점도 원래 Control Point의 가중치를 받는다.
		Data.SkinWeights.SetNum(Data.Mesh.Vertices.Num());
		for (int32 v = 0; v < Data.Mesh.Vertices.Num(); ++v)
		{
			const int32 CP = Data.Mesh.LODSourceVertices[v].PositionIndex;
			if (CP >= 0 && CP < CPWeights.Num())
			{
				Data.SkinWeights[v] = CPWeights[CP];
			}
		}
	}

	FQuat QuatFromRotationRows(const FVector& X, const FVector& Y, const FVector& Z)
	{
		const float Trace = X.X + Y.Y + Z.Z;

		if (Trace > 0.0f)
		{
			const float FourW = 2.0f * std::sqrt(1.0f + Trace);
			return FQuat((Y.Z - Z.Y) / FourW, (Z.X - X.Z) / FourW, (X.Y - Y.X) / FourW, 0.25f * FourW);
		}

		if (X.X >= Y.Y && X.X >= Z.Z)
		{
			const float FourX = 2.0f * std::sqrt(1.0f + X.X - Y.Y - Z.Z);
			return FQuat(0.25f * FourX, (X.Y + Y.X) / FourX, (X.Z + Z.X) / FourX, (Y.Z - Z.Y) / FourX);
		}

		if (Y.Y >= Z.Z)
		{
			const float FourY = 2.0f * std::sqrt(1.0f + Y.Y - X.X - Z.Z);
			return FQuat((X.Y + Y.X) / FourY, 0.25f * FourY, (Y.Z + Z.Y) / FourY, (Z.X - X.Z) / FourY);
		}

		const float FourZ = 2.0f * std::sqrt(1.0f + Z.Z - X.X - Y.Y);
		return FQuat((X.Z + Z.X) / FourZ, (Y.Z + Z.Y) / FourZ, 0.25f * FourZ, (X.Y - Y.X) / FourZ);
	}

	bool TryDecomposeBoneLocal(const FMatrix& Local, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale)
	{
		const FVector Position(Local.M[3][0], Local.M[3][1], Local.M[3][2]);

		FVector X(Local.M[0][0], Local.M[0][1], Local.M[0][2]);
		FVector Y(Local.M[1][0], Local.M[1][1], Local.M[1][2]);
		FVector Z(Local.M[2][0], Local.M[2][1], Local.M[2][2]);

		FVector Scale(X.Size(), Y.Size(), Z.Size());

		const auto IsFinite = [](const FVector& V)
		{
			return std::isfinite(V.X) && std::isfinite(V.Y)	&& std::isfinite(V.Z);
		};

		constexpr float MinScale = 1e-6f;
		if (!IsFinite(Position) || !IsFinite(Scale)	|| Scale.X <= MinScale || Scale.Y <= MinScale || Scale.Z <= MinScale)
		{
			return false;
		}

		X = X / Scale.X;
		Y = Y / Scale.Y;
		Z = Z / Scale.Z;

		// 세 축이 서로 직교해야 위치·회전·크기로 표현할 수 있다.
		constexpr float OrthoTolerance = 1e-3f;	if (std::abs(X.Dot(Y)) > OrthoTolerance	|| std::abs(X.Dot(Z)) > OrthoTolerance || std::abs(Y.Dot(Z)) > OrthoTolerance)
		{
			return false;
		}

		// 반사가 있는 경우 부호를 X 스케일에 저장합니다.
		if (X.Cross(Y).Dot(Z) < 0.0f)
		{
			X = X * -1.0f;
			Scale.X = -Scale.X;
		}

		OutPosition = Position;
		OutRotation = QuatFromRotationRows(X, Y, Z).Normalized();
		OutScale = Scale;
		return true;
	}

}


namespace
{
	// Import와 Bake 로드 결과를 같은 형식으로 남겨 두 경로를 로그로 비교한다 (Step 8 완료 기준)
	void LogLoadSummary(const FString& Path, const char* Source, double ElapsedMs, const FSkeletalMeshData& Data)
	{
		HTR_LOG(Info, "[FBX] {} {} in {:.1f} ms (verts {}, tris {}, sections {}, bones {}, weights {})",
			Path, Source, ElapsedMs, Data.Mesh.Vertices.Num(), Data.Mesh.Indices.Num() / 3,
			Data.Mesh.Sections.Num(), Data.Bones.Num(), Data.SkinWeights.Num());
	}

	double MillisecondsSince(std::chrono::steady_clock::time_point Start)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	}

	std::optional<int64> GetAnimationSourceTime(const FString& Path)
	{
		int64 SourceTime = 0;
		if (FFbxBake::GetSourceTime(Path, SourceTime))
		{
			return SourceTime;
		}
		return std::nullopt;
	}

	UAnimSequence* CreateAnimSequence(const FString& Path, int32 SampleRate, const FFbxAnimBakeData& Data)
	{
		TUniquePtr<UAnimSequence> Sequence(NewObject<UAnimSequence>());
		if (!Sequence || !Sequence->InitializeModel())
		{
			HTR_LOG(Error, "Failed to initialize animation sequence: {}", Path);
			return nullptr;
		}

		TUniquePtr<UAnimDataModel> Model(Sequence->GetDataModel());
		TUniquePtr<UAnimDataController> Controller(NewObject<UAnimDataController>());
		if (!Controller)
		{
			HTR_LOG(Error, "Failed to create animation data controller: {}", Path);
			return nullptr;
		}

		Controller->SetModel(Model.get());
		Controller->SetFrameRate(FFrameRate(SampleRate, 1));
		if (!Controller->SetNumberOfFrames(FFrameNumber(Data.NumFrames)))
		{
			HTR_LOG(Error, "Failed to set animation frame count: {}", Data.NumFrames);
			return nullptr;
		}

		for (const FBoneAnimationTrack& Track : Data.Tracks)
		{
			const FRawAnimSequenceTrack& Keys = Track.InternalTrack;
			if (!Controller->AddBoneTrack(Track.Name) ||
				!Controller->SetBoneTrackKeys(Track.Name, Keys.PosKeys, Keys.RotKeys, Keys.ScaleKeys))
			{
				HTR_LOG(Error, "Failed to store animation track '{}'", Track.Name.ToString());
				return nullptr;
			}
		}

		(void)Model.release();
		return Sequence.release();
	}
}

TUniquePtr<FSkeletalMeshData> FFbxImporter::LoadSkeletalMeshData(const FString& Path)
{
	const auto Start = std::chrono::steady_clock::now();
	const FString BinPath = Path + ".bin";

	// 0. Bake가 있고 원본이 그대로면 FBX SDK를 거치지 않는다
	if (TUniquePtr<FSkeletalMeshData> Baked = FFbxBake::Read(BinPath, Path))
	{
		LogLoadSummary(Path, "loaded from bake", MillisecondsSince(Start), *Baked);
		return Baked;
	}

	FFbxContext Context;

	// 1. 파일열기
	if (!LoadScene(Path, Context))
	{
		return nullptr;
	}

	TUniquePtr<FSkeletalMeshData> Data = MakeUnique<FSkeletalMeshData>();
	TArray<FbxNode*> BoneNodes;
	CollectBones(Context.Scene->GetRootNode(), -1, Data->Bones, BoneNodes);
	// 스킨된 메시가 없으면 리그가 딸려 왔어도 StaticMesh다. 이후 단계(Bake 형식, LoadAsset 분기)는 뼈 유무로 갈리므로 뼈를 비운다
	if (!HasSkinnedMesh(Context.Scene))
	{
		if (!Data->Bones.IsEmpty())
		{
			HTR_LOG(Info, "FBX has {} bones but no skinned mesh, importing as StaticMesh: {}", Data->Bones.Num(), Path);
		}
		Data->Bones.Reset();
		BoneNodes.Reset();
	}
	else if (Data->Bones.IsEmpty())
	{
		HTR_LOG(Warning, "FBX has skinned mesh but no skeleton node: {}", Path);
	}
	ExtractRefLocals(Data->Bones, BoneNodes);
	TArray<FbxNode*> MeshNodes;
	CollectMeshNodes(Context.Scene->GetRootNode(), MeshNodes);
	const FFbxSkinData SkinData = ExtractSkin(MeshNodes, BoneNodes, Data->Bones);
	ExtractMesh(MeshNodes, Path, SkinData.RigidMeshBindGlobals, Data->Mesh);
	ApplyControlPointWeights(SkinData.ControlPointWeights, *Data);
	LogLoadSummary(Path, "imported", MillisecondsSince(Start), *Data);

	// 2. 다음 로드부터는 Bake를 읽는다
	FFbxBake::Write(BinPath, Path, *Data);
	return Data;
}

// 디버그용 출력
void FFbxImporter::PrintFbxInfo(const FString& Path)
{
	FFbxContext Context;
	if (!LoadScene(Path, Context))
	{
		return;
	}

	HTR_LOG(Info, "FBX loaded: {}", Path);
	HTR_LOG(Info, "SDK Version: {}", Context.Manager->GetVersion());
	HTR_LOG(Info, "Root Children: {}", Context.Scene->GetRootNode()->GetChildCount());
	HTR_LOG(Info, "Node Count: {}", Context.Scene->GetNodeCount());
	int32 NonTriangleMeshes = 0;
	const int32 MeshCount = Context.Scene->GetSrcObjectCount<FbxMesh>();
	for (int32 i = 0; i < MeshCount; ++i)
	{
		if (!Context.Scene->GetSrcObject<FbxMesh>(i)->IsTriangleMesh())
		{
			++NonTriangleMeshes;
		}
	}
	HTR_LOG(Info, "Meshes: {} (non-triangle: {})", MeshCount, NonTriangleMeshes);

	PrintNodeTree(Context.Scene->GetRootNode(), 0);
}

void FFbxImporter::PrintSkeleton(const FSkeletalMeshData& Data)
{
	HTR_LOG(Info, "Bones: {}", Data.Bones.Num());

	TArray<int32> Depths;
	for (int32 i = 0; i < Data.Bones.Num(); ++i)
	{
		const FBoneInfo& Bone = Data.Bones[i];
		const int32 Depth = Bone.ParentIndex >= 0 ? Depths[Bone.ParentIndex] + 1 : 0;
		Depths.Add(Depth);

		HTR_LOG(Info, "{}[{}] {}", FString(static_cast<size_t>(Depth) * 2, ' '), i, Bone.Name);
	}
}

static UAnimSequence* LoadAnimSequenceInContext(const FString& Path, int32 StackIndex, int32 SampleRate,
	FFbxContext& Context, std::optional<int64> SourceTime)
{
	if (SampleRate <= 0)
	{
		HTR_LOG(Error, "Invalid SampleRate");
		return nullptr;
	}

	if (StackIndex < 0)
	{
		HTR_LOG(Error, "Invalid animation stack index: {}", StackIndex);
		return nullptr;
	}

	const auto Start = std::chrono::steady_clock::now();
	const FString BinPath = std::format("{}.anim_{}_{}.bin", Path, StackIndex, SampleRate);
	if (SourceTime)
	{
		const TUniquePtr<FFbxAnimBakeData> Baked = FFbxBake::ReadAnimation(BinPath, *SourceTime, StackIndex, SampleRate);
		if (UAnimSequence* Cached = Baked ? CreateAnimSequence(Path, SampleRate, *Baked) : nullptr)
		{
			HTR_LOG(Info, "[FBX ANIM] {} loaded from bake in {:.1f} ms", BinPath, MillisecondsSince(Start));
			return Cached;
		}
	}

	if (!Context.Scene && !LoadScene(Path, Context, /*bTriangulateMeshes=*/false))
	{
		return nullptr;
	}

	FbxScene* Scene = Context.Scene;

	const int32 StackCount = Scene->GetSrcObjectCount<FbxAnimStack>();

	if (StackIndex >= StackCount)
	{
		HTR_LOG(Error, "Invalid animation stack index: {} (stack count: {})", StackIndex, StackCount);
		return nullptr;
	}

	FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
	if (!AnimStack)
	{
		HTR_LOG(Error, "Failed to get animation stack: {}", StackIndex);
		return nullptr;
	}

	Scene->SetCurrentAnimationStack(AnimStack);

	const FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();
	const double StartSeconds = TimeSpan.GetStart().GetSecondDouble();
	const double EndSeconds = TimeSpan.GetStop().GetSecondDouble();
	const double DurationSeconds = EndSeconds - StartSeconds;

	if (DurationSeconds <= 0.0)
	{
		HTR_LOG(Error, "Invalid animation duration: {} seconds", DurationSeconds);
		return nullptr;
	}

	HTR_LOG(Info, "FBX animation '{}': {:.3f}s to {:.3f}s, duration {:.3f}s", AnimStack->GetName(), StartSeconds, EndSeconds, DurationSeconds);

	const double FrameCount = std::floor(DurationSeconds * SampleRate);

	if (!std::isfinite(FrameCount) || FrameCount < 1.0 || FrameCount >= std::numeric_limits<int32>::max())
	{
		HTR_LOG(Error, "Unsupported animation frame count: {}", FrameCount);
		return nullptr;
	}

	const int32 NumFrames = static_cast<int32>(FrameCount);
	const int32 NumKeys = NumFrames + 1;
	const double SampleInterval = 1.0 / SampleRate;

	TArray<FBoneInfo> Bones;
	TArray<FbxNode*> BoneNodes;

	CollectBones(Scene->GetRootNode(), -1, Bones, BoneNodes);

	if (Bones.IsEmpty())
	{
		HTR_LOG(Error, "No skeleton bones in FBX: {}", Path);
		return nullptr;
	}

	FFbxAnimBakeData Data;
	Data.NumFrames = NumFrames;
	Data.Tracks.SetNum(Bones.Num());
	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		Data.Tracks[BoneIndex].Name = FName(Bones[BoneIndex].Name);
	}

	TArray<FMatrix> Globals;
	Globals.SetNum(Bones.Num());

	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		FbxTime SampleTime;
		SampleTime.SetSecondDouble(StartSeconds + KeyIndex * SampleInterval);

		for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
		{
			const FMatrix Global = ToEngineMatrix(BoneNodes[BoneIndex]->EvaluateGlobalTransform(SampleTime));
			Globals[BoneIndex] = Global;

			FMatrix Local = Global;
			const int32 ParentIndex = Bones[BoneIndex].ParentIndex;

			if (ParentIndex >= 0)
			{
				const FMatrix& ParentGlobal = Globals[ParentIndex];
				const float Determinant = ParentGlobal.Determinant();

				if (!std::isfinite(Determinant) || FMath::IsNearlyZero(Determinant))
				{
					HTR_LOG(Error, "Cannot invert parent of bone '{}' at key {}", Bones[BoneIndex].Name, KeyIndex);
					return nullptr;
				}

				Local = Global * ParentGlobal.Inverse();
			}

			FVector Position;
			FQuat Rotation;
			FVector Scale;

			if (!TryDecomposeBoneLocal(Local, Position, Rotation, Scale))
			{
				HTR_LOG(Error, "Cannot decompose bone '{}' at key {}",	Bones[BoneIndex].Name, KeyIndex);
				return nullptr;
			}

			FRawAnimSequenceTrack& Track = Data.Tracks[BoneIndex].InternalTrack;
			Track.PosKeys.Add(Position);
			Track.RotKeys.Add(Rotation);
			Track.ScaleKeys.Add(Scale);
		}
	}

	UAnimSequence* Sequence = CreateAnimSequence(Path, SampleRate, Data);
	if (!Sequence)
	{
		return nullptr;
	}

	int64 CurrentSourceTime = 0;
	if (SourceTime && FFbxBake::GetSourceTime(Path, CurrentSourceTime) && CurrentSourceTime == *SourceTime)
	{
		FFbxBake::WriteAnimation(BinPath, *SourceTime, StackIndex, SampleRate, Data);
	}

	HTR_LOG(Info, "Imported animation '{}' with {} bone tracks and {} keys per track in {:.1f} ms",
		AnimStack->GetName(), Data.Tracks.Num(), NumKeys, MillisecondsSince(Start));
	return Sequence;
}

UAnimSequence* FFbxImporter::LoadAnimSequence(const FString& Path, int32 StackIndex, int32 SampleRate)
{
	FFbxContext Context;
	return LoadAnimSequenceInContext(Path, StackIndex, SampleRate, Context, GetAnimationSourceTime(Path));
}

bool FFbxImporter::LoadAnimationClips(const FString& Path, TArray<FFbxAnimationClip>& OutClips,
	const std::function<UAnimSequence*(int32)>& FindLoadedClip, int32 SampleRate)
{
	OutClips.Reset();
	// Keep the imported scene open while sampling each stack that misses the cache.
	const std::optional<int64> SourceTime = GetAnimationSourceTime(Path);
	FFbxContext Context;
	const auto SceneStart = std::chrono::steady_clock::now();
	const bool bSceneLoaded = LoadScene(Path, Context, /*bTriangulateMeshes=*/false);
	HTR_LOG(Info, "[FBX ANIM] LoadScene took {:.1f} ms: {}", MillisecondsSince(SceneStart), Path);
	if (!bSceneLoaded)
	{
		return false;
	}

	const auto ClipsStart = std::chrono::steady_clock::now();
	int32 MemoryHits = 0;
	int32 FailedClips = 0;
	double FailedClipMilliseconds = 0.0;
	const int32 StackCount = Context.Scene->GetSrcObjectCount<FbxAnimStack>();
	for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
	{
		const FbxAnimStack* Stack = Context.Scene->GetSrcObject<FbxAnimStack>(StackIndex);
		if (!Stack)
		{
			continue;
		}

		const FbxTimeSpan TimeSpan = Stack->GetLocalTimeSpan();
		UAnimSequence* Sequence = FindLoadedClip ? FindLoadedClip(StackIndex) : nullptr;
		if (Sequence)
		{
			++MemoryHits;
		}
		else
		{
			const auto ClipStart = std::chrono::steady_clock::now();
			Sequence = LoadAnimSequenceInContext(Path, StackIndex, SampleRate, Context, SourceTime);
			if (!Sequence)
			{
				const double ElapsedMilliseconds = MillisecondsSince(ClipStart);
				++FailedClips;
				FailedClipMilliseconds += ElapsedMilliseconds;
				HTR_LOG(Warning, "[FBX ANIM] Clip {} '{}' failed after {:.1f} ms",
					StackIndex, Stack->GetName(), ElapsedMilliseconds);
			}
		}
		OutClips.Add({
			StackIndex,
			Stack->GetName(),
			TimeSpan.GetStop().GetSecondDouble() - TimeSpan.GetStart().GetSecondDouble(),
			Sequence
		});
	}
	HTR_LOG(Info, "[FBX ANIM] Clip processing took {:.1f} ms ({} clips, {} memory hits, {} failed in {:.1f} ms)",
		MillisecondsSince(ClipsStart), OutClips.Num(), MemoryHits, FailedClips, FailedClipMilliseconds);
	return true;
}
