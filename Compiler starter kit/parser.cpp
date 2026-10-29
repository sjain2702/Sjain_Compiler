/*
 * parser.cpp - LL(1) recursive-descent parser following the course Lille
 * syntax handout.  The expression functions mirror its precedence levels.
 */
#include "parser.h"

parser::parser(scanner* s, error_handler* e) : scan(s), errors(e), current(nullptr) {
	advance();
}

bool parser::have(symbol::symbol_type s) const { return scan->have(s); }

void parser::advance() { current = scan->get_token(); }

[[noreturn]] void parser::syntax_error(const string& expected) {
	string message = "Syntax error: " + expected + "; found " + current->get_symbol()->symtostr() + ".";
	errors->flag(current, 124);
	throw lille_exception(message);
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

void parser::parse_program() {
	expect(symbol::program_sym);
	expect(symbol::identifier);
	expect(symbol::is_sym);
	parse_block();
	expect(symbol::semicolon_sym);
	expect(symbol::end_of_program);
}

void parser::parse_block() {
	// FIRST(declaration) = { identifier, procedure, function }; BEGIN is the
	// FOLLOW of declarations and therefore selects the end of this repetition.
	while (have(symbol::identifier) || have(symbol::procedure_sym) || have(symbol::function_sym))
		parse_declaration();
	expect(symbol::begin_sym);
	parse_statement_list();
	expect(symbol::end_sym);
	if (have(symbol::identifier)) advance(); // Optional block name; name matching is semantic.
}

void parser::parse_declaration() {
	if (have(symbol::procedure_sym) || have(symbol::function_sym)) {
		bool function = have(symbol::function_sym);
		advance();
		expect(symbol::identifier);
		if (have(symbol::left_paren_sym)) {
			advance();
			parse_parameter_list();
			expect(symbol::right_paren_sym);
		}
		if (function) { expect(symbol::return_sym); parse_type(); }
		expect(symbol::is_sym);
		parse_block();
		expect(symbol::semicolon_sym);
		return;
	}
	parse_identifier_list();
	expect(symbol::colon_sym);
	if (have(symbol::constant_sym)) advance();
	parse_type();
	if (have(symbol::becomes_sym)) {
		advance();
		if (have(symbol::integer) || have(symbol::real_num)) advance();
		else if (have(symbol::strng) || have(symbol::true_sym) || have(symbol::false_sym)) advance();
		else syntax_error("expected a number, string, or boolean constant");
	}
	expect(symbol::semicolon_sym);
}

void parser::parse_identifier_list() {
	expect(symbol::identifier);
	while (have(symbol::comma_sym)) { advance(); expect(symbol::identifier); }
}

void parser::parse_type() {
	if (!is_type_start()) syntax_error("expected a Lille type");
	advance();
}

void parser::parse_parameter_list() {
	parse_parameter();
	while (have(symbol::semicolon_sym)) { advance(); parse_parameter(); }
}

void parser::parse_parameter() {
	parse_identifier_list();
	expect(symbol::colon_sym);
	if (!have(symbol::value_sym) && !have(symbol::ref_sym)) syntax_error("expected value or ref parameter mode");
	advance();
	parse_type();
}

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
		advance();
		if (have(symbol::becomes_sym)) { advance(); parse_expression(); }
		else if (have(symbol::left_paren_sym)) parse_actual_parameters();
		return;
	}
	if (have(symbol::exit_sym)) {
		advance();
		if (have(symbol::when_sym)) { advance(); parse_expression(); }
	} else if (have(symbol::return_sym)) {
		advance();
		if (is_expression_start()) parse_expression();
	} else if (have(symbol::read_sym)) {
		advance(); bool paren = have(symbol::left_paren_sym); if (paren) advance();
		expect(symbol::identifier);
		while (have(symbol::comma_sym)) { advance(); expect(symbol::identifier); }
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

void parser::parse_actual_parameters() {
	expect(symbol::left_paren_sym);
	parse_expression();
	while (have(symbol::comma_sym)) { advance(); parse_expression(); }
	expect(symbol::right_paren_sym);
}

void parser::parse_if_statement() {
	expect(symbol::if_sym); parse_expression(); expect(symbol::then_sym); parse_statement_list();
	while (have(symbol::elsif_sym)) { advance(); parse_expression(); expect(symbol::then_sym); parse_statement_list(); }
	if (have(symbol::else_sym)) { advance(); parse_statement_list(); }
	expect(symbol::end_sym); expect(symbol::if_sym);
}

void parser::parse_loop_statement() {
	expect(symbol::loop_sym); parse_statement_list(); expect(symbol::end_sym); expect(symbol::loop_sym);
}

void parser::parse_for_statement() {
	expect(symbol::for_sym); expect(symbol::identifier); expect(symbol::in_sym);
	if (have(symbol::reverse_sym)) advance();
	parse_range(); parse_loop_statement();
}

void parser::parse_while_statement() {
	expect(symbol::while_sym); parse_expression(); parse_loop_statement();
}

void parser::parse_range() {
	parse_simple_expression(); expect(symbol::range_sym); parse_simple_expression();
}

bool parser::is_relop() const {
	return have(symbol::greater_than_sym) || have(symbol::less_than_sym) || have(symbol::equals_sym) ||
		have(symbol::not_equals_sym) || have(symbol::less_or_equal_sym) || have(symbol::greater_or_equal_sym);
}
bool parser::is_addop() const { return have(symbol::plus_sym) || have(symbol::minus_sym); }
bool parser::is_multop() const { return have(symbol::asterisk_sym) || have(symbol::slash_sym); }

void parser::parse_expression() {
	parse_simple_expression();
	if (is_relop()) { advance(); parse_simple_expression(); }
	else if (have(symbol::in_sym)) { advance(); parse_range(); }
}

void parser::parse_simple_expression() {
	parse_expr2(); while (have(symbol::ampersand_sym)) { advance(); parse_expr2(); }
}

void parser::parse_expr2() {
	parse_term();
	while (is_addop() || have(symbol::or_sym)) { advance(); parse_term(); }
}

void parser::parse_term() {
	parse_factor();
	while (is_multop() || have(symbol::and_sym)) { advance(); parse_factor(); }
}

void parser::parse_factor() {
	// The factor alternatives are LL(1): unary addop starts only the signed
	// primary alternative; all other factor starts are in FIRST(primary).
	if (is_addop()) { advance(); parse_primary(); return; }
	parse_primary();
	if (have(symbol::power_sym)) { advance(); parse_primary(); }
}

void parser::parse_primary() {
	if (have(symbol::not_sym) || have(symbol::odd_sym)) { advance(); parse_expression(); }
	else if (have(symbol::left_paren_sym)) { advance(); parse_simple_expression(); expect(symbol::right_paren_sym); }
	else if (have(symbol::identifier)) { advance(); if (have(symbol::left_paren_sym)) parse_actual_parameters(); }
	else if (have(symbol::integer) || have(symbol::real_num) || have(symbol::strng) || have(symbol::true_sym) || have(symbol::false_sym)) advance();
	else syntax_error("expected an expression primary");
}
