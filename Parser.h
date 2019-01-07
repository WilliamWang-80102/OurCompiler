#pragma once
#include "Lexer.h"
#include "AST.h"

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static int CurTok;
static int getNextToken() {
	CurTok = gettok();
	//如果读到':'，则检查是否为赋值符号':='
	if (CurTok == ':') {
		CurTok = gettok();
		if (CurTok == '=') {
			CurTok = ':=';
		}
		else {
			fprintf(stderr, "Error: 错误的赋值符号 :=");
		}
	}
	return CurTok;
}

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
	if ((CurTok != ':=') && !isascii(CurTok))
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

static std::unique_ptr<ExprAST> ParseNumberExpr();
static std::unique_ptr<ExprAST> ParseParenExpr();
static std::unique_ptr<ExprAST> ParseIdentifierExpr();
static std::unique_ptr<ExprAST> ParseBinOpRHS(int, std::unique_ptr<ExprAST>);
static std::unique_ptr<ExprAST> ParseExpression();
static std::unique_ptr<ExprAST> ParseReturnExpr();
static std::unique_ptr<ExprAST> ParseWhileExpr();
static std::unique_ptr<ExprAST> ParseIfExpr();
static std::unique_ptr<ExprAST> ParseDclrExpr();
static std::unique_ptr<ExprAST> ParsePrimary();
static std::unique_ptr<PrototypeAST> ParsePrototype();
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

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
///   ::= identifier ':= ' expression
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;
	getNextToken(); // eat identifier.
	// 简单变量
	if (CurTok != '(')
		return llvm::make_unique<VariableExprAST>(IdName);

	// Call.
	getNextToken(); // eat (
	std::vector<std::unique_ptr<ExprAST>> Args;
	//该if语句可以优化
	if (CurTok != ')') {
		while (true) {
			if (auto Arg = ParseExpression()) {
				Args.push_back(std::move(Arg));
			}
			else {
				return nullptr;
			}

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

//ParseReturnExpr - 实现返回语句
static std::unique_ptr<ExprAST> ParseReturnExpr()
{
	getNextToken();//eat RETURN
	std::unique_ptr<ExprAST> Expr = ParseExpression();
	if (Expr)
		return llvm::make_unique<RetStatAST>(std::move(Expr));
	else
		return LogError("Expect return value!");
}

//处理IF和WHILE后面的条件语句
static std::unique_ptr<ExprAST> ParseConditionExpr() {
	std::unique_ptr<ExprAST> Cond = ParseExpression();
	if (Cond) {
		return std::move(Cond);
	}
	else return LogError("Expect condition statement!");
}

//ParseWhileExpr - 实现While循环
static std::unique_ptr<ExprAST> ParseWhileExpr() {
	getNextToken();//eat WHILE
	std::unique_ptr<ExprAST> Cond;
	std::unique_ptr<ExprAST> Stats;
	std::unique_ptr<WhileStatAST> WhilePtr;
	Cond = ParseConditionExpr();
	if (CurTok == tok_do) {
		getNextToken();//eat DO
		Stats = ParseStats();
		if (!Stats) return LogError("Expect 'DO' statements!");
	}
	else return LogError("Expect 'DO'!");
	if (CurTok == tok_done) {
		getNextToken();//eat DONE
		WhilePtr = llvm::make_unique<WhileStatAST>(std::move(Cond), std::move(Stats));
		return WhilePtr;
	}
	else return LogError("Expect 'DONE'!");//读到done可以安全退出
}

//ParseIfExpr - 实现If判断
static std::unique_ptr<ExprAST> ParseIfExpr() {
	getNextToken();//eat IF
	std::unique_ptr<ExprAST> Cond, ThenStat, ElseStat;
	std::unique_ptr<IfStatAST> IfPtr;
	Cond = ParseConditionExpr(); //分析if后面的条件
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

//ParseDclrExpr - 实现变量声明语句解析
static std::unique_ptr<ExprAST> ParseDclrExpr() {
	getNextToken(); // eat the var.

	std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

	// At least one variable name is required.
	if (CurTok != tok_identifier)
		return LogError("expected identifier after var");

	while (true) {
		std::string Name = IdentifierStr;
		getNextToken(); // eat identifier.

		// Read the optional initializer.
		std::unique_ptr<ExprAST> Init = nullptr;
		if (CurTok == ':=') {
			getNextToken(); // eat the ':='.

			Init = ParseExpression();
			if (!Init)
				return nullptr;
		}

		VarNames.push_back(std::make_pair(Name, std::move(Init)));

		// End of var list, exit loop.
		if (CurTok != ',')
			break;
		getNextToken(); // eat the ','.

		if (CurTok != tok_identifier)
			return LogError("expected identifier list after var");
	}

	return llvm::make_unique<DeclareExprAST>(std::move(VarNames));
}

/// primary 是一个表达式中的基本单元，包括identifierexpr（变量， 函数调用， 赋值表达式）, numberexpr, parenexpr
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

/// unary
///   ::= primary
///   ::= '!' unary
static std::unique_ptr<ExprAST> ParseUnary() {
	// If the current token is not an operator, it must be a primary expr.
	if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
		return ParsePrimary();

	// If this is a unary operator, read it.
	int Opc = CurTok;
	getNextToken();
	if (auto Operand = ParseUnary())
		return llvm::make_unique<UnaryExprAST>(Opc, std::move(Operand));
	return nullptr;
}

/// binoprhs
///   ::= ('+' unary)*
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

		// Parse the unary expression after the binary operator.
		auto RHS = ParseUnary();
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
		LHS =
			llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
	}
}

/// expression
///   ::= unary binoprhs
///
static std::unique_ptr<ExprAST> ParseExpression() {
	auto LHS = ParseUnary();
	if (!LHS)
		return nullptr;

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
	//VSL语言参数以','间隔，此处修改
	//已修改 段
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

	//此处改为分析函数体的语句
	if (auto E = ParseStats())
		return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(E));
	return nullptr;
}

/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
	getNextToken(); // eat extern.
	return ParsePrototype();
}

/// ParseStats - 分析语句块的函数
static std::unique_ptr<ExprsAST> ParseStats() {
	//getNextToken();
	std::vector<std::unique_ptr<ExprAST>> Stats;
	if (CurTok != '{') {
		Stats.push_back(std::move(ParseStat()));
		return llvm::make_unique<ExprsAST>(std::move(Stats));
	}
	else {
		//仅考虑正常语法情况，需增加异常处理
		getNextToken();//滤掉'{'
		while (CurTok != '}') {
			Stats.push_back(std::move(ParseStat()));
		}
		//消耗掉'}'
		getNextToken();
		return llvm::make_unique<ExprsAST>(std::move(Stats));
	}
}

/// ParseStat - 分析单条语句的函数
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

		//RETURN
	case tok_return:
		return ParseReturnExpr();

	default:
		return ParseExpression();
	}
}
