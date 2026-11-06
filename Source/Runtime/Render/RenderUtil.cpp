#include "EnginePCH.h"
#include "RenderUtil.h"

#include <algorithm>

namespace
{
	const char* GetShaderTarget(EShaderType Type)
	{
		switch (Type)
		{
		case EShaderType::Vertex:
			return "vs_5_0";
			break;
		case EShaderType::Pixel:
			return "ps_5_0";
			break;
		case EShaderType::Compute:
			return "cs_5_0";
			break;
		default:
			return "";
			break;
		}
	}

	const char* GetShaderPrefix(EShaderType Type)
	{
		switch (Type)
		{
		case EShaderType::Vertex:
			return "_VS.cso";
			break;
		case EShaderType::Pixel:
			return  "_PS.cso";
			break;
		case EShaderType::Compute:
			return  "_CS.cso";
			break;
		default:
			return "";
			break;
		}
	}
}

FShaderByteCode RenderUtil::CompileShader(FString Path, const char* EntryPoint, EShaderType ShaderType, const D3D_SHADER_MACRO* Macros)
{
	const char* Target = GetShaderTarget(ShaderType);

	uint32 Flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	FShaderByteCode ByteCode;

	std::wstring WPath(Path.begin(), Path.end());

	//for (const auto& D : Defines)
	//	Macros.push_back({ D.first.c_str(), D.second.c_str() });
	//Macros.push_back({ nullptr, nullptr });		// 끝 표시

	ComPtr<ID3DBlob> ErrorBlob;
	HRESULT hr = D3DCompileFromFile(WPath.c_str(), Macros, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint, Target, Flags, 0, ByteCode.Blob.GetAddressOf(), ErrorBlob.GetAddressOf());

	if (FAILED(hr))
	{
		if (ErrorBlob)
			OutputDebugStringA((const char*)ErrorBlob->GetBufferPointer());
		else
			OutputDebugStringA(("[Shader] file not found: " + Path + "\n").c_str());

		ByteCode.Blob.Reset();
	}
	return ByteCode;
}

void RenderUtil::SaveByteCode(FString CsoPath, const FShaderByteCode& ByteCode)
{
	fs::create_directories(fs::path(CsoPath).parent_path());
	std::ofstream Out(CsoPath, std::ios::binary);
	if (!Out) { fs::remove(CsoPath); return; }
	Out.write(reinterpret_cast<const char*>(ByteCode.GetData()), ByteCode.GetSize());
}

FShaderByteCode RenderUtil::LoadByteCode(const FString& CsoPath)
{
	FShaderByteCode Result;

	std::ifstream In(CsoPath, std::ios::binary | std::ios::ate);
	if (!In) return Result;

	const std::streamsize Size = In.tellg();
	In.seekg(0);
	if (Size <= 0) return Result;

	ComPtr<ID3DBlob> Blob;
	if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(Size), &Blob))) return Result;

	In.read(reinterpret_cast<char*>(Blob->GetBufferPointer()), Size);
	if (In) Result.Blob = std::move(Blob);
	return Result;
}

FString RenderUtil::GetCsoPath(const FString& ShaderPath, EShaderType ShaderType, const D3D_SHADER_MACRO* Macros)
{
	const fs::path Rel = fs::relative(ShaderPath, "Resources/Shader");
	const char* Suffix = GetShaderPrefix(ShaderType);
	FString MacroSuffix;
	while (Macros && Macros->Name)
	{
		MacroSuffix += FString("_") + Macros->Name;
		Macros++;
	}

	// Debug는 최적화를 끄고(D3DCOMPILE_SKIP_OPTIMIZATION) 컴파일하므로 구성마다 캐시를 따로 둔다.
#ifdef _DEBUG
	fs::path Out = fs::path("Intermediate/Shaders/Debug") / Rel.parent_path();
#else
	fs::path Out = fs::path("Intermediate/Shaders/Release") / Rel.parent_path();
#endif
	Out /= Rel.stem().generic_string() + MacroSuffix.c_str() + Suffix;
	return Out.generic_string();
}

namespace
{
	// 셰이더 파일과 그 파일이 #include "..." 하는 파일들 중 가장 최근 수정 시각.
	// include는 D3D_COMPILE_STANDARD_FILE_INCLUDE와 같이 포함하는 파일의 폴더 기준으로 찾는다.
	bool GetLatestSourceTime(const fs::path& Path, std::vector<fs::path>& Visited, fs::file_time_type& InOutLatest)
	{
		std::error_code Ec;
		const fs::path Normalized = fs::weakly_canonical(Path, Ec);
		if (Ec) return false;
		if (std::find(Visited.begin(), Visited.end(), Normalized) != Visited.end()) return true;
		Visited.push_back(Normalized);

		const auto Time = fs::last_write_time(Normalized, Ec);
		if (Ec) return false;
		if (Time > InOutLatest) InOutLatest = Time;

		std::ifstream File{ Normalized };
		if (!File) return false;
		std::string Line;
		while (std::getline(File, Line))
		{
			const size_t Directive = Line.find("#include");
			if (Directive == std::string::npos) continue;
			// 주석 처리된 include는 건너뛴다.
			const size_t Comment = Line.find("//");
			if (Comment != std::string::npos && Comment < Directive) continue;
			const size_t Open = Line.find('"', Directive);
			const size_t Close = Open == std::string::npos ? std::string::npos : Line.find('"', Open + 1);
			if (Close == std::string::npos) continue;
			if (!GetLatestSourceTime(Normalized.parent_path() / Line.substr(Open + 1, Close - Open - 1), Visited, InOutLatest))
				return false;
		}
		return true;
	}
}

// .cso가 셰이더 파일과 그 include 파일들보다 나중에 만들어졌으면 최신이다.
// include가 있는 셰이더(UberLit 등)도 캐시를 쓰도록, include 파일의 수정 시각까지 본다.
bool RenderUtil::IsCsoUpToDate(const FString& ShaderPath, const FString& CsoPath)
{
	std::error_code Ec;
	if (!fs::exists(CsoPath, Ec)) return false;
	const auto CsoTime = fs::last_write_time(CsoPath, Ec);
	if (Ec) return false;

	std::vector<fs::path> Visited;
	fs::file_time_type LatestSource = fs::file_time_type::min();
	if (!GetLatestSourceTime(fs::path(ShaderPath), Visited, LatestSource)) return false;

	return CsoTime > LatestSource;
}

FShaderByteCode RenderUtil::GetOrCompile(const FString& ShaderPath, const char* EntryPoint, EShaderType ShaderType, FString& OutCSOPath, const D3D_SHADER_MACRO* Macros)
{
	OutCSOPath = GetCsoPath(ShaderPath, ShaderType, Macros);

	if (IsCsoUpToDate(ShaderPath, OutCSOPath))
	{
		FShaderByteCode Cached = LoadByteCode(OutCSOPath);
		if (Cached.IsValid()) return Cached;
	}

	FShaderByteCode Compiled = CompileShader(ShaderPath, EntryPoint, ShaderType, Macros);
	if (Compiled.IsValid())
		SaveByteCode(OutCSOPath, Compiled);

	return Compiled;

}
