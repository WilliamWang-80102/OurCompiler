#include "llvm/ADT/STLExtras.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
	while (isspace(LastChar))
		LastChar = getchar();

	if (isalpha(LastChar)) { //��ĸ
		IdentifierStr = LastChar;
		while (isalnum((LastChar = getchar())))//��ĸ������
			IdentifierStr += LastChar;

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
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while (isdigit(LastChar) || LastChar == '.');

		NumVal = strtod(NumStr.c_str(), nullptr);
		return tok_number;
	}

	//�˴��޸Ķ�ע�͵Ĵ���������'#'������"//"
	if (LastChar == '#') {
		// Comment until end of line.
		do
			LastChar = getchar();
		while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		if (LastChar != EOF)
			return gettok();
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
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST {
		double Val;

	public:
		NumberExprAST(double Val) : Val(Val) {}
	};

	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST {
		std::string Name;

	public:
		VariableExprAST(const std::string &Name) : Name(Name) {}
	};

	/// AssignExpr - ������ֵ���ʽ
	class AssignExpr : public ExprAST {
		std::string Ident;
		std::unique_ptr<ExprAST> Expr;
	public:
		AssignExpr(std::string Ident, std::unique_ptr<ExprAST> Expr)
		:Ident(Ident),Expr(std::move(Expr)){}
	};

	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST {
		char Op;
		std::unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
			std::unique_ptr<ExprAST> RHS)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
	};

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST {
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const std::string &Callee,
			std::vector<std::unique_ptr<ExprAST>> Args)
			: Callee(Callee), Args(std::move(Args)) {}
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

		const std::string &getName() const { return Name; }
	};

	class ExprsAST;

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST : public ExprAST {
		std::unique_ptr<PrototypeAST> Proto;
		// std::unique_ptr<ExprAST> Body; 
		// �����Ķ��屻�޸�Ϊ��ǩ���� + �����顱����ʽ
		std::unique_ptr<ExprsAST> Body;
	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			std::unique_ptr<ExprsAST> Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}
	};

	///ExprsAST - ������ʽ���
	class ExprsAST : public ExprAST {
		std::vector<std::unique_ptr<ExprAST>> Stats; //�����ʽ����
	public:
		ExprsAST(std::vector<std::unique_ptr<ExprAST>> Stats)
			:Stats(std::move(Stats)) {}
	};

	///RetStatAST - ���������
	class RetStatAST : public ExprAST {
		std::unique_ptr<ExprAST> Expr; // ����������ı��ʽ
	public:
		RetStatAST(std::unique_ptr<ExprAST> Expr) : Expr(std::move(Expr)) {}
	};

	/// PrtStatAST - ��ӡ�����
	/// ��ӡ������Ķ����������ʽ���ַ����� PRINT print_item1, print_item2...
	class PrtStatAST : public ExprAST {
		std::vector<std::unique_ptr<ExprAST>> Args;
	public:
		PrtStatAST(std::vector<std::unique_ptr<ExprAST>> Args) : Args(std::move(Args)) {}
	};

	/// NullStatAST - �������
	class NullStatAST : public ExprAST {
	public:
		NullStatAST() {}
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
	};

	/// WhileStatAST - ��ѭ�������
	class WhileStatAST : public ExprAST {
		std::unique_ptr<ExprAST> Cond; // �������ʽ
		std::unique_ptr<ExprAST> Stat; // Cond ΪTrue���ִ������
	public:
		WhileStatAST(std::unique_ptr<ExprAST> Cond,
			std::unique_ptr<ExprAST> Stat)
			:Cond(std::move(Cond)), Stat(std::move(Stat)) {}
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

	if (CurTok != '(') // Simple variable ref.
		return llvm::make_unique<VariableExprAST>(IdName);

	//�����־����Ϊ��ֵ����
	if (CurTok == ':')
	{
		getNextToken();
		//δ�����쳣�����
		if (CurTok == '=')
		{
			//�˵�'='
			getNextToken();
			std::unique_ptr<ExprAST> RHS = ParseExpression();
			if (!RHS) {
				return llvm::make_unique<AssignExpr>(IdName, RHS);
			}				
		}
	}
	// Call.
	getNextToken(); // eat (
	std::vector<std::unique_ptr<ExprAST>> Args;
	if (CurTok != ')') {
		while (true) {
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
			else
				return nullptr;

			if (CurTok == ')')
				break;

			if (CurTok != ',')
				return LogError("Expected ')' or ',' in argument list");
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
	if (CurTok == tok_return)
	{
		std::unique_ptr<ExprAST> Expr = ParseExpression();
		if (!Expr) {
			//auto Result = new RetStatAST(std::move(Expr));
			auto Result = llvm::make_unique<RetStatAST>(Expr);
			return Result;
		}
	}
}

//ParsePrintExpr - ʵ�ִ�ӡ���
static std::unique_ptr<ExprAST> ParsePrintExpr()
{
	std::vector <std::unique_ptr<ExprAST>> Args;
	if (CurTok == tok_print)
	{
		getNextToken();
		if (CurTok == '"')
		while (getNextToken() != '"')
		{
			//getPrintString()�������ڻ�ȡ˫����֮�������
			Args.push_back(ParseExpression());
		}
		if (CurTok == tok_number)
			Args.push_back(ParseNumberExpr());
		if(CurTok==tok_identifier)
			Args.push_back(ParsePrimary());
	}
	auto Result = llvm::make_unique<PrtStatAST>(std::move(Args));
	return Result;
}

//ParseWhileExpr - ʵ��Whileѭ��
static std::unique_ptr<ExprAST> ParseWhileExpr() {
	ParseParenExpr();
	CurTok = getNextToken();
	if (CurTok == -14) {
		CurTok = getNextToken();
		switch (CurTok) {
		case -4:
		case -9:
		case -13:ParseStat(); break;
		case '{':ParseStats(); break;
			//default:error(); break;
		}
		//else error();
		CurTok = getNextToken();
		if (CurTok == -15) return WhileStatAST;
		//else error(); //����done���԰�ȫ�˳�
	}
}

//ParseIfExpr - ʵ��If�ж�
static std::unique_ptr<ExprAST> ParseIfExpr() {
	
	ParseParenExpr(); //����if���������
	CurTok = getNextToken();
	if (CurTok == -10) {
		CurTok = getNextToken();
		switch (CurTok) {
		case -4:
		case -9:
		case -13:ParseStat(); break;
		case '{':ParseStats(); break;
			//default:error(); break;
		}
		//else error();
		CurTok = getNextToken();
		if (CurTok == -12) return IfStatAST;
		//else error(); //����fi���԰�ȫ�˳�
	}
}

//ParseDclrExpr - ʵ�ֱ�������������
static std::unique_ptr<ExprAST> ParseDclrExpr() {

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
			return nullptr;

		// If BinOp binds less tightly with RHS than the operator after RHS, let
		// the pending operator take RHS as its LHS.
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
			if (!RHS)
				return nullptr;
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
	//�����������LHSΪ�գ��������Ƿ�Ϊ'-'
	std::unique_ptr<ExprAST> LHS = llvm::make_unique<ExprAST>();
	if (CurTok != '-') {
		LHS = ParsePrimary();
		if (!LHS)
			return nullptr;
	}

	return ParseBinOpRHS(0, std::move(LHS));
}

/// prototype
///   ::= id '(' id* ')'
static std::unique_ptr<PrototypeAST> ParsePrototype() {
	if (CurTok != tok_identifier)
		return LogErrorP("Expected function name in prototype");

	std::string FnName = IdentifierStr;
	getNextToken();

	if (CurTok != '(')
		return LogErrorP("Expected '(' in prototype");
	//VSL���Բ�����','������˴��޸�
	std::vector<std::string> ArgNames;
	while (getNextToken() == tok_identifier)
		ArgNames.push_back(IdentifierStr);
	if (CurTok != ')')
		return LogErrorP("Expected ')' in prototype");

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
	std::vector<std::unique_ptr<ExprAST>> Stats;
	if (CurTok != '{') {
		Stats.push_back(ParseStat());
		return llvm::make_unique<ExprsAST>(std::move(Stats));
	}
	else {
		//�����������﷨������������쳣����
		while (getNextToken() != '}') {
			Stats.push_back(ParseStat());
		}
		return llvm::make_unique<ExprsAST>(std::move(Stats));
	}
}

/// ParseStat - �����������ĺ���
static std::unique_ptr<ExprAST> ParseStat() {
	switch (CurTok)
	{
		//VAR
	case tok_var:
		ParseDclrExpr();

		//IF
	case tok_if:
		ParseIfExpr();

		//WHILE
	case tok_while:
		ParseWhileExpr();

		//IDENTI
	case tok_identifier:
		ParseIdentifierExpr();

		//RETURN
	case tok_return:
		ParseReturnExpr();

		//PRINT
	case tok_print:
		ParsePrintExpr();

	default:
		//Υ���﷨������
		LogError("Unknown Statement!");
		break;
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
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleDefinition() {
	if (ParseDefinition()) {
		fprintf(stderr, "Parsed a function definition.\n");
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleExtern() {
	if (ParseExtern()) {
		fprintf(stderr, "Parsed an extern\n");
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleTopLevelExpression() {
	// Evaluate a top-level expression into an anonymous function.
	if (ParseTopLevelExpr()) {
		fprintf(stderr, "Parsed a top-level expr\n");
	}
	else {
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleIf() {
	if (ParseIfExpr()) {
		fprintf(stderr, "Parse a if statement\n");
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
	}
	else {
		//Skip token for error recovery.
		getNextToken();
	}
}
static void HandleReturn() {
	if (ParseReturnExpr()) {
		fprintf(stderr, "Parse a return statement\n");
	}
	else {
		//Skip token for error recovery.
		getNextToken();
	}
}
>>>>>>> 119e5da5e784a42154170607991fc87359e22059
/// top ::= definition | external | expression | ';'
static void MainLoop() {
	while (true) {
		fprintf(stderr, "ready> ");
		switch (CurTok) {
		case tok_eof:
			return;
		case ';': // ignore top-level semicolons.
			getNextToken();
			break;
		case tok_func:
			HandleDefinition();
			break;
		case tok_extern:
			HandleExtern();
			break;
		case tok_if:
			HandleIf();
			break;
		case tok_while:
			HandleWhile();
			break;
		case tok_print:
			HandlePrint();
			break;
		case tok_return:
			HandleReturn();
			break;
		case tok_continue:
			HandleContinue();
			break;
		case tok_var:
			HandleDeclaration();
			break;
		case '{':
			ParseStats();
			break;
		default:
			HandleTopLevelExpression();
			break;
		}
	}
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
	// Install standard binary operators.
	// 1 is lowest precedence.
	BinopPrecedence['<'] = 10;
	BinopPrecedence['>'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40;
	BinopPrecedence['/'] = 40;// highest.
	//������������������ȼ�


	// Prime the first token.
	fprintf(stderr, "ready> ");
	getNextToken();

	// Run the main "interpreter loop" now.
	MainLoop();

	return 0;
}