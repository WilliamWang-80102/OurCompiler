#pragma once
#include "Include.h"
//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (aka Parse Tree)
//===----------------------------------------------------------------------===//

namespace {
	/// ExprAST - Base class for all expression nodes.
	class ExprAST {
	public:
		virtual ~ExprAST() = default;
		virtual Value *codegen() = 0;
		virtual char *getType() = 0;
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST {
		double Val;

	public:
		NumberExprAST(double Val) : Val(Val) {}
		Value *codegen() override;
		char *getType() override { return "NumberExprAST"; }
	};

	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST {
		std::string Name;

	public:
		VariableExprAST(const std::string &Name) : Name(Name) {}
		Value *codegen() override;
		const std::string &getName() const { return Name; }
		char *getType() override { return "VariableExprAST"; }
	};

	/// DeclareExprAST - Expression like 'VAR x,y,z'.
	class DeclareExprAST : public ExprAST {

		std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
		//std::unique_ptr<ExprAST> Body;
	public:
		DeclareExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames)
			: VarNames(std::move(VarNames)) {}

		Value *codegen() override;
		char *getType() override { return "DeclareExprAST"; }
	};

	/// UnaryExprAST - Expression class for a unary operator.
	class UnaryExprAST : public ExprAST {
		char Opcode;
		std::unique_ptr<ExprAST> Operand;

	public:
		UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
			: Opcode(Opcode), Operand(std::move(Operand)) {}

		Value *codegen() override;
		char *getType() override { return "UnaryExprAST"; }
	};

	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST {
		int Op;
		std::unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS,
			std::unique_ptr<ExprAST> RHS)
			: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		Value *codegen() override;
		char *getType() override { return "BinaryExprAST"; }
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
		char *getType() override { return "CallExprAST"; }
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
		// 函数的定义被修改为“签名” + “语句块”的形式
		std::unique_ptr<ExprAST> Body;
	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
			std::unique_ptr<ExprAST> Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}
		Function *codegen();
	};

	///ExprsAST - 语句块表达式结点
	class ExprsAST : public ExprAST {
		std::vector<std::unique_ptr<ExprAST>> Stats; //多句表达式向量
	public:
		ExprsAST(std::vector<std::unique_ptr<ExprAST>> Stats)
			:Stats(std::move(Stats)) {}
		Value *codegen() override;
		char *getType() override { return "ExprsAST"; }
	};

	///RetStatAST - 返回语句结点
	class RetStatAST : public ExprAST {
		std::unique_ptr<ExprAST> Expr; // 返回语句后面的表达式
	public:
		RetStatAST(std::unique_ptr<ExprAST> Expr) : Expr(std::move(Expr)) {}
		Value *codegen() override;
		char *getType() override { return "RetStatAST"; }
	};

	/// IfStatAST - 条件语句结点
	class IfStatAST : public ExprAST {
		std::unique_ptr<ExprAST> Cond; // 条件表达式
		std::unique_ptr<ExprAST> Then; // Cond 为True后的执行语句块

		std::unique_ptr<ExprAST> Else; // Cond 为False后的执行语句块
	public:
		IfStatAST(std::unique_ptr<ExprAST> Cond,
			std::unique_ptr<ExprAST> ThenStat,
			std::unique_ptr<ExprAST> ElseStat)
			:Cond(std::move(Cond)),
			Then(std::move(ThenStat)),
			Else(std::move(ElseStat)) {}
		Value *codegen() override;
		char *getType() override { return "IfStatAST"; }
	};

	/// WhileStatAST - 当循环语句结点
	class WhileStatAST : public ExprAST {
		std::unique_ptr<ExprAST> Cond; // 条件表达式
		std::unique_ptr<ExprAST> Stat; // Cond 为True后的执行语句块
	public:
		WhileStatAST(std::unique_ptr<ExprAST> Cond,
			std::unique_ptr<ExprAST> Stat)
			:Cond(std::move(Cond)), Stat(std::move(Stat)) {}
		Value *codegen() override;
		char *getType() override { return "WhileStatAST"; }
	};

} // end anonymous namespace