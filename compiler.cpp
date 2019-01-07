#include "Include.h"
#include "Parser.h"
#include "Codegen.h"
#include "WTF.h"

//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

static void InitializeModuleAndPassManager() {
	// Open a new module.
	TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
	TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

	// Create a new pass manager attached to it.
	TheFPM = llvm::make_unique<legacy::FunctionPassManager>(TheModule.get());

	// Promote allocas to registers.
	TheFPM->add(createPromoteMemoryToRegisterPass());
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
			TheJIT->addModule(std::move(TheModule));
			InitializeModuleAndPassManager();
		}
	}
	else {
		//Skip token for error recovery.
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

static void HandleContinue() {
	if (ParseNullExpr()) {
		fprintf(stderr, "Parsed a null expression.\n");
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
			//期待下一项输出
			getNextToken();
			break;
		}
	}
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();

	// Install standard binary operators.
	// 1 is lowest precedence.
	BinopPrecedence[':='] = 2;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 20;
	BinopPrecedence['*'] = 40;
	BinopPrecedence['/'] = 40;// highest.
	//这里增加运算符的优先级

	// Prime the first token.
	fprintf(stderr, "ready> ");
	getNextToken();
	TheJIT = llvm::make_unique<KaleidoscopeJIT>();
	InitializeModuleAndPassManager();
	
	// Run the main "interpreter loop" now.
	MainLoop();
	
	//system("pause");
	return 0;
}

