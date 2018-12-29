#pragma once
#include "KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
using namespace llvm::orc;