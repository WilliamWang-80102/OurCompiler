#pragma once
#include "AST.h"
//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;
static std::map<std::string, AllocaInst*> NamedValues;
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

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
	const std::string &VarName) {
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
		TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), nullptr, VarName);
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
	// Load the value.
	return Builder.CreateLoad(V, Name.c_str());
}

//二元表达式中间代码生成
Value *BinaryExprAST::codegen() {
	// Special case ':=' because we don't want to emit the LHS as an expression.
	if (Op == ':=') {
		// Assignment requires the LHS to be an identifier.
		VariableExprAST *LHSE = dynamic_cast<VariableExprAST*>(LHS.get());
		if (!LHSE)
			return LogErrorV("destination of ':=' must be a variable");
		// Codegen the RHS.
		Value *Val = RHS->codegen();
		if (!Val)
			return nullptr;
		// Look up the name.
		Value *Variable = NamedValues[LHSE->getName()];
		if (!Variable)
			return LogErrorV("Unknown variable name");
		Builder.CreateStore(Val, Variable);
		return Val;
	}
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
	Function *CalleeF = getFunction(Callee);
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

Value *WhileStatAST::codegen() {
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
	std::string VarName = "i";
	AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
	Value *CondV = Cond->codegen();
	if (!CondV) return nullptr;
	Builder.CreateStore(CondV, Alloca);
	Value * ExprV1 = 
		Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)), "loopcond");
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
	if (ThenV == Constant::getNullValue(Type::getDoubleTy(TheContext)))
		Builder.CreateBr(MergeBB);

	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = Builder.GetInsertBlock();

	// Emit else block.
	TheFunction->getBasicBlockList().push_back(ElseBB);
	Builder.SetInsertPoint(ElseBB);

	Value *ElseV = Else->codegen();
	if (ElseV == Constant::getNullValue(Type::getDoubleTy(TheContext)))
		Builder.CreateBr(MergeBB);

	// Codegen of 'Else' can change the current block, update ElseBB for the PHI.
	ElseBB = Builder.GetInsertBlock();
	
	// Emit merge block.
	TheFunction->getBasicBlockList().push_back(MergeBB);
	Builder.SetInsertPoint(MergeBB);
	/*
	PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");
	PN->addIncoming(ThenV, ThenBB);
	PN->addIncoming(ElseV, ElseBB);
	*/
	
	return Constant::getNullValue(Type::getDoubleTy(TheContext));
}

Value *DeclareExprAST::codegen() {

	Function *TheFunction = Builder.GetInsertBlock()->getParent();

	// Register all variables and emit their initializer.
	for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
		const std::string &VarName = VarNames[i].first;
		ExprAST *Init = VarNames[i].second.get();
		// Emit the initializer before adding the variable to scope, this prevents
		// the initializer from referencing the variable itself, and permits stuff
		// like this:
		//  var a = 1 in
		//    var a = a in ...   # refers to outer 'a'.
		Value *InitVal;
		if (Init) {
			InitVal = Init->codegen();
			if (!InitVal)
				return nullptr;
		}
		else { // If not specified, use 0.0.
			InitVal = ConstantFP::get(TheContext, APFloat(0.0));
		}

		AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
		Builder.CreateStore(InitVal, Alloca);
		/*
		// Remember the old variable binding so that we can restore the binding when
		// we unrecurse.
		OldBindings.push_back(NamedValues[VarName]);
		*/

		// Remember this binding.
		NamedValues[VarName] = Alloca;
	}

	// Return the body computation.
	return Constant::getNullValue(Type::getDoubleTy(TheContext));
}

Value *UnaryExprAST::codegen() {
	Value *OperandV = Operand->codegen();
	if (!OperandV)
		return nullptr;

	Function *F = getFunction(std::string("unary") + Opcode);
	if (!F)
		return LogErrorV("Unknown unary operator");

	return Builder.CreateCall(F, OperandV, "unop");
}

Value *ExprsAST::codegen() {
	Value * BlockValue = Constant::getNullValue(Type::getDoubleTy(TheContext));
	//将语句块的内容全部转化为ir
	for (std::vector<std::unique_ptr<ExprAST>>::const_iterator iter = Stats.cbegin(); 

		iter != Stats.cend(); iter++) {
		//如果语句为返回语句，将返回值计算给BlockValue
		if (strcmp((*iter)->getType(), "RetStatAST") == 0 ) {
			BlockValue = (*iter)->codegen();
		}
		else (*iter)->codegen();
	}
	//语句块分析结束，不返回指针
	return BlockValue;
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
	for (auto &Arg : TheFunction->args()) {
		// Create an alloca for this variable.
		AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());

		// Store the initial value into the alloca.
		Builder.CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues[Arg.getName()] = Alloca;
	}

	Body->codegen();

	// Validate the generated code, checking for consistency.
	if (verifyFunction(*TheFunction)) {
		Builder.CreateRet(Constant::getNullValue(Type::getDoubleTy(TheContext)));
		if (verifyFunction(*TheFunction)) {
			// Error reading body, remove function.
			TheFunction->eraseFromParent();
			return nullptr;
		}
	}
	// Run the optimizer on the function.
	//TheFPM->run(*TheFunction);

	return TheFunction;
	
}

//返回语句代码生成codegen()
Value *RetStatAST::codegen()
{
	Value * RetValue = Expr->codegen();
	return Builder.CreateRet(RetValue);
}