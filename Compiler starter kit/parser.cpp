/*
 * parser.cpp - LL(1) recursive-descent parser following the course Lille
 * syntax handout.  The expression functions mirror its precedence levels.
 *
 * Student : Shlok Jain
 * Course  : CSC-4510
 *
 * Description of modifications (Semantic Analysis phase):
 *   Static semantic analysis is integrated into the parser.
 *     - Declarations are entered into the id_table (array of BSTs); every
 *       block, routine body and FOR loop opens a scope and closes it again,
 *       which triggers the symbol-table printout.
 *     - Identifier uses are resolved with id_table::lookup (innermost scope
 *       outward) -> "not declared" (#81).
 *     - Types are computed bottom-up through the expression functions and
 *       checked for assignments, operators, conditions, ranges, returns and
 *       procedure / function calls (parameter count, type and mode).
 *   Error cascades are suppressed: an expression whose type is unknown
 *   (because of an earlier error) is compatible with everything.
 */
#include "parser.h"

parser::parser(scanner* s, error_handler* e, id_table* table) : scan(s), errors(e), current(nullptr), ids(table) {
	advance();
}

/* ======================================================================
 *  Semantic helper routines
 * ====================================================================== */

// Report a semantic error at the given token (line/position come from the token).
void parser::semantic_error(token* tok, int code) { errors->flag(tok, code); }

// Type of a literal token.
lille_type::lille_ty parser::token_type(token* tok) const {
	switch (tok->get_sym()) {
	case symbol::integer: return lille_type::type_integer;
	case symbol::real_num: return lille_type::type_real;
	case symbol::strng: return lille_type::type_string;
	case symbol::true_sym: case symbol::false_sym: return lille_type::type_boolean;
	default: return lille_type::type_unknown;
	}
}

// Assignment compatibility: identical types, or an integer stored in a real.
// An unknown type (result of an earlier error) is compatible with anything so
// that one mistake does not trigger a chain of follow-on errors.
bool parser::compatible(lille_type::lille_ty lhs, lille_type::lille_ty rhs) const {
	if (lhs == lille_type::type_unknown || rhs == lille_type::type_unknown) return true;
	return lhs == rhs || (lhs == lille_type::type_real && rhs == lille_type::type_integer);
}

bool parser::is_numeric(lille_type::lille_ty t) const {
	return t == lille_type::type_integer || t == lille_type::type_real;
}

// Result type of an arithmetic operator.  Both operands must be numeric (#116);
// integer op real yields real.
lille_type::lille_ty parser::arithmetic_result(token* op, lille_type::lille_ty l, lille_type::lille_ty r) {
	if (l == lille_type::type_unknown || r == lille_type::type_unknown) return lille_type::type_unknown;
	if (!is_numeric(l) || !is_numeric(r)) { semantic_error(op, 116); return lille_type::type_unknown; }
	return (l == lille_type::type_real || r == lille_type::type_real) ? lille_type::type_real : lille_type::type_integer;
}

// The expression just parsed must be BOOLEAN (conditions of if/elsif/while/exit when).
// 'where' is the first token of that expression so the error points at it.
void parser::require_boolean(token* where) {
	if (last_expr_type != lille_type::type_boolean && last_expr_type != lille_type::type_unknown)
		semantic_error(where, 103);
}

// Compare the actual parameters of a call with the callee's declared signature:
// count (#97), type (#98) and kind - a REF formal needs a variable actual (#99).
void parser::check_call(token* name, const actual_params& actuals) {
	vector<lille_type::lille_ty> expected; vector<lille_kind::lille_knd> modes;
	ids->signature(name->get_identifier_value(), expected, modes);
	if (expected.size() != actuals.types.size()) { semantic_error(name, 97); return; }
	for (size_t i = 0; i < expected.size(); ++i) {
		if (!compatible(expected[i], actuals.types[i])) semantic_error(name, 98);
		if (modes[i] == lille_kind::ref_param) {
			// A reference parameter must receive exactly the same type and an assignable variable.
			if (expected[i] != actuals.types[i] && actuals.types[i] != lille_type::type_unknown) semantic_error(name, 98);
			if (!actuals.lvalues[i]) semantic_error(name, 99);
		}
	}
}

/* ======================================================================
 *  Scanner interface helpers
 * ====================================================================== */

bool parser::have(symbol::symbol_type s) const { return scan->have(s); }

void parser::advance() { current = scan->get_token(); }

[[noreturn]] void parser::syntax_error(const string& expected) {
	string message = "Syntax error: " + expected + "; found " + current->get_symbol()->symtostr();
	errors->flag(current, 124, message);
	// The error handler has printed and recorded the detailed diagnostic; use
	// an empty exception message so the top-level handler does not print it twice.
	throw lille_exception("");
}

void parser::expect(symbol::symbol_type s) {
	if (!scan->have(s)) syntax_error("expected " + symbol(s).symtostr());
	scan->must_be(s);
	current = scan->this_token();
}

bool parser::is_type_start() const {
	return have(symbol::integer_sym) || have(symbol::real_sym) || have(symbol::string_sym) || have(symbol::boolean_sym);
}

bool parser::is_statement_start() const {
	return have(symbol::identifier) || have(symbol::exit_sym) || have(symbol::return_sym) ||
		have(symbol::read_sym) || have(symbol::write_sym) || have(symbol::writeln_sym) ||
		have(symbol::null_sym) || have(symbol::if_sym) || have(symbol::loop_sym) ||
		have(symbol::for_sym) || have(symbol::while_sym);
}

/* ======================================================================
 *  Program and blocks
 * ====================================================================== */

// program ::= PROGRAM id IS block ;
// Scope level 0 holds the predefined library functions and the program name;
// the program's own declarations live in scope level 1 (opened by parse_block).
void parser::parse_program() {
	ids->enter_scope();                 // scope level 0
	ids->declare_builtins();
	expect(symbol::program_sym);
	token* program_name = current;
	expect(symbol::identifier);
	ids->insert(program_name->get_identifier_value(), lille_type(lille_type::type_prog), lille_kind(lille_kind::unknown), program_name->get_line_number(), program_name->get_pos_on_line());
	expect(symbol::is_sym);
	parse_block(program_name);
	expect(symbol::semicolon_sym);
	expect(symbol::end_of_program);
	ids->exit_scope();                  // leave scope level 0 (final table printout)
}

// block ::= { declaration } BEGIN statement-list END [ id ]
// A new scope level is opened for the block; the routine's formal parameters
// (collected by parse_parameter) are inserted first, so they are local to it.
void parser::parse_block(token* block_name) {
	ids->enter_scope();
	for (const auto& p : pending_parameters)
		if (!ids->insert(p.name, lille_type(p.type), lille_kind(p.mode), p.tok->get_line_number(), p.tok->get_pos_on_line()))
			semantic_error(p.tok, 82);                    // duplicate parameter name
	pending_parameters.clear();
	// FIRST(declaration) = { identifier, procedure, function }; BEGIN is the
	// FOLLOW of declarations and therefore selects the end of this repetition.
	while (have(symbol::identifier) || have(symbol::procedure_sym) || have(symbol::function_sym))
		parse_declaration();
	expect(symbol::begin_sym);
	parse_statement_list();
	expect(symbol::end_sym);
	if (have(symbol::identifier)) {   // optional closing name must equal the block's name (#107)
		if (current->get_identifier_value() != block_name->get_identifier_value()) semantic_error(current, 107);
		advance();
	}
	ids->exit_scope();                // prints the symbol table, then drops this scope
}

/* ======================================================================
 *  Declarations
 * ====================================================================== */

void parser::parse_declaration() {
	if (have(symbol::procedure_sym) || have(symbol::function_sym)) {
		bool function = have(symbol::function_sym);
		advance();
		token* routine = current;
		expect(symbol::identifier);
		lille_type::lille_ty result = function ? lille_type::type_unknown : lille_type::type_proc;
		// The routine's name is declared in the ENCLOSING scope (before its
		// parameters are read, so the routine may call itself recursively).
		bool inserted = ids->insert(routine->get_identifier_value(), lille_type(function ? lille_type::type_func : lille_type::type_proc), lille_kind(lille_kind::unknown), routine->get_line_number(), routine->get_pos_on_line());
		if (!inserted) semantic_error(routine, 82);
		formal_types.clear(); formal_modes.clear(); pending_parameters.clear();
		parsing_function_params = function;
		if (have(symbol::left_paren_sym)) {
			advance();
			parse_parameter_list();
			expect(symbol::right_paren_sym);
		}
		parsing_function_params = false;
		if (function) {
			expect(symbol::return_sym); parse_type(); result = declared_type;
			if (inserted) ids->set_return_type(routine->get_identifier_value(), result);
		}
		if (inserted) ids->set_signature(routine->get_identifier_value(), formal_types, formal_modes);
		expect(symbol::is_sym);
		return_types.push_back(function ? result : lille_type::type_unknown);
		function_has_return.push_back(false);
		parse_block(routine);
		bool has_return = function_has_return.back();
		function_has_return.pop_back();
		return_types.pop_back();
		if (function && !has_return) semantic_error(routine, 109);   // a function needs a RETURN
		expect(symbol::semicolon_sym);
		return;
	}
	// Variable / constant declaration:  id-list : [CONSTANT] type [:= literal] ;
	bool is_constant = false;
	parse_identifier_list();
	expect(symbol::colon_sym);
	if (have(symbol::constant_sym)) { is_constant = true; advance(); }
	parse_type();
	for (token* name : declared_tokens) {
		if (!ids->insert(name->get_identifier_value(), lille_type(declared_type), lille_kind(is_constant ? lille_kind::constant : lille_kind::variable), name->get_line_number(), name->get_pos_on_line())) semantic_error(name, 82);
	}
	if (have(symbol::becomes_sym)) {
		advance();
		string sign;
		token* sign_token = current;
		if (have(symbol::plus_sym) || have(symbol::minus_sym)) { sign = have(symbol::minus_sym) ? "-" : ""; advance(); }
		token* literal = current;
		if (have(symbol::integer) || have(symbol::real_num) || have(symbol::strng) || have(symbol::true_sym) || have(symbol::false_sym)) {
			if (!sign.empty() || sign_token != literal) {
				// a sign is only meaningful on a numeric literal
				if (token_type(literal) != lille_type::type_integer && token_type(literal) != lille_type::type_real) semantic_error(literal, 111);
			}
			if (!compatible(declared_type, token_type(literal))) semantic_error(literal, 111);   // initial value must match declared type
			string value;
			if (literal->get_sym() == symbol::integer) value = to_string(literal->get_integer_value());
			else if (literal->get_sym() == symbol::real_num) value = to_string(literal->get_real_value());
			else if (literal->get_sym() == symbol::strng) value = literal->get_string_value();
			else value = literal->get_sym() == symbol::true_sym ? "TRUE" : "FALSE";
			value = sign + value;
			for (token* name : declared_tokens) ids->set_value(name->get_identifier_value(), value);
			advance();
		}
		else syntax_error("expected a number, string, or boolean constant");
	}
	else if (is_constant) semantic_error(current, 110);       // a constant must be given a value
	expect(symbol::semicolon_sym);
}

void parser::parse_identifier_list() {
	declared_tokens.clear();
	if (!have(symbol::identifier)) syntax_error("expected identifier");
	declared_tokens.push_back(current);
	advance();
	while (have(symbol::comma_sym)) { advance(); if (!have(symbol::identifier)) syntax_error("expected identifier"); declared_tokens.push_back(current); advance(); }
}

void parser::parse_type() {
	if (!is_type_start()) syntax_error("expected a Lille type");
	switch (current->get_sym()) {
	case symbol::integer_sym: declared_type = lille_type::type_integer; break;
	case symbol::real_sym: declared_type = lille_type::type_real; break;
	case symbol::string_sym: declared_type = lille_type::type_string; break;
	case symbol::boolean_sym: declared_type = lille_type::type_boolean; break;
	default: declared_type = lille_type::type_unknown;
	}
	advance();
}

void parser::parse_parameter_list() {
	parse_parameter();
	while (have(symbol::semicolon_sym)) { advance(); parse_parameter(); }
}

// parameter ::= id-list : (VALUE | REF) type
// Records each formal's type and mode for the routine's signature and queues it
// for insertion into the routine's scope.  Functions may only have VALUE parameters (#123).
void parser::parse_parameter() {
	parse_identifier_list();
	expect(symbol::colon_sym);
	if (!have(symbol::value_sym) && !have(symbol::ref_sym)) syntax_error("expected value or ref parameter mode");
	lille_kind::lille_knd mode = have(symbol::ref_sym) ? lille_kind::ref_param : lille_kind::value_param;
	token* mode_token = current;
	if (parsing_function_params && mode == lille_kind::ref_param) semantic_error(mode_token, 123);
	advance();
	parse_type();
	for (token* name : declared_tokens) {
		formal_types.push_back(declared_type); formal_modes.push_back(mode);
		pending_parameters.push_back({name->get_identifier_value(), declared_type, mode, name});
	}
}

/* ======================================================================
 *  Statements
 * ====================================================================== */

void parser::parse_statement_list() {
	// Statement-list FIRST is the statement-start set. END, ELSIF, and ELSE
	// are FOLLOW tokens in their enclosing productions and stop the list.
	if (!is_statement_start()) syntax_error("expected a statement");
	parse_statement();
	expect(symbol::semicolon_sym);
	while (is_statement_start()) {
		parse_statement();
		expect(symbol::semicolon_sym);
	}
}

void parser::parse_statement() {
	if (have(symbol::if_sym)) parse_if_statement();
	else if (have(symbol::loop_sym)) parse_loop_statement();
	else if (have(symbol::for_sym)) parse_for_statement();
	else if (have(symbol::while_sym)) parse_while_statement();
	else parse_simple_statement();
}

void parser::parse_simple_statement() {
	if (have(symbol::identifier)) {
		// Either an assignment  id := expression   or a procedure call  id [ ( actuals ) ].
		token* lhs = current;
		lille_type lhs_type;
		lille_kind lhs_kind;
		bool declared = ids->lookup(lhs->get_identifier_value(), lhs_type, lhs_kind);
		if (!declared) semantic_error(lhs, 81);                       // identifier not declared
		advance();
		if (have(symbol::becomes_sym)) {
			// Only variables and reference parameters may be assigned (#85).
			if (declared && !lhs_kind.is_kind(lille_kind::variable) && !lhs_kind.is_kind(lille_kind::ref_param)) semantic_error(lhs, 85);
			advance(); parse_expression();
			if (declared && !compatible(lhs_type.get_type(), last_expr_type)) semantic_error(lhs, 93);  // LHS/RHS types
		}
		else if (have(symbol::left_paren_sym)) {
			actual_params actuals = parse_actual_parameters();
			if (declared) {
				if (lhs_type.get_type() != lille_type::type_proc) semantic_error(lhs, 90);   // must name a procedure
				else check_call(lhs, actuals);
			}
		}
		else if (declared) {
			// Procedure call without an argument list: the procedure must take no parameters.
			if (lhs_type.get_type() != lille_type::type_proc) semantic_error(lhs, 90);
			else check_call(lhs, actual_params{});
		}
		return;
	}
	if (have(symbol::exit_sym)) {
		token* exit_token = current; advance();
		if (loop_depth == 0) semantic_error(exit_token, 89);          // EXIT only inside a loop
		if (have(symbol::when_sym)) { advance(); token* start = current; parse_expression(); require_boolean(start); }
	} else if (have(symbol::return_sym)) {
		token* ret = current;
		advance();
		if (is_expression_start()) {
			parse_expression();
			if (!function_has_return.empty()) function_has_return.back() = true;
			if (return_types.empty() || return_types.back() == lille_type::type_unknown) semantic_error(ret, 88);   // procedures/main cannot return a value
			else if (!compatible(return_types.back(), last_expr_type)) semantic_error(ret, 87);   // value must match the function's result type
		} else if (return_types.empty()) semantic_error(ret, 88);
		else if (return_types.back() != lille_type::type_unknown) semantic_error(ret, 87);       // function must return a value
	} else if (have(symbol::read_sym)) {
		// Every item read must be a declared, assignable variable.
		advance(); bool paren = have(symbol::left_paren_sym); if (paren) advance();
		for (;;) {
			token* target = current;
			expect(symbol::identifier);
			lille_type t; lille_kind k;
			if (!ids->lookup(target->get_identifier_value(), t, k)) semantic_error(target, 81);
			else if (!k.is_kind(lille_kind::variable) && !k.is_kind(lille_kind::ref_param)) semantic_error(target, 85);
			if (!have(symbol::comma_sym)) break;
			advance();
		}
		if (paren) expect(symbol::right_paren_sym);
	} else if (have(symbol::write_sym) || have(symbol::writeln_sym)) {
		bool writeln = have(symbol::writeln_sym); advance();
		bool paren = have(symbol::left_paren_sym); if (paren) advance();
		if (!writeln || (paren && !have(symbol::right_paren_sym)) || (!paren && is_expression_start())) {
			parse_expression();
			while (have(symbol::comma_sym)) { advance(); parse_expression(); }
		}
		if (paren) expect(symbol::right_paren_sym);
	} else if (have(symbol::null_sym)) advance();
	else syntax_error("expected a simple statement");
}

bool parser::is_expression_start() const {
	return have(symbol::identifier) || have(symbol::integer) || have(symbol::real_num) || have(symbol::strng) ||
		have(symbol::true_sym) || have(symbol::false_sym) || have(symbol::not_sym) || have(symbol::odd_sym) ||
		have(symbol::left_paren_sym) || have(symbol::plus_sym) || have(symbol::minus_sym);
}

// Parses ( expr {, expr} ) and returns the type and assignability of each actual.
// The result is local to this call so nested calls cannot overwrite it.
parser::actual_params parser::parse_actual_parameters() {
	actual_params actuals;
	expect(symbol::left_paren_sym);
	if (!have(symbol::right_paren_sym)) {
		parse_expression(); actuals.types.push_back(last_expr_type); actuals.lvalues.push_back(last_expr_lvalue);
		while (have(symbol::comma_sym)) { advance(); parse_expression(); actuals.types.push_back(last_expr_type); actuals.lvalues.push_back(last_expr_lvalue); }
	}
	expect(symbol::right_paren_sym);
	return actuals;
}

void parser::parse_if_statement() {
	token* start;
	expect(symbol::if_sym); start = current; parse_expression(); require_boolean(start); expect(symbol::then_sym); parse_statement_list();
	while (have(symbol::elsif_sym)) { advance(); start = current; parse_expression(); require_boolean(start); expect(symbol::then_sym); parse_statement_list(); }
	if (have(symbol::else_sym)) { advance(); parse_statement_list(); }
	expect(symbol::end_sym); expect(symbol::if_sym);
}

void parser::parse_loop_statement() {
	expect(symbol::loop_sym); ++loop_depth; parse_statement_list(); --loop_depth; expect(symbol::end_sym); expect(symbol::loop_sym);
}

// for id IN [REVERSE] range loop-statement
// The range is evaluated in the ENCLOSING scope; the loop variable (an INTEGER
// of kind FOR_IDENT, not assignable) lives in its own scope that exists only
// while the loop body is parsed.
void parser::parse_for_statement() {
	expect(symbol::for_sym); token* loop_name = current; expect(symbol::identifier);
	expect(symbol::in_sym);
	if (have(symbol::reverse_sym)) advance();
	parse_range();
	ids->enter_scope();
	if (!ids->insert(loop_name->get_identifier_value(), lille_type(lille_type::type_integer), lille_kind(lille_kind::for_ident), loop_name->get_line_number(), loop_name->get_pos_on_line())) semantic_error(loop_name, 82);
	parse_loop_statement();
	ids->exit_scope();
}

void parser::parse_while_statement() {
	expect(symbol::while_sym); token* start = current; parse_expression(); require_boolean(start); parse_loop_statement();
}

// range ::= simple-expression .. simple-expression   (both bounds INTEGER, #104)
void parser::parse_range() {
	token* start = current;
	parse_simple_expression();
	if (last_expr_type != lille_type::type_integer && last_expr_type != lille_type::type_unknown) semantic_error(start, 104);
	expect(symbol::range_sym);
	start = current;
	parse_simple_expression();
	if (last_expr_type != lille_type::type_integer && last_expr_type != lille_type::type_unknown) semantic_error(start, 104);
}

/* ======================================================================
 *  Expressions  (each routine leaves its result type in last_expr_type)
 * ====================================================================== */

bool parser::is_relop() const {
	return have(symbol::greater_than_sym) || have(symbol::less_than_sym) || have(symbol::equals_sym) ||
		have(symbol::not_equals_sym) || have(symbol::less_or_equal_sym) || have(symbol::greater_or_equal_sym);
}
bool parser::is_addop() const { return have(symbol::plus_sym) || have(symbol::minus_sym); }
bool parser::is_multop() const { return have(symbol::asterisk_sym) || have(symbol::slash_sym); }

// expression ::= simple-expression [ relop simple-expression | IN range ]   -> BOOLEAN when a relation is present
void parser::parse_expression() {
	parse_simple_expression();
	if (is_relop()) {
		auto lhs = last_expr_type; token* op = current; advance(); parse_simple_expression();
		// Operands must have matching types (an integer may be compared with a real).
		if (!compatible(lhs, last_expr_type) && !compatible(last_expr_type, lhs)) semantic_error(op, 114);
		last_expr_type = lille_type::type_boolean; last_expr_lvalue = false;
	}
	else if (have(symbol::in_sym)) {
		token* op = current;
		if (last_expr_type != lille_type::type_integer && last_expr_type != lille_type::type_unknown) semantic_error(op, 102);
		advance(); parse_range();
		last_expr_type = lille_type::type_boolean; last_expr_lvalue = false;
	}
}

// simple-expression ::= expr2 { & expr2 }   (string concatenation, both sides STRING)
void parser::parse_simple_expression() {
	parse_expr2();
	while (have(symbol::ampersand_sym)) {
		token* op = current; auto lhs = last_expr_type; advance(); parse_expr2();
		bool known = lhs != lille_type::type_unknown && last_expr_type != lille_type::type_unknown;
		if (known && (lhs != lille_type::type_string || last_expr_type != lille_type::type_string)) semantic_error(op, 115);
		last_expr_type = known ? lille_type::type_string : lille_type::type_unknown; last_expr_lvalue = false;
	}
}

// expr2 ::= term { (+ | - | OR) term }
void parser::parse_expr2() {
	parse_term();
	while (is_addop() || have(symbol::or_sym)) {
		token* op = current; auto lhs = last_expr_type; auto oper = current->get_sym(); advance(); parse_term();
		if (oper == symbol::or_sym) {
			bool known = lhs != lille_type::type_unknown && last_expr_type != lille_type::type_unknown;
			if (known && (lhs != lille_type::type_boolean || last_expr_type != lille_type::type_boolean)) semantic_error(op, 103);
			last_expr_type = known ? lille_type::type_boolean : lille_type::type_unknown;
		}
		else last_expr_type = arithmetic_result(op, lhs, last_expr_type);
		last_expr_lvalue = false;
	}
}

// term ::= factor { (* | / | AND) factor }
void parser::parse_term() {
	parse_factor();
	while (is_multop() || have(symbol::and_sym)) {
		token* op = current; auto lhs = last_expr_type; auto oper = current->get_sym(); advance(); parse_factor();
		if (oper == symbol::and_sym) {
			bool known = lhs != lille_type::type_unknown && last_expr_type != lille_type::type_unknown;
			if (known && (lhs != lille_type::type_boolean || last_expr_type != lille_type::type_boolean)) semantic_error(op, 103);
			last_expr_type = known ? lille_type::type_boolean : lille_type::type_unknown;
		}
		else last_expr_type = arithmetic_result(op, lhs, last_expr_type);
		last_expr_lvalue = false;
	}
}

// factor ::= (+|-) primary | primary [ ** primary ]
void parser::parse_factor() {
	// The factor alternatives are LL(1): unary addop starts only the signed
	// primary alternative; all other factor starts are in FIRST(primary).
	if (is_addop()) {
		token* op = current; advance(); parse_primary();
		if (!is_numeric(last_expr_type) && last_expr_type != lille_type::type_unknown) { semantic_error(op, 116); last_expr_type = lille_type::type_unknown; }
		last_expr_lvalue = false;
		return;
	}
	parse_primary();
	if (have(symbol::power_sym)) {
		token* op = current; auto lhs = last_expr_type; advance(); parse_primary();
		last_expr_type = arithmetic_result(op, lhs, last_expr_type); last_expr_lvalue = false;
	}
}

void parser::parse_primary() {
	if (have(symbol::not_sym) || have(symbol::odd_sym)) {
		// NOT needs a BOOLEAN operand, ODD needs an INTEGER operand; both yield BOOLEAN.
		token* unary = current; auto op = current->get_sym(); advance(); parse_expression();
		if (op == symbol::not_sym && last_expr_type != lille_type::type_boolean && last_expr_type != lille_type::type_unknown) semantic_error(unary, 103);
		else if (op == symbol::odd_sym && last_expr_type != lille_type::type_integer && last_expr_type != lille_type::type_unknown) semantic_error(unary, 102);
		last_expr_type = lille_type::type_boolean; last_expr_lvalue = false;
	}
	else if (have(symbol::left_paren_sym)) {
		advance(); parse_expression(); expect(symbol::right_paren_sym);
		last_expr_lvalue = false;
	}
	else if (have(symbol::identifier)) {
		token* name = current; lille_type typ; lille_kind kind;
		bool declared = ids->lookup(name->get_identifier_value(), typ, kind);
		if (!declared) semantic_error(name, 81);                      // identifier not declared
		advance();
		last_expr_type = lille_type::type_unknown; last_expr_lvalue = false;
		if (have(symbol::left_paren_sym)) {
			// identifier ( actuals )  : must be a function call.
			actual_params actuals = parse_actual_parameters();
			if (declared) {
				if (typ.get_type() != lille_type::type_func) semantic_error(name, 121);
				else { check_call(name, actuals); last_expr_type = ids->function_return_type(name->get_identifier_value()); }
			}
		}
		else if (declared) {
			if (typ.get_type() == lille_type::type_func) {
				// A function without an argument list is a call with no arguments: only legal if it has no parameters.
				vector<lille_type::lille_ty> expected; vector<lille_kind::lille_knd> modes;
				ids->signature(name->get_identifier_value(), expected, modes);
				if (!expected.empty()) semantic_error(name, 121);
				else last_expr_type = ids->function_return_type(name->get_identifier_value());
			}
			else if (typ.get_type() == lille_type::type_proc || typ.get_type() == lille_type::type_prog) semantic_error(name, 91);   // not a value
			else {
				last_expr_type = typ.get_type();
				last_expr_lvalue = kind.is_kind(lille_kind::variable) || kind.is_kind(lille_kind::ref_param);
			}
		}
	}
	else if (have(symbol::integer) || have(symbol::real_num) || have(symbol::strng) || have(symbol::true_sym) || have(symbol::false_sym)) { last_expr_type = token_type(current); last_expr_lvalue = false; advance(); }
	else syntax_error("expected an expression primary");
}
