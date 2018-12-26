#include "Include.h"
#pragma once
//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
	//文件结束符
	tok_eof = -1,

	// commands
	//函数定义符
	tok_func = -2,
	//函数申明符
	tok_extern = -3,

	// primary
	//标识符
	tok_identifier = -4,
	//数字
	tok_number = -5,
	//return
	tok_return = -6,
	//print

	tok_print = -7,
	//在这里补充VSL的关键字FUNC等....
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
/// 请修改gettok代码，在读到合适的单词后，准确分析它的语法属性，返回词性。
static int gettok() {
	static int LastChar = ' ';

	// Skip any whitespace.
	while (isspace(LastChar)) {
		LastChar = getchar();
	}


	if (isalpha(LastChar)) { //字母
		IdentifierStr = LastChar;
		while (isalnum((LastChar = getchar())))//字母或数字
			IdentifierStr += LastChar;
		//以下考虑strcmp()函数的使用
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

	//此处应修改，避免出现1.2.3仍能通过的情况，修改后请删去本行
	if (isdigit(LastChar) || LastChar == '.') { // 数字
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

	//此处修改对注释的处理，不再是'#'，而是"//"
	//对注释的处理已修改为"//"
	if (LastChar == '/') {
		//如果是除法，返回该除法符号
		if ((LastChar = getchar()) != '/')
			return '/';

		// Comment until end of line.
		do
			LastChar = getchar();
		while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		//此处考虑/r/n的情况
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