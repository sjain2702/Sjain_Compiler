/*
 * parser.h - recursive-descent syntax checker for the Lille grammar.
 */
#ifndef PARSER_H_
#define PARSER_H_

#include "scanner.h"

class parser {
private:
	 scanner* scan;
	 error_handler* errors;
	 token* current;

	 bool have(symbol::symbol_type s) const;
	 void advance();
	 void expect(symbol::symbol_type s);
	 [[noreturn]] void syntax_error(const string& expected);
	 bool is_type_start() const;
	 bool is_statement_start() const;
	 bool is_expression_start() const;

	 void parse_block();
	 void parse_declaration();
	 void parse_identifier_list();
	 void parse_type();
	 void parse_parameter_list();
	 void parse_parameter();
	 void parse_statement_list();
	 void parse_statement();
	 void parse_simple_statement();
	 void parse_if_statement();
	 void parse_loop_statement();
	 void parse_for_statement();
	 void parse_while_statement();
	 void parse_range();
	 void parse_expression();
	 void parse_simple_expression();
	 void parse_expr2();
	 void parse_term();
	 void parse_factor();
	 void parse_primary();
	 void parse_actual_parameters();
	 bool is_relop() const;
	 bool is_addop() const;
	 bool is_multop() const;

public:
	 parser(scanner* s, error_handler* e);
	 void parse_program();
};

#endif
