#include "Include.h"
#pragma once
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
	while (isspace(LastChar)) {
		LastChar = getchar();
	}


	if (isalpha(LastChar)) { //��ĸ
		IdentifierStr = LastChar;
		while (isalnum((LastChar = getchar())))//��ĸ������
			IdentifierStr += LastChar;
		//���¿���strcmp()������ʹ��
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

	//�˴��޸Ķ�ע�͵Ĵ���������'#'������"//"
	//��ע�͵Ĵ������޸�Ϊ"//"
	if (LastChar == '/') {
		//����ǳ��������ظó�������
		if ((LastChar = getchar()) != '/')
			return '/';

		// Comment until end of line.
		do
			LastChar = getchar();
		while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		//�˴�����/r/n�����
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