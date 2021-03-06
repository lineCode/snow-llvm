#include "lexer.hpp"

#include <alloca.h>

/*
EOF, EOL, DO, UNLESS, ELSE, IF, ELSEIF, WHILE, UNTIL, TRY, CATCH, ENSURE, END,
RETURN, BREAK, CONTINUE, SELF, CURRENT_SCOPE, INTEGER, FLOAT, STRING, TRUE, FALSE, NIL,
IDENTIFIER, PARALLEL_THREAD, PARALLEL_FORK, OPERATOR_FOURTH, LOG_AND, LOG_OR, LOG_XOR,
LOG_NOT, OPERATOR_THIRD, OPERATOR_SECOND, OPERATOR_FIRST, DOT
*/

namespace {
	// Utilities
	
	inline bool is_ascii_letter(const char* utf8) {
		char c = *utf8;
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}
	
	inline bool is_special_word_character(const char* utf8) {
		char c = *utf8;
		return (c == '_') || (c == '@');
	}
	
	inline bool is_unicode_control_byte(char c) {
		return (c & 0x80) == 0x80;
	}
	
	inline bool is_unicode_letter(const char* utf8, size_t& out_char_len) {
		const char* p = utf8;
		if (is_unicode_control_byte(*p)) {
			do {
				++p;
			} while (is_unicode_control_byte(*p));
			out_char_len = p - utf8;
			return true;
		}
		return false;
	}
	
	inline bool is_word_character(const char* utf8, size_t& out_char_len) {
		out_char_len = 1;
		if (is_ascii_letter(utf8))           { out_char_len = 1; return true; }
		if (is_special_word_character(utf8)) { out_char_len = 1; return true; }
		if (is_unicode_letter(utf8, out_char_len)) { return true; }
		return false;
	}
	
	inline bool is_operator_character(const char* utf8, size_t& out_char_len) {
		out_char_len = 1;
		switch (*utf8) {
			case '+':
			case '-':
			case '*':
			case '/':
			case '=':
			case '!':
			case '~':
			case '^':
			case '&':
			case '|':
			case '<':
			case '>':
			case '%':
			case '\\':
				return true;
			default:
				return false;
		}
	}
	
	inline bool is_operator(const char* utf8, size_t& out_operator_len) {
		size_t char_len;
		if (is_operator_character(utf8, char_len)) {
			out_operator_len = char_len;
			while (is_operator_character(utf8+out_operator_len, char_len)) out_operator_len += char_len;
			return true;
		}
		return false;
	}
	
	inline bool is_number(const char* p) {
		char c = *p;
		if (c >= '0' && c <= '9') return true;
		return false;
	}
	
	inline bool is_numeric_begin(const char* p, bool& out_floating_point) {
		char c = *p;
		if (is_number(p)) return true;
		if (c == '.' && is_number(p+1)) { out_floating_point = true; return true; }
		return false;
	}
	
	inline bool is_plus_or_minus(const char* p) {
		return (*p == '+' || *p == '-');
	}
	
	inline bool is_numeric(const char* p, bool& out_floating_point) {
		char c = *p;
		if (is_number(p)) return true;
		if ((c == 'e' || c == 'E') && (is_number(p+1) || (is_plus_or_minus(p+1) && is_number(p+2)))) { out_floating_point = true; return true; }
		char before = *(p-1);
		if (is_plus_or_minus(p) && (before == 'e' || before == 'E')) { out_floating_point = true; return true; }
		if (c == '.' && is_number(p+1)) { out_floating_point = true; return true; }
		return false;
	}
	
	inline bool is_whitespace(const char* p, size_t& out_char_len) {
		out_char_len = 1; // TODO: Unicode whitespace?
		switch (*p) {
			case ' ': return true;
			case '\t': return true;
			case '\n': return true;
			case '\r': return true;
			default: return false;
		}
	}
	
	inline bool is_alphanumeric_character(const char* utf8, size_t& out_char_len) {
		if (is_word_character(utf8, out_char_len)) return true;
		if (is_number(utf8)) { out_char_len = 1; return true; }
		return false;
	}
	
	template <typename T>
	inline T min(T a, T b) {
		if (a < b) return a;
		return b;
	}
	
	inline bool strings_equal(const char* s1, size_t len1, const char* s2, size_t len2) {
		if (len1 != len2) return false;
		return strncmp(s1, s2, len1) == 0;
	}
	
	inline bool strings_equal(const char* s1, size_t len1, const char* s2) {
		size_t len2 = strlen(s2);
		return strings_equal(s1, len1, s2, len2);
	}
}

#define RECOGNIZE_KEYWORD(STR, TOKEN) if (strings_equal(p, word_len, (STR))) { type = Token::TOKEN; }
#define RECOGNIZE_CHAR(CHAR, TOKEN) case CHAR: { type = Token::TOKEN; break; }

namespace snow {
	void Lexer::tokenize() {
		_buffer.clear();
		
		const char* p = _input;
		const char* end = _input+_input_length;
		int current_line_number = 1;
		const char* current_line_begin = p;
		int multiline_comment_level = 0;
		bool single_line_comment = false;
		while (p < end) {
			size_t char_len;
			
			// Handle comments
			if (*p == '-' && *(p+1) == '*') {
				p += 2;
				++multiline_comment_level;
				continue;
			}
			
			if (multiline_comment_level > 0) {
				if (*p == '*' && *(p+1) == '-') {
					p += 2;
					--multiline_comment_level;
				} else {
					if (*p == '\n') {
						++current_line_number;
						current_line_begin = p+1;
					}
					++p;
				}
				continue;
			}
			
			if (*p == '-' && *(p+1) == '-') {
				p += 2;
				single_line_comment = true;
				continue;
			}
			
			if (single_line_comment && *p == '\n') {
				++p;
				single_line_comment = false;
				continue;
			}
			
			if (single_line_comment) {
				if (*p == '\n') {
					single_line_comment = false;
					++current_line_number;
					current_line_begin = p+1;
				}
				++p;
				continue;
			}
			
			// Handle whitespace
			if (is_whitespace(p, char_len)) {
				//printf("it is whitespace!\n");
				if (*p == '\n') {
					_buffer.push(Token(Token::END_OF_LINE, p, 1, current_line_number, current_line_begin));
					++current_line_number;
					current_line_begin = p+1;
				}
				p += char_len;
				continue;
			}
			
			// Handle end of statement
			if (*p == ';') {
				_buffer.push(Token(Token::END_OF_STATEMENT, p, 1, current_line_number, current_line_begin));
				++p;
				continue;
			}
			
			// Handle keywords
			if (is_word_character(p, char_len)) {
				//printf("it is a word!\n");
				const char* q = p+char_len;
				while (is_alphanumeric_character(q, char_len)) q+=char_len;
				if (*q == '?') { ++q; }
				const size_t word_len = q-p;
				
				Token::Type type;
				
				RECOGNIZE_KEYWORD("do", DO)
				else RECOGNIZE_KEYWORD("then", THEN)
				else RECOGNIZE_KEYWORD("end", END)
				else RECOGNIZE_KEYWORD("if", IF)
				else RECOGNIZE_KEYWORD("else", ELSE)
				else RECOGNIZE_KEYWORD("unless", UNLESS)
				else RECOGNIZE_KEYWORD("while", WHILE)
				else RECOGNIZE_KEYWORD("until", UNTIL)
				else RECOGNIZE_KEYWORD("try", TRY)
				else RECOGNIZE_KEYWORD("catch", CATCH)
				else RECOGNIZE_KEYWORD("ensure", ENSURE)
				else RECOGNIZE_KEYWORD("return", RETURN)
				else RECOGNIZE_KEYWORD("break", BREAK)
				else RECOGNIZE_KEYWORD("continue", CONTINUE)
				else RECOGNIZE_KEYWORD("self", SELF)
				else RECOGNIZE_KEYWORD("here", HERE)
				else RECOGNIZE_KEYWORD("it", IT)
				else RECOGNIZE_KEYWORD("true", TRUE)
				else RECOGNIZE_KEYWORD("false", FALSE)
				else RECOGNIZE_KEYWORD("nil", NIL)
				else RECOGNIZE_KEYWORD("and", LOG_AND)
				else RECOGNIZE_KEYWORD("or", LOG_OR)
				else RECOGNIZE_KEYWORD("xor", LOG_XOR)
				else RECOGNIZE_KEYWORD("not", LOG_NOT)
				else RECOGNIZE_KEYWORD("as", AS)
				else {
					type = Token::IDENTIFIER;
				}
				
				_buffer.push(Token(type, p, word_len, current_line_number, current_line_begin));
				p=q;
				continue;
			}
			
			// Handle numbers
			bool is_floating_point = false;
			if (is_numeric_begin(p, is_floating_point)) {
				//printf("it is a number!\n");
				const char* q = p+1;
				while (is_numeric(q, is_floating_point)) ++q;
				Token::Type type = is_floating_point ? Token::FLOAT : Token::INTEGER;
				_buffer.push(Token(type, p, q-p, current_line_number, current_line_begin));
				p=q;
				continue;
			}
			
			// Handle syntactic characters
			// TODO: Include pseudo-syntactic unicode ranges
			{
				Token::Type type;
				bool found = true;
				switch (*p) {
					RECOGNIZE_CHAR('(', PARENS_BEGIN)
					RECOGNIZE_CHAR(')', PARENS_END)
					RECOGNIZE_CHAR('[', BRACKET_BEGIN)
					RECOGNIZE_CHAR(']', BRACKET_END)
					RECOGNIZE_CHAR('{', BRACE_BEGIN)
					RECOGNIZE_CHAR('}', BRACE_END)
					RECOGNIZE_CHAR('.', DOT)
					RECOGNIZE_CHAR(',', COMMA)
					RECOGNIZE_CHAR(':', ASSIGN)
					RECOGNIZE_CHAR(';', SEMICOLON)
/*					RECOGNIZE_CHAR('´', TICK)
					RECOGNIZE_CHAR('`', BACKTICK)*/
					RECOGNIZE_CHAR('#', HASH)
					RECOGNIZE_CHAR('$', DOLLAR)
					default: { found = false; break; }
				}
				if (found) {
					_buffer.push(Token(type, p, 1, current_line_number, current_line_begin));
					++p;
					continue;
				}
			}
			
			// Handle operators
			{
				size_t operator_len;
				if (is_operator(p, operator_len)) {
					Token::Type t = Token::OPERATOR_SECOND;
					if (strings_equal(p, operator_len, "*")
					|| strings_equal(p, operator_len, "/")
					|| strings_equal(p, operator_len, "**")
					|| strings_equal(p, operator_len, "%")) {
						t = Token::OPERATOR_THIRD;          
					} else                                  
					if (strings_equal(p, operator_len, "=")
					|| strings_equal(p, operator_len, "!=")
					|| strings_equal(p, operator_len, "~=")
					|| strings_equal(p, operator_len, ">")
					|| strings_equal(p, operator_len, "<")
					|| strings_equal(p, operator_len, ">=")
					|| strings_equal(p, operator_len, "<=")
					|| strings_equal(p, operator_len, "==")) {
						t = Token::OPERATOR_FIRST;
					} else
					if (strings_equal(p, operator_len, "!")) {
						t = Token::LOG_NOT;
					}
					
					_buffer.push(Token(t, p, operator_len, current_line_number, current_line_begin));
					p += operator_len;
					continue;
				}
			}
			
			// Handle strings
			{
				// TODO: Maybe something clever about interpolation and escape chars?
				char lookfor = 0;
				bool terminated = false;
				if (*p == '\'' || *p == '\"') lookfor = *p;
				if (lookfor) {
					const char* q = p+1;
					while (q < end) {
						if (*q == lookfor && *(q-1) != '\\') {
							terminated = true;
							break;
						}
						++q;
					}
					_buffer.push(Token(lookfor == '\'' ? Token::SQSTRING : Token::DQSTRING, p+1, q-p-1, current_line_number, current_line_begin));
					p=q+1;
					continue;
				}
			}
			
			fprintf(stderr, "%s: Cannot tokenize (line %d col %ld): '%c'\n", _path.c_str(), current_line_number, p - current_line_begin, *p);
			++p;
		}
		_buffer.push(Token(Token::END_OF_FILE, p, 0, current_line_number, current_line_begin));
	}
}
