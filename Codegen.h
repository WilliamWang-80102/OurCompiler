#pragma once
#include "Parser.h"
#include "AST.h"
//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, Value *> NamedValues;
static std::unique_ptr<legacy::FunctionPassManager> TheFPM;
static std::unique_ptr<KaleidoscopeJIT> TheJIT;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

Value *LogErrorV(const char *Str) {
	LogError(Str);
	return nullptr;
}

Function *getFunction(std::string Name) {
	// First, see if the function has already been added to the current module.
	if (auto *F = TheModule->getFunction(Name))
		return F;

	// If not, check whether we can codegen the declaration from some existing
	// prototype.
	auto FI = FunctionProtos.find(Name);
	if (FI != FunctionProtos.end())
		return FI->second->codegen();

	// If no existing prototype exists, return null.
	return nullptr;
}

//常数处理
Value *NumberExprAST::codegen() {
	return ConstantFP::get(TheContext, APFloat(Val));
}

//单个变量中间代码处理
Value *VariableExprAST::codegen() {
	// Look this variable up in the function.
	Value *V = NamedValues[Name];
	if (!V)
		LogErrorV("Unknown variable name");
	return V;
}

//二元表达式中间代码生成
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
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
	const std::string &VarName) {
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
		TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), 0,
		VarName.c_str());
}
Value *WhileStatAST::codegen() {
	/*Function *TheFunction = Builder.GetInsertBlock()->getParent();
	BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
	BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);

	Value *EndCond = Cond->codegen();
	if (!EndCond)
		return nullptr;
	EndCond= Builder.CreateFCmpONE(EndCond, ConstantFP::get(TheContext,APFloat(0.0)),"loopCondIn");
	Builder.CreateCondBr(EndCond,LoopBB,AfterBB);
	Builder.SetInsertPoint(LoopBB);
	Value *inLoopVal = Stat->codegen();
	if (!inLoopVal)
		return nullptr;
	EndCond = Builder.CreateFCmpONE(Cond->codegen(), ConstantFP::get(TheContext, APFloat(0.0)), "loopCondOut");
	Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
	Builder.SetInsertPoint(AfterBB);
	return Constant::getNullValue(Type::getDoubleTy(TheContext));*/
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
	std::string VarName = "i";
	AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
	Value *CondV = Cond->codegen();
	if (!CondV)
			return nullptr;
		Builder.CreateStore(CondV, Alloca);
		Value * ExprV1 = Builder.CreateFCmpONE(
			CondV, ConstantFP::get(TheContext, APFloat(0.0)), "loopcond");
		BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
		BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop");
		Builder.CreateCondBr(ExprV1, LoopBB, AfterBB);
		//Builder.CreateBr(LoopBB);
		Builder.SetInsertPoint(LoopBB);
		//AllocaInst *OldVal = NamedValues[VarName];
		NamedValues[VarName] = Alloca;
	
		if (!Stat->codegen())
			return nullptr;
	
		//Value *CurVar = Builder.CreateLoad(Alloca, VarName.c_str());
		Value *CurVar = Cond->codegen();
		CurVar = Builder.CreateFCmpONE(
			CurVar, ConstantFP::get(TheContext, APFloat(0.0)), "loopcond1");
	
		BasicBlock *LoopEndBB = Builder.GetInsertBlock();
		Builder.CreateCondBr(CurVar, LoopBB, AfterBB);
		TheFunction->getBasicBlockList().push_back(AfterBB);
		Builder.SetInsertPoint(AfterBB);
	
		
		/*if (OldVal)
		NamedValues[VarName] = OldVal;
		else
		NamedValues.erase(VarName);*/
	
		// for expr always returns 0.0.
		return Constant::getNullValue(Type::getDoubleTy(TheContext));
}

Value *IfStatAST::codegen() {
	Value *CondV = Cond->codegen();
	if (!CondV)
		return nullptr;

	// Convert condition to a bool by comparing non-equal to 0.0.
	CondV = Builder.CreateFCmpONE(
		CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");

	Function *TheFunction = Builder.GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  Insert the 'then' block at the
	// end of the function.
	BasicBlock *ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);
	BasicBlock *ElseBB = BasicBlock::Create(TheContext, "else");
	BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");

	Builder.CreateCondBr(CondV, ThenBB, ElseBB);

	// Emit then value.
	Builder.SetInsertPoint(ThenBB);

	Value *ThenV = Then->codegen();
	if (!ThenV)
		return nullptr;

	Builder.CreateBr(MergeBB);
	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = Builder.GetInsertBlock();

	// Emit else block.
	TheFunction->getBasicBlockList().push_back(ElseBB);
	Builder.SetInsertPoint(ElseBB);

	Value *ElseV = Else->codegen();
	if (!ElseV)
		return nullptr;

	Builder.CreateBr(MergeBB);
	// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
	ElseBB = Builder.GetInsertBlock();

	// Emit merge block.
	TheFunction->getBasicBlockList().push_back(MergeBB);
	Builder.SetInsertPoint(MergeBB);
	PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

	PN->addIncoming(ThenV, ThenBB);
	PN->addIncoming(ElseV, ElseBB);
	return PN;
}

Value *DeclareExprAST::codegen() {
	return nullptr;
}

Value *ExprsAST::codegen() {
	/*
	//将语句块的内容全部转化为ir
	for (std::vector<std::unique_ptr<ExprAST>>::const_iterator iter = Stats.cbegin(); iter != Stats.cend(); iter++) {
	(*iter)->codegen();
	}
	//语句块分析结束，不返回指针
	return nullptr;
	*/
	//调试各生成代码阶段，语句块只有一句
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
	// Transfer ownership of the prototype to the FunctionProtos map, but keep a
	// reference to it for use below.
	auto &P = *Proto;
	FunctionProtos[Proto->getName()] = std::move(Proto);
	Function *TheFunction = getFunction(P.getName());
	if (!TheFunction)
		return nullptr;

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

		// Run the optimizer on the function.
		TheFPM->run(*TheFunction);

		return TheFunction;
	}

	// Error reading body, remove function.
	TheFunction->eraseFromParent();
	return nullptr;
}

//string的代码生成
Value *StringAST::Codegen()
{
	Value *V = NamedValues[str];
	return V;
}

//返回语句代码生成codegen()
Value *RetStatAST::codegen()
{
	Value *RetValue = Expr->codegen();
	/*
	return Builder.CreateRet(RetValue);
	*/
	return RetValue;
}

//打印语句代码生成codegen()
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
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static void InitializeModuleAndPassManager() {
	// Open a new module.
	TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
	TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

	// Create a new pass manager attached to it.
	TheFPM = llvm::make_unique<legacy::FunctionPassManager>(TheModule.get());

	// Do simple "peephole" optimizations and bit-twiddling optzns.
	TheFPM->add(createInstructionCombiningPass());
	// Reassociate expressions.
	TheFPM->add(createReassociatePass());
	// Eliminate Common SubExpressions.
	TheFPM->add(createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	TheFPM->add(createCFGSimplificationPass());

	TheFPM->doInitialization();
}

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
			//TheJIT->addModule(std::move(TheModule));
			//InitializeModuleAndPassManager();

		}
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
			//非函数体报错
		default:
			LogError("Error!");
			fprintf(stderr, "ready> ");
			//getNextToken();
			/*
			//报错则清空输入流中的数据，期待用户重新输入
			while (getNextToken() != '\n') {}
			*/
			//期待下一项输出
			getNextToken();
			break;
		}
	}
}