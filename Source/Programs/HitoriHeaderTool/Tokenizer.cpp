#include "Tokenizer.h"

#include <cctype>

namespace
{
	bool IsIdentStart(char C) { return std::isalpha((unsigned char)C) || C == '_'; }
	bool IsIdentChar(char C) { return std::isalnum((unsigned char)C) || C == '_'; }

	class FTokenizer
	{
	public:
		FTokenizer(const std::string& InSource, const fs::path& InFile, std::vector<FToken>& InTokens, FDiagnostics& InDiag)
			:Src(InSource), File(InFile), Tokens(InTokens), Diag(InDiag)
		{

		}

		bool Run()
		{
			if (Src.compare(0, 3, "\xEF\xBB\xBF") == 0)
			{
				Pos = 3;
			}

			while (Pos < Src.size())
			{
				const char C = Src[Pos];

				if (C == '\n')
				{
					++Line;
					++Pos;
					bLineStart = true;
				}
				else if (std::isspace((unsigned char)C))
				{
					++Pos;
				}
				else if (C == '/' && Peek(1) == '/')
				{
					SkipLineComment();
				}
				else if (C == '/' && Peek(1) == '*')
				{
					if (!SkipBlockComment())
					{
						return false;
					}
				}
				else if (C == '#' && bLineStart)
				{
					if (!SkipDirective())
					{
						return false;
					}
				}
				else
				{
					bLineStart = false;
					if (!ReadToken())
					{
						return false;
					}
				}
			}
			return true;
		}


	private:
		char Peek(size_t Offset) const { return Pos + Offset < Src.size() ? Src[Pos + Offset] : '\0'; }

		void Add(ETokenType Type, size_t Begin, int TokenLine)
		{
			Tokens.push_back({ Type, Src.substr(Begin, Pos - Begin), TokenLine });
		}

		void SkipLineComment()
		{
			while (Pos < Src.size() && Src[Pos] != '\n')
			{
				++Pos;
			}
		}

		bool SkipBlockComment()
		{
			const int StartLine = Line;
			Pos += 2;
			while (Pos < Src.size() && !(Src[Pos] == '*' && Peek(1) == '/'))
			{
				if (Src[Pos] == '\n')
				{
					++Line;
				}
				++Pos;
			}
			if (Pos >= Src.size())
			{
				Diag.Error(File, StartLine, "/* 주석이 닫히지 않았어요.");
				return false;
			}
			Pos += 2;
			return true;
		}

		bool SkipDirective()
		{
			while (Pos < Src.size() && Src[Pos] != '\n')
			{
				if (Src[Pos] == '\\' && (Peek(1) == '\n' || (Peek(1) == '\r' && Peek(2) == '\n')))
				{
					Pos += Peek(1) == '\r' ? 3 : 2;
					++Line;
				}
				else if (Src[Pos] == '/' && Peek(1) == '/')
				{
					SkipLineComment();
				}
				else if (Src[Pos] == '/' && Peek(1) == '*')
				{
					if (!SkipBlockComment())
					{
						return false;
					}
				}
				else
				{
					++Pos;
				}
			}
			return true;
		}

		bool ReadQuoted(char Quote, size_t Begin, int TokenLine)
		{
			++Pos;
			while (Pos < Src.size() && Src[Pos] != Quote)
			{
				if (Src[Pos] == '\n')
				{
					break;
				}
				if (Src[Pos] == '\\' && Peek(1)=='\n')
				{
					++Line;
				}
				Pos += Src[Pos] == '\\' ? 2 : 1;
			}
			if (Pos >= Src.size() || Src[Pos] != Quote)
			{
				Diag.Error(File, TokenLine, "Quote is not closed");
				return false;
			}
			++Pos;
			Add(Quote == '"' ? ETokenType::String : ETokenType::Char, Begin, TokenLine);
			return true;
		}

		// R"delim( ... )delim"
		bool ReadRawString(size_t Begin, int TokenLine)
		{
			++Pos; 
			const size_t DelimBegin = Pos;
			while (Pos < Src.size() && Src[Pos] != '(')
			{
				++Pos;
			}
			const std::string Close = ")" + Src.substr(DelimBegin, Pos - DelimBegin) + "\"";
			const size_t End = Src.find(Close, Pos);
			if (End == std::string::npos)
			{
				Diag.Error(File, TokenLine, "Raw string is not closed");
				return false;
			}
			for (size_t I = Pos; I < End; ++I)
			{
				if (Src[I] == '\n')
				{
					++Line;
				}
			}
			Pos = End + Close.size();
			Add(ETokenType::String, Begin, TokenLine);
			return true;
		}

		bool ReadToken()
		{
			const size_t Begin = Pos;
			const int TokenLine = Line;
			const char C = Src[Pos];

			if (IsIdentStart(C))
			{
				while (Pos < Src.size() && IsIdentChar(Src[Pos]))
				{
					++Pos;
				}

				const std::string Ident = Src.substr(Begin, Pos - Begin);
				const char Next = Peek(0);
				if (Next == '"' || Next == '\'')
				{
					if (Ident == "R" || Ident == "LR" || Ident == "uR" || Ident == "UR" || Ident == "u8R")
					{
						return ReadRawString(Begin, TokenLine);
					}
					if (Ident == "L" || Ident == "u" || Ident == "U" || Ident == "u8")
					{
						return ReadQuoted(Next, Begin, TokenLine);
					}
				}
				Add(ETokenType::Identifier, Begin, TokenLine);
				return true;
			}
			if (std::isdigit((unsigned char)C) || (C == '.' && std::isdigit((unsigned char)Peek(1))))
			{
				while (Pos < Src.size())
				{
					const char D = Src[Pos];
					if (IsIdentChar(D) || D == '.' || D == '\'')
					{
						++Pos;
					}
					else if ((D == '+' || D == '-') && (Src[Pos - 1] == 'e' || Src[Pos - 1] == 'E' || Src[Pos - 1] == 'p' || Src[Pos - 1] == 'P'))
					{
						++Pos;
					}
					else
					{
						break;
					}
				}
				Add(ETokenType::Number, Begin, TokenLine);
				return true;
			}

			if (C == '"' || C == '\'')
			{
				return ReadQuoted(C, Begin, TokenLine);
			}

			if ((C == ':' && Peek(1) == ':') || (C == '-' && Peek(1) == '>'))
			{
				Pos += 2;
			}
			else
			{
				++Pos;
			}
			Add(ETokenType::Symbol, Begin, TokenLine);
			return true;
		}

		const std::string& Src;
		const fs::path& File;
		std::vector<FToken>& Tokens;
		FDiagnostics& Diag;

		size_t Pos = 0;
		int Line = 1;
		bool bLineStart = true;
	};
}

bool Tokenize(const std::string& Source, const fs::path& File, std::vector<FToken>& OutTokens, FDiagnostics& Diag)
{
	OutTokens.clear();
	return FTokenizer(Source, File, OutTokens, Diag).Run();
}
