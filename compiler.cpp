#include "Include.h"
#include "Parser.h"
#include "Codegen.h"
#include "WTF.h"

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

