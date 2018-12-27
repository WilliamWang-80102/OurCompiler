#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"




#define NUM_EXPR "NumberExpression"
#define VAR_EXPR "VariableExpression"
#define DCL_EXPR "DeclarationExpression"
#define ASG_EXPR "AssignmentExpression"
#define BIN_EXPR "BinaryExpression"
#define CAL_EXPR "CallExpression"
#define PRT_EXPR "FunctionPrototype"
#define FNC_EXPR "FunctionDefinition"
#define BLK_EXPR "BlockExpression"
#define RET_EXPR "ReturnExpression"
#define PRT_EXPR "PrintExpression"
#define CNT_EXPR "ContinueExpression"
#define IF_EXPR "IfExpression"
#define WHILE_EXPR "WhileExpression"

using namespace llvm;
//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
	//�ļ�������
	tok_eof = -1,

	// commands
	//���������
	tok_func = -2,
	//����������
	tok_extern = -3,

	// primary
	//��ʶ��
	tok_identifier = -4,
	//����
	tok_number = -5,
	//return
	tok_return = -6,
	//print

	tok_print = -7,
	//�����ﲹ��VSL�Ĺؼ���FUNC��....
	tok_continue = -8,
	tok_if = -9,
	tok_then = -10,
	tok_else = -11,
	tok_fi = -12,
	tok_while = -13,
	tok_do = -14,
	tok_done = -15,
	tok_var = -16
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

/// gettok - Return the next token from standard input.
/// ���޸�gettok���룬�ڶ������ʵĵ��ʺ�׼ȷ���������﷨���ԣ����ش��ԡ�
static int gettok() {
	static int LastChar = ' ';

	// Skip any whitespace.
	while (isspace(LastChar)) {
		LastChar = getchar();
	}


	if (isalpha(LastChar)) { //��ĸ
		IdentifierStr = LastChar;
		while (isalnum((LastChar = getchar())))//��ĸ������
			IdentifierStr += LastChar;
		//���¿���strcmp()������ʹ��
		if (IdentifierStr == "FUNC")
			return tok_func;
		if (IdentifierStr == "extern")
			return tok_extern;
		if (IdentifierStr == "PRINT")
			return tok_print;
		if (IdentifierStr == "RETURN")
			return tok_return;
		if (IdentifierStr == "CONTINUE")
			return tok_continue;
		if (IdentifierStr == "IF")
			return tok_if;
		if (IdentifierStr == "THEN")
			return tok_then;
		if (IdentifierStr == "ELSE")
			return tok_else;
		if (IdentifierStr == "FI")
			return tok_fi;
		if (IdentifierStr == "WHILE")
			return tok_while;
		if (IdentifierStr == "DO")
			return tok_do;
		if (IdentifierStr == "DONE")
			return tok_done;
		if (IdentifierStr == "VAR")
			return tok_var;
		return tok_identifier;
	}

	//�˴�Ӧ�޸ģ��������1.2.3����ͨ����������޸ĺ���ɾȥ����
	if (isdigit(LastChar) || LastChar == '.') { // ����
		std::string NumStr;
		int num_point = 0;
		do {
			if (LastChar == '.')
				num_point++;
			NumStr += LastChar;
			LastChar = getchar();
		} while (isdigit(LastChar) || LastChar == '.');
		if (num_point < 2) {
			NumVal = strtod(NumStr.c_str(), nullptr);
			return tok_number;
		}
		else {
			fprintf(stderr, "Invalid demical");
			return 0;
		}
	}

	//�˴��޸Ķ�ע�͵Ĵ���������'#'������"//"
	//��ע�͵Ĵ������޸�Ϊ"//"
	if (LastChar == '/') {
		//����ǳ��������ظó�������
		if ((LastChar = getchar()) != '/')
			return '/';

		// Comment until end of line.
		do
			LastChar = getchar();
		while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		//�˴�����/r/n�����
		if (LastChar != EOF)
			return gettok();
	}
	if (LastChar == '"') {
		LastChar = ' ';
		return '"';
	}
	// Check for end of file.  Don't eat the EOF.
	if (LastChar == EOF)
		return tok_eof;
	// Otherwise, just return the character as its ascii value.
	int ThisChar = LastChar;
	LastChar = getchar();
	return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

namespace {
	/// ExprAST - Base class for all expression nodes.
	class ExprAST {
	public:
		virtual ~ExprAST() = default;
		virtual Value *codegen() = 0;
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST {
		double Val;

	public:
		NumberExprAST(double Val) : Val(Val) {}
		Value *codegen() override;
	};

	///StringAST
	class StringAST : public ExprAST {
		std::string str;

	public:
		StringAST(std::string str) : str(str) {}
		virtual Value *Codegen();
		virtual void printAST() {
			//����ַ������
		};
		Value *codegen() override;
	};

	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST {
		std::string Name;

	public:
		VariableExprAST(const std::string &Name) : Name(Name) {}
		Value *codegen() override;
	};

	/// DeclareExprAST - Expression like 'VAR x,y,z'.
	class DeclareExprAST : public ExprAST {
		std::vector<std::string> Names;
	public:
		DeclareExprAST(const std::vector<std::string> &Names) : Names(Names) {}
		Value *codegen() override;
	};

	/// AssignExpr - ������ֵ���ʽ
	class AssignExpr : public ExprAST {
		std::string Ident;
		std::unique_ptr<ExprAST> Expr;
	public:
		AssignExpr(std::string Ident, std::unique_ptr<ExprAST> Expr)
			:Ident(Ident), Expr(std::move(Expr)) {}
		Value *codegen() override;
	};

	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST {
		char Op;
		std::unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
			std::unique_ptr<ExprAST> RHS)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		Value *codegen() override;
	};

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST {
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const std::string &Callee,
			std::vector<std::unique_ptr<ExprAST>> Args)
			: Callee(Callee), Args(std::move(Args)) {}
		Value *codegen() override;
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST {
		std::string Name;
		std::vector<std::string> Args;

	public:
		PrototypeAST(const std::string &Name, std::vector<std::string> Args)
			: Name(Name), Args(std::move(Args)) {}
		Function *codegen();
		const std::string &getName() const { return Name; }
	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST {
		std::unique_ptr<PrototypeAST> Proto;
		// std::unique_ptr<ExprAST> Body; 
		// �����Ķ��屻�޸�Ϊ��ǩ���� + �����顱����ʽ
		std::unique_ptr<ExprAST> Body;
	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			std::unique_ptr<ExprAST> Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}
		Function *codegen();
	};

	///ExprsAST - ������ʽ���
	class ExprsAST : public ExprAST {
		std::vector<std::unique_ptr<ExprAST>> Stats; //�����ʽ����
	public:
		ExprsAST(std::vector<std::unique_ptr<ExprAST>> Stats)
			:Stats(std::move(Stats)) {}
		Value *codegen() override;
	};

	///RetStatAST - ���������
	class RetStatAST : public ExprAST {
		std::unique_ptr<ExprAST> Expr; // ����������ı��ʽ
	public:
		RetStatAST(std::unique_ptr<ExprAST> Expr) : Expr(std::move(Expr)) {}
		Value *codegen() override;
	};

	/// PrtStatAST - ��ӡ�����
	/// ��ӡ������Ķ����������ʽ���ַ����� PRINT print_item1, print_item2...
	class PrtStatAST : public ExprAST {
		std::vector<std::unique_ptr<ExprAST>> Args;
	public:
		PrtStatAST(std::vector<std::unique_ptr<ExprAST>> Args) : Args(std::move(Args)) {}
		Value *codegen() override;
	};

	/// NullStatAST - �������
	class NullStatAST : public ExprAST {
	public:
		NullStatAST() {}
		Value *codegen() override;
	};

	/// IfStatAST - ���������
	class IfStatAST : public ExprAST {
		std::unique_ptr<ExprAST> Cond; // �������ʽ
		std::unique_ptr<ExprAST> ThenStat; // Cond ΪTrue���ִ������

		std::unique_ptr<ExprAST> ElseStat; // Cond ΪFalse���ִ������
	public:
		IfStatAST(std::unique_ptr<ExprAST> Cond,
			std::unique_ptr<ExprAST> ThenStat,
			std::unique_ptr<ExprAST> ElseStat)
			:Cond(std::move(Cond)),
			ThenStat(std::move(ThenStat)),
			ElseStat(std::move(ElseStat)) {}
		Value *codegen() override;
	};

	/// WhileStatAST - ��ѭ�������
	class WhileStatAST : public ExprAST {
		std::unique_ptr<ExprAST> Cond; // �������ʽ
		std::unique_ptr<ExprAST> Stat; // Cond ΪTrue���ִ������
	public:
		WhileStatAST(std::unique_ptr<ExprAST> Cond,
			std::unique_ptr<ExprAST> Stat)
			:Cond(std::move(Cond)), Stat(std::move(Stat)) {}
		Value *codegen() override;
	};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
	if (!isascii(CurTok))
		return -1;

	// Make sure it's a declared binop.
	int TokPrec = BinopPrecedence[CurTok];
	if (TokPrec <= 0)
		return -1;
	return TokPrec;
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
	LogError(Str);
	return nullptr;
}

static std::unique_ptr<ExprAST> ParseNullExpr();
static std::unique_ptr<ExprAST> ParseNumberExpr();
static std::unique_ptr<ExprAST> ParseParenExpr();
static std::unique_ptr<ExprAST> ParseIdentifierExpr();
static std::unique_ptr<ExprAST> ParseBinOpRHS(int, std::unique_ptr<ExprAST>);
static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<ExprAST> ParseReturnExpr();
static std::unique_ptr<ExprAST> ParsePrintExpr();
static std::unique_ptr<ExprAST> ParseWhileExpr();
static std::unique_ptr<ExprAST> ParseIfExpr();
static std::unique_ptr<ExprAST> ParseDclrExpr();
static std::unique_ptr<ExprAST> ParsePrimary();
static std::unique_ptr<PrototypeAST> ParsePrototype();
static std::unique_ptr<FunctionAST> ParseTopLevelExpr();
static std::unique_ptr<PrototypeAST> ParseExtern();
static std::unique_ptr<ExprsAST> ParseStats();
static std::unique_ptr<ExprAST> ParseStat();
static std::unique_ptr<ExprAST> ParseString();


///print������ַ����ڵ�
static std::unique_ptr<ExprAST> ParseString()
{
	std::string str = "";
	char c;
	while ((c = getchar()) != '"') {
		str += c;
		if (c == '\n') {
			return LogError("Expect '\"'");
		}
	}
	auto Result = llvm::make_unique<StringAST>(str);
	return std::move(Result);
}

/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
	auto Result = llvm::make_unique<NumberExprAST>(NumVal);
	getNextToken(); // consume the number
	return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
	getNextToken(); // eat (.
	auto V = ParseExpression();
	if (!V)
		return nullptr;

	if (CurTok != ')')
		return LogError("expected ')'");
	getNextToken(); // eat ).
	return V;
}

/// NullExpr ::= CONTINUE;
static std::unique_ptr<ExprAST> ParseNullExpr() {
	getNextToken();
	return llvm::make_unique<NullStatAST>();
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
///   ::= identifier ':= ' expression
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;

	getNextToken(); // eat identifier.
	//�����־����Ϊ��ֵ����
	if (CurTok == ':')
	{
		getNextToken();
		if (CurTok == '=')
		{
			//�˵�'='
			getNextToken();
			std::unique_ptr<ExprAST> RHS = ParseExpression();
			if (RHS) {
				fprintf(stderr, "Parsed an assignment statement\n");
				return llvm::make_unique<AssignExpr>(IdName, std::move(RHS));
			}
		}
		else
			return LogError("Expect ':=', error at ParseIdentifierExpr");// û��'='���쳣����
	}
	if (CurTok != '(') // Simple variable ref.
		return llvm::make_unique<VariableExprAST>(IdName);
	// Call.
	getNextToken(); // eat (
	std::vector<std::unique_ptr<ExprAST>> Args;
	//��if�������Ż�
	if (CurTok != ')') {
		while (true) {
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
			else
				return nullptr;

			if (CurTok == ')')
				break;

			if (CurTok != ',')
				return LogError("Expect ')' or ',' in argument list");
			getNextToken();
		}
	}

	// Eat the ')'.
	getNextToken();
	
	return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}

//ParseReturnExpr - ʵ�ַ������
static std::unique_ptr<ExprAST> ParseReturnExpr()
{
	getNextToken();//eat RETURN
	std::unique_ptr<ExprAST> Expr = ParseExpression();
	if (CurTok == ';') {
		if (Expr) return llvm::make_unique<RetStatAST>(std::move(Expr));
		else return LogError("Expect return value!");
	}
	else return LogError("Expect ';'");
}

//ParsePrintExpr - ʵ�ִ�ӡ���
static std::unique_ptr<ExprAST> ParsePrintExpr()
{
	std::vector <std::unique_ptr<ExprAST>> Args;
	while (CurTok != ';')
	{
		getNextToken();
		if (CurTok == '"') {
			//getPrintString()�������ڻ�ȡ˫����֮�������
			Args.push_back(ParseString());
			getNextToken();
		}
		if (CurTok == tok_identifier) {
			auto E = ParseExpression();
			if (E) {
				Args.push_back(std::move(E));
			}
			/*else {
				return LogError("Unexpected token");
			}*/
		}
		if (CurTok == ';')break;
		if (CurTok != ',') {
			getNextToken();
			return LogError("Unexpected token");
		}
	}
	auto Result = llvm::make_unique<PrtStatAST>(std::move(Args));
	return Result;
}

//����IF��WHILE������������
static std::unique_ptr<ExprAST> ParseConditionExpr() {
	std::unique_ptr<ExprAST> Cond = ParseExpression();
	if (Cond) {
		return std::move(Cond);
	}
	else return LogError("Expect condition statement!");
}
//ParseWhileExpr - ʵ��Whileѭ��
static std::unique_ptr<ExprAST> ParseWhileExpr() {
	getNextToken();//eat WHILE
	std::unique_ptr<ExprAST> Cond, Stat;
	std::unique_ptr<WhileStatAST> WhilePtr;
	Cond = ParseConditionExpr();
	if (CurTok == tok_do) {
		getNextToken();//eat DO
		Stat = ParseStats();
		if (!Stat) return LogError("Expect 'DO' statements!");
		//return LogError("Not A WHILE Parser!");
	}
	else return LogError("Expect 'DO'!");
	if (CurTok == tok_done) {
		getNextToken();//eat DONE
		WhilePtr = llvm::make_unique<WhileStatAST>(std::move(Cond), std::move(Stat));
		return WhilePtr;
	}
	else return LogError("Expect 'DONE'!");//����done���԰�ȫ�˳�
}
//ParseIfExpr - ʵ��If�ж�
static std::unique_ptr<ExprAST> ParseIfExpr() {
	getNextToken();//eat IF
	std::unique_ptr<ExprAST> Cond, ThenStat, ElseStat;
	std::unique_ptr<IfStatAST> IfPtr;
	Cond = ParseConditionExpr(); //����if���������
	if (CurTok == tok_then) {
		getNextToken();//eat THEN
		ThenStat = ParseStats();
		if (!ThenStat) return LogError("Expect 'THEN' statements!");
	}
	else return LogError("Expect 'THEN'!");
	if (CurTok == tok_fi) {
		getNextToken();//eat FI
		IfPtr = llvm::make_unique<IfStatAST>(std::move(Cond), std::move(ThenStat), std::move(ElseStat));
		return IfPtr;
	}
	else if (CurTok == tok_else) {
		getNextToken();//eat ELSE
		ElseStat = ParseStats();
		if (ElseStat) {
			if (CurTok == tok_fi) {
				getNextToken();//eat FI
				IfPtr = llvm::make_unique<IfStatAST>(std::move(Cond), std::move(ThenStat), std::move(ElseStat));
				return IfPtr;
			}
			else return LogError("Expect 'FI'!");
		}
		else return LogError("Expect 'ELSE' statements!");
	}
	return nullptr;
}

//ParseDclrExpr - ʵ�ֱ�������������
static std::unique_ptr<ExprAST> ParseDclrExpr() {
	getNextToken();
	std::vector<std::string> Names;
	while (CurTok == tok_identifier) {
		Names.push_back(IdentifierStr);
		getNextToken();
		if (CurTok == ',') {
			getNextToken();//eat ','
			if (CurTok == ';') {//��ֹ���� VAR X,;֮������
				LogError("Expect var names");
				getNextToken();//eat ';'
				return nullptr;
			}
		}
	}
	getNextToken();//��ȥ';'
	if (Names.empty()) {//��ֹ���� VAR ;�Ŀձ����������
		return LogError("Expect var names");
	}
	else {
		return llvm::make_unique<DeclareExprAST>(std::move(Names));
	}
}

/// primary ��һ�����ʽ�еĻ�����Ԫ������identifierexpr�������� �������ã� ��ֵ���ʽ��, numberexpr, parenexpr
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
	switch (CurTok) {
	default:
		return LogError("unknown token when expecting an expression");
	case tok_identifier:
		return ParseIdentifierExpr();
	case tok_number:
		return ParseNumberExpr();
	case '(':
		return ParseParenExpr();
	}
}

/// binoprhs
///   ::= ('+' primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
	std::unique_ptr<ExprAST> LHS) {
	// If this is a binop, find its precedence.
	while (true) {
		int TokPrec = GetTokPrecedence();

		// If this is a binop that binds at least as tightly as the current binop,
		// consume it, otherwise we are done.
		if (TokPrec < ExprPrec)
			return LHS;

		// Okay, we know this is a binop.
		int BinOp = CurTok;
		getNextToken(); // eat binop

		// Parse the primary expression after the binary operator.
		auto RHS = ParsePrimary();
		if (!RHS)
			return LogError("ParseBinOpRHS error! Expect rhs but find null!");

		// If BinOp binds less tightly with RHS than the operator after RHS, let
		// the pending operator take RHS as its LHS.
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
			if (!RHS)
				return LogError("ParseBinOpRHS error! Expect rhs but find null!");
		}

		// Merge LHS/RHS.
		LHS = llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS),
			std::move(RHS));
	}
}

/// expression
///   ::= primary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression() {
	//���Ƚ�LHS��Ϊ���ֳ�����㣬ֵΪ0
	std::unique_ptr<ExprAST> LHS = llvm::make_unique<NumberExprAST>(0);
	//���������'-'��ͷ������������Ӧ���������Ķ�����ʽ
	if (CurTok != '-') {
		LHS = ParsePrimary();
		if (!LHS)
			return LogError("ParseExpression error! Expect LHS but find null!");
	}
	//�����'-'��ͷ����ô����0�������ΪLHS�����ж�����ʽ����
	return ParseBinOpRHS(0, std::move(LHS));
}

/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
	if (CurTok != tok_identifier)
		return LogErrorP("Expect function name in prototype");

	std::string FnName = IdentifierStr;
	getNextToken();

	if (CurTok != '(')
		return LogErrorP("Expect '(' in prototype");
	//VSL���Բ�����','������˴��޸�
	//���޸� ��
	std::vector<std::string> ArgNames;
	getNextToken();//eat '('
	while (CurTok == tok_identifier) {
		ArgNames.push_back(IdentifierStr);
		if (getNextToken() == ',') getNextToken();//eat ','
	}
	if (CurTok != ')')
		return LogErrorP("Expect ')' in prototype");

	// success.
	getNextToken(); // eat ')'.

	return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'FUNC' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
	getNextToken(); // eat FUNC.
	auto Proto = ParsePrototype();
	if (!Proto)
		return nullptr;

	//�˴���Ϊ��������������
	if (auto E = ParseStats())
		return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
	return nullptr;
}

/// toplevelexpr ::= expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
	if (auto E = ParseExpression()) {
		// Make an anonymous proto.
		auto Proto = llvm::make_unique<PrototypeAST>("__anon_expr",
			std::vector<std::string>());
		return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
	}
	return nullptr;
}

/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
	getNextToken(); // eat extern.
	return ParsePrototype();
}

/// ParseStats - ��������ĺ���
static std::unique_ptr<ExprsAST> ParseStats() {
	//getNextToken();
	std::vector<std::unique_ptr<ExprAST>> Stats;
	if (CurTok != '{') {
		Stats.push_back(std::move(ParseStat()));
		return llvm::make_unique<ExprsAST>(std::move(Stats));
	}
	else {
		//�����������﷨������������쳣����
		getNextToken();//�˵�'{'
		while (CurTok != '}') {
			Stats.push_back(std::move(ParseStat()));
			getNextToken();
		}
		//���ĵ�'}'
		getNextToken();
		return llvm::make_unique<ExprsAST>(std::move(Stats));
	}
}

/// ParseStat - �����������ĺ���
static std::unique_ptr<ExprAST> ParseStat() {
	switch (CurTok)
	{
		//VAR
	case tok_var:
		return ParseDclrExpr();

		//IF
	case tok_if:
		return ParseIfExpr();

		//WHILE
	case tok_while:
		return ParseWhileExpr();

		//IDENTI
	case tok_identifier:
		return ParseExpression();

	case tok_number:
		return ParseExpression();

		//RETURN
	case tok_return:
		return ParseReturnExpr();

		//PRINT
	case tok_print:
		return ParsePrintExpr();

	default:
		//Υ���﷨������
		LogError("Unknown Statement!");
		getNextToken();
		return nullptr;
	}
}



//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//



static void HandleContinue() {
	if (ParseNullExpr()) {
		fprintf(stderr, "Parsed a null expression.\n");
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleDeclaration() {
	if (ParseDclrExpr()) {
		fprintf(stderr, "Parsed a declaration statement.\n");
		//getNextToken();
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleDefinition() {
	if (auto FnAST = ParseDefinition()) {
		if (auto *FnIR = FnAST->codegen()) {
			fprintf(stderr, "Read function definition:");
			FnIR->print(errs());
			fprintf(stderr, "\n");
		}
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleExtern() {
	if (ParseExtern()) {
		fprintf(stderr, "Parsed an extern\n");
		getNextToken();
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleTopLevelExpression() {
	// Evaluate a top-level expression into an anonymous function.
	if (auto FnAST = ParseTopLevelExpr()) {
		if (auto *FnIR = FnAST->codegen()) {
			fprintf(stderr, "Read top-level expression:");
			FnIR->print(errs());
			fprintf(stderr, "\n");
		}
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleIf() {
	if (ParseIfExpr()) {
		fprintf(stderr, "Parsed a if statement\n");
	}
	else {
		//Skip token for error recovery.
		getNextToken();
	}
}

static void HandleWhile() {
	if (ParseWhileExpr()) {
		fprintf(stderr, "Parse a while statement\n");
	}
	else {
		//Skip token for error recovery.
		getNextToken();
	}
}

static void HandlePrint() {
	if (ParsePrintExpr()) {
		fprintf(stderr, "Parse a print statement\n");
		getNextToken();
	}
	else {
		//Skip token for error recovery.
		getNextToken();
	}
}

static void HandleReturn() {
	if (ParseReturnExpr()) {
		fprintf(stderr, "Parse a return statement\n");
		getNextToken();
	}
	else {
		//Skip token for error recovery.
		getNextToken();
	}
}

static void HandleIdentifierExpr() {
	// Evaluate a identifier expression into an anonymous function.
	if (auto E = ParseIdentifierExpr()) {
		// Make an anonymous proto.
		auto Proto = llvm::make_unique<PrototypeAST>("__anon_expr",
			std::vector<std::string>());
		auto FnAST = llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
		if (auto *FnIR = FnAST->codegen()) {
			fprintf(stderr, "Read identifier expression:");
			FnIR->print(errs());
			fprintf(stderr, "\n");
		}
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

/// top ::= definition | external | expression | ';'

static void MainLoop() {
	while (true) {
		switch (CurTok) {
		case tok_func:
			HandleDefinition();
			break;
		case tok_identifier:
			//ParseIdentifierExpr();
			HandleIdentifierExpr();
			break;
		case tok_return:
			HandleReturn();
			break;
		case tok_print:
			HandlePrint();
			break;
		case tok_if:
			HandleIf();
			break;
		case tok_while:
			HandleWhile();
			break;
		case tok_var:
			HandleDeclaration();
			break;
		case tok_eof:
			return;
		case '#':
			fprintf(stderr, "ready> ");
			getNextToken();
			break;
			//�Ǻ����屨��
		default:
			LogError("Error!");
			fprintf(stderr, "ready> ");
			//getNextToken();
			/*
			//����������������е����ݣ��ڴ��û���������
			while (getNextToken() != '\n') {}
			*/
			//�ڴ���һ�����
			getNextToken();
			break;
		}
	}
}
//===----------------------------------------------------------------------===//
// Program Parse Code
//===----------------------------------------------------------------------===//
/*
static std::unique_ptr<FunctionAST> ParseProgram() {

	//�洢�����е����к������������
	std::vector<std::unique_ptr<FunctionAST>> func_list;
	while (true) {
		switch (CurTok)
		{
		case tok_func:
		{
			auto E = ParseDefinition();
			func_list.push_back(std::move(E));
		}
		case '#'://�������������س����﷨��
			return llvm::make_unique<FunctionAST>(std::move(func_list));
		}
	}
}
*/

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;
Value *LogErrorV(const char *Str) {
	LogError(Str);
	return nullptr;
}

//��������
Value *NumberExprAST::codegen() {
	return ConstantFP::get(TheContext, APFloat(Val));
}

//���������м���봦��
Value *VariableExprAST::codegen() {
	// Look this variable up in the function.
	Value *V = NamedValues[Name];
	if (!V)
		LogErrorV("Unknown variable name");
	return V;
}

//��Ԫ���ʽ�м��������
Value *BinaryExprAST::codegen() {
	Value *L = LHS->codegen();
	Value *R = RHS->codegen();
	if (!L || !R)
		return LogErrorV("invalid LHS or RHS!");

	switch (Op) {
	case '+':
		return Builder.CreateFAdd(L, R, "addtmp");
	case '-':
		return Builder.CreateFSub(L, R, "subtmp");
	case '*':
		return Builder.CreateFMul(L, R, "multmp");
	case '/':
		return Builder.CreateFDiv(L, R, "divtmp");
	default:
		return LogErrorV("invalid binary operator!");
	}
}

Value *CallExprAST::codegen() {
	// Look up the name in the global module table.
	Function *CalleeF = TheModule->getFunction(Callee);
	if (!CalleeF)
		return LogErrorV("Unknown function referenced");

	// If argument mismatch error.
	if (CalleeF->arg_size() != Args.size())
		return LogErrorV("Incorrect # arguments passed");

	std::vector<Value *> ArgsV;
	for (unsigned i = 0, e = Args.size(); i != e; ++i) {
		ArgsV.push_back(Args[i]->codegen());
		if (!ArgsV.back())
			return nullptr;
	}

	return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Value *StringAST::codegen() {
	return nullptr;
}

Value *NullStatAST::codegen() {
	return nullptr;
}

Value *AssignExpr::codegen() {
	return nullptr;
}

Value *WhileStatAST::codegen() {
	return nullptr;
}

Value *IfStatAST::codegen() {
	return nullptr;
}

Value *DeclareExprAST::codegen() {
	return nullptr;
}

Value *ExprsAST::codegen() {
	/*
	//�����������ȫ��ת��Ϊir
	for (std::vector<std::unique_ptr<ExprAST>>::const_iterator iter = Stats.cbegin(); iter != Stats.cend(); iter++) {
		(*iter)->codegen();
	}
	//�������������������ָ��
	return nullptr;
	*/
	//���Ը����ɴ���׶Σ�����ֻ��һ��
	return Stats[0]->codegen();
}

Function *PrototypeAST::codegen() {
	//Make the function type:  double(double,double) etc.
	std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(TheContext));
	FunctionType *FT =
		FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

	Function *F =
		Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto &Arg : F->args())
		Arg.setName(Args[Idx++]);
	return F;
}

Function *FunctionAST::codegen() {
	// First, check for an existing function from a previous 'extern' declaration.
	Function *TheFunction = TheModule->getFunction(Proto->getName());

	if (!TheFunction)
		TheFunction = Proto->codegen();

	if (!TheFunction) {
		fprintf(stderr, "Function Proto codegen error!");
		return nullptr;
	}
		

	// Create a new basic block to start insertion into.
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	Builder.SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto &Arg : TheFunction->args())
		NamedValues[Arg.getName()] = &Arg;

	
	if (Value *RetVal = Body->codegen()) {
		// Finish off the function.
		Builder.CreateRet(RetVal);

		// Validate the generated code, checking for consistency.
		verifyFunction(*TheFunction);

		return TheFunction;
	}

	// Error reading body, remove function.
	TheFunction->eraseFromParent();
	return TheFunction;
}

//string�Ĵ�������
Value *StringAST::Codegen()
{
	Value *V = NamedValues[str];
	return V;
}

//��������������codegen()
Value *RetStatAST::codegen()
{
	Value *RetValue = Expr->codegen();
	/*
	return Builder.CreateRet(RetValue);
	*/
	return RetValue;
}

//��ӡ����������codegen()
Value *PrtStatAST::codegen()
{

	/*std::vector<Value *> ArgsP;
	for (unsigned i = 0, e = Args.size(); i != e; ++i) {
	ArgsP.push_back(Args[i]->codegen());
	}
	return Builder.CreatePrt(ArgsP);*/

	return nullptr;
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
	// Install standard binary operators.
	// 1 is lowest precedence.
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40;
	BinopPrecedence['/'] = 40;// highest.
	//������������������ȼ�

	// Prime the first token.
	fprintf(stderr, "ready> ");
	getNextToken();

	// Make the module, which holds all the code.
	TheModule = llvm::make_unique<Module>("my cool jit", TheContext);

	// Run the main "interpreter loop" now.
	MainLoop();

	// Print out all of the generated code.
	TheModule->print(errs(), nullptr);

	system("pause");

	return 0;
}

