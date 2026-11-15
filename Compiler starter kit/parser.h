/*
 * parser.h - recursive-descent parser for the Lille grammar, extended with
 *            static semantic analysis driven by the symbol table.
 *
 * Student : Shlok Jain
 * Course  : CSC-4510
 *
 * Description of modifications (Semantic Analysis phase):
 *   Semantic checking is performed during parsing ("syntax-directed").  Each
 *   expression parse function leaves the type of the expression it parsed in
 *   last_expr_type (and whether it was an assignable variable in
 *   last_expr_lvalue).  Declarations are inserted into the id_table, and
 *   every identifier use is resolved with id_table::lookup.
 */
#ifndef PARSER_H_
#define PARSER_H_

#include "scanner.h"
#include "id_table.h"
#include <vector>

class parser {
private:
	// One formal parameter waiting to be inserted into the routine's own scope.
	struct pending_param {
		std::string name;
		lille_type::lille_ty type;
		lille_kind::lille_knd mode;
		token* tok;                   // used for the line/position and for error reports
	};
	// Types / assignability of the actual parameters of ONE call.  Kept local
	// to each call so that nested calls such as f(g(x)) cannot clobber each other.
	struct actual_params {
		std::vector<lille_type::lille_ty> types;
		std::vector<bool> lvalues;
	};

	 scanner* scan;
	 error_handler* errors;
	 token* current;
	 id_table* ids;

	 // Declaration bookkeeping
	 std::vector<token*> declared_tokens;
	 lille_type::lille_ty declared_type = lille_type::type_unknown;
	 std::vector<pending_param> pending_parameters;
	 std::vector<lille_type::lille_ty> formal_types;
	 std::vector<lille_kind::lille_knd> formal_modes;
	 bool parsing_function_params = false;

	 // Expression bookkeeping
	 lille_type::lille_ty last_expr_type = lille_type::type_unknown;
	 bool last_expr_lvalue = false;

	 // Context for return / exit checks
	 std::vector<lille_type::lille_ty> return_types;   // innermost routine's result type (unknown = procedure)
	 std::vector<bool> function_has_return;
	 int loop_depth = 0;

	 // Semantic helpers
	 void semantic_error(token* tok, int code);
	 lille_type::lille_ty token_type(token* tok) const;
	 bool compatible(lille_type::lille_ty lhs, lille_type::lille_ty rhs) const;
	 bool is_numeric(lille_type::lille_ty t) const;
	 lille_type::lille_ty arithmetic_result(token* op, lille_type::lille_ty l, lille_type::lille_ty r);
	 void require_boolean(token* where);
	 void check_call(token* name, const actual_params& actuals);

	 // Scanner interface helpers
	 bool have(symbol::symbol_type s) const;
	 void advance();
	 void expect(symbol::symbol_type s);
	 [[noreturn]] void syntax_error(const string& expected);
	 bool is_type_start() const;
	 bool is_statement_start() const;
	 bool is_expression_start() const;

	 // Grammar productions
	 void parse_block(token* block_name);
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
	 actual_params parse_actual_parameters();
	 bool is_relop() const;
	 bool is_addop() const;
	 bool is_multop() const;

public:
	 parser(scanner* s, error_handler* e, id_table* table);
	 void parse_program();
};

#endif
