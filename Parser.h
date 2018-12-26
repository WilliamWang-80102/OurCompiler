#pragma once
#include "Include.h"
#include "Lexer.h"
#include "AST.h"

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
