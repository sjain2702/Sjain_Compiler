/*
 * idtable.cpp
 *
 * Written:            1 June 2020
 * Last modified:      1 June 2023
 *      Author: Michael Oudshoorn
 *
 * ---------------------------------------------------------------------
 * Modified for the Semantic Analysis phase
 * Student : Shlok Jain
 * Course  : CSC-4510
 *
 * Description of modifications:
 *   Implements the symbol table as an array of BSTs (one BST per scope
 *   level), with insertion, outward-searching lookup, scope entry/exit,
 *   pre-declaration of the library functions, and the required symbol-table
 *   printout performed every time a scope is exited.
 * ---------------------------------------------------------------------
 */

#include <iostream>
#include <iomanip>
#include "id_table.h"

using namespace std;

id_table::id_table(error_handler* err) : error(err) { }

// Release every BST that is still allocated (normally none remain).
id_table::~id_table() {
	for (int i = 0; i < MAX_SCOPES; ++i) { free_tree(roots[i]); roots[i] = nullptr; }
}

// ---------------------------------------------------------------------------
// Scope management
// ---------------------------------------------------------------------------

// Activate a new, innermost scope level.  Its BST starts out empty.
void id_table::enter_scope() {
	if (current_scope + 1 >= MAX_SCOPES) throw lille_exception("Maximum scope depth exceeded.");
	++current_scope;
	free_tree(roots[current_scope]);   // defensive: guarantee a fresh, empty tree
	roots[current_scope] = nullptr;
}

// Leave the innermost scope.  The spec requires the table to be printed
// BEFORE the identifiers of the scope are removed, so we print first (all
// active levels 0..current_scope, including the one being exited), then
// free the BST for that level and decrement current_scope.
void id_table::exit_scope() {
	if (current_scope < 0) return;
	print_active_scopes("EXITING SCOPE LEVEL " + to_string(current_scope));
	free_tree(roots[current_scope]);
	roots[current_scope] = nullptr;
	--current_scope;
}

int id_table::scope_level() const { return current_scope; }

// ---------------------------------------------------------------------------
// BST primitives
// ---------------------------------------------------------------------------

// Iterative BST search inside ONE scope's tree.
id_table::entry* id_table::find_in_scope(entry* root, const string& name) const {
	while (root) {
		if (name == root->name) return root;
		root = name < root->name ? root->left : root->right;
	}
	return nullptr;
}

// Lookup: start at the current (innermost) scope and move outward through the
// enclosing scopes; the first match is the visible declaration, so inner
// declarations hide outer ones of the same name.
id_table::entry* id_table::find_visible(const string& name) const {
	for (int scope = current_scope; scope >= 0; --scope)
		if (entry* found = find_in_scope(roots[scope], name)) return found;
	return nullptr;
}

// Insert into the BST of the CURRENT scope only.  Returns false (and inserts
// nothing) if the name already exists in this scope, which the parser reports
// as "declared multiple times".  Redeclaring a name from an OUTER scope is legal.
bool id_table::insert(const string& name, lille_type type, lille_kind kind, int line, int position, const string& value) {
	if (current_scope < 0) enter_scope();
	entry** node = &roots[current_scope];
	while (*node) {
		if (name == (*node)->name) return false;
		node = name < (*node)->name ? &(*node)->left : &(*node)->right;
	}
	*node = new entry(name, type, kind, current_scope, line, position, value);
	return true;
}

// Post-order deletion of a whole BST.
void id_table::free_tree(entry* root) {
	if (!root) return;
	free_tree(root->left);
	free_tree(root->right);
	delete root;
}

// ---------------------------------------------------------------------------
// Queries / attribute updates (all use the outward-searching lookup)
// ---------------------------------------------------------------------------

bool id_table::contains(const string& name) const { return find_visible(name) != nullptr; }

bool id_table::lookup(const string& name, lille_type& type, lille_kind& kind) const {
	entry* found = find_visible(name);
	if (!found) return false;
	type = found->type;
	kind = found->kind;
	return true;
}

void id_table::set_signature(const string& name, const vector<lille_type::lille_ty>& types, const vector<lille_kind::lille_knd>& modes) {
	if (entry* found = find_visible(name)) { found->parameter_types = types; found->parameter_modes = modes; }
}

bool id_table::signature(const string& name, vector<lille_type::lille_ty>& types, vector<lille_kind::lille_knd>& modes) const {
	entry* found = find_visible(name);
	if (!found) return false;
	types = found->parameter_types;
	modes = found->parameter_modes;
	return true;
}

void id_table::set_value(const string& name, const string& value) {
	if (entry* found = find_visible(name)) found->value = value;
}

void id_table::set_return_type(const string& name, lille_type::lille_ty type) {
	if (entry* found = find_visible(name)) found->return_type = type;
}

lille_type::lille_ty id_table::function_return_type(const string& name) const {
	if (entry* found = find_visible(name)) return found->return_type;
	return lille_type::type_unknown;
}

// Pre-declare the Lille library functions.  Identifiers are upper-cased by the
// scanner, so they are stored in upper case.  All take one VALUE parameter.
// To add another library routine, add one line to the table below.
void id_table::declare_builtins() {
	struct builtin { const char* name; lille_type::lille_ty param; lille_type::lille_ty result; };
	static const builtin library[] = {
		{ "INT2STRING",  lille_type::type_integer, lille_type::type_string  },
		{ "REAL2STRING", lille_type::type_real,    lille_type::type_string  },
		{ "STRING2INT",  lille_type::type_string,  lille_type::type_integer },
		{ "STRING2REAL", lille_type::type_string,  lille_type::type_real    },
	};
	for (const builtin& b : library) {
		if (!insert(b.name, lille_type(lille_type::type_func), lille_kind(lille_kind::unknown), 0, 0)) continue;
		set_signature(b.name, { b.param }, { lille_kind::value_param });
		set_return_type(b.name, b.result);
		find_in_scope(roots[current_scope], b.name)->predefined = true;
	}
}

// ---------------------------------------------------------------------------
// Printing
// ---------------------------------------------------------------------------

static string mode_name(lille_kind::lille_knd m) { return m == lille_kind::ref_param ? "REF" : "VALUE"; }

// Print one identifier as a table row: name, type, kind, scope level, line,
// position, then value / parameter / return information where relevant.
void id_table::print_entry(const entry* e) const {
	entry* m = const_cast<entry*>(e);   // lille_type/lille_kind accessors are non-const
	string line = e->predefined ? "-" : to_string(e->line);
	string pos  = e->predefined ? "-" : to_string(e->position);
	cout << "  " << left << setw(14) << e->name << setw(10) << m->type.to_string()
		 << setw(13) << m->kind.to_string() << setw(7) << e->scope
		 << setw(6) << line << setw(6) << pos;
	string details;
	if (e->predefined) details += "predefined; ";
	if (!e->value.empty()) details += "value=" + e->value + (m->type.is_type(lille_type::type_proc) || m->type.is_type(lille_type::type_func) ? "; " : "");
	if (m->type.is_type(lille_type::type_proc) || m->type.is_type(lille_type::type_func)) {
		details += "params=" + to_string(e->parameter_types.size());
		if (!e->parameter_types.empty()) {
			details += " (";
			for (size_t i = 0; i < e->parameter_types.size(); ++i) {
				if (i) details += ", ";
				details += mode_name(e->parameter_modes[i]) + " " + lille_type(e->parameter_types[i]).to_string();
			}
			details += ")";
		}
		if (m->type.is_type(lille_type::type_func)) details += "; returns " + lille_type(e->return_type).to_string();
	}
	cout << details << endl;
}

// INORDER traversal (left subtree, node, right subtree) -> names print in
// alphabetical order.
void id_table::print_tree(entry* root) const {
	if (!root) return;
	print_tree(root->left);
	print_entry(root);
	print_tree(root->right);
}

// Print every active scope level (0 .. current_scope), outermost first.
void id_table::print_active_scopes(const string& banner) {
	cout << endl << "=====================================================================" << endl;
	cout << " SYMBOL TABLE  --  " << banner << endl;
	cout << "=====================================================================" << endl;
	for (int level = 0; level <= current_scope; ++level) {
		cout << "Scope level " << level << (level == current_scope ? "   <-- scope being exited" : "") << endl;
		cout << "  " << left << setw(14) << "NAME" << setw(10) << "TYPE" << setw(13) << "KIND"
			 << setw(7) << "SCOPE" << setw(6) << "LINE" << setw(6) << "POS" << "DETAILS" << endl;
		if (!roots[level]) cout << "  (no identifiers)" << endl;
		else print_tree(roots[level]);
		cout << endl;
	}
}

// Public dump used for ad-hoc debugging.
void id_table::dump_id_table(bool dump_all) {
	if (!dump_all) {
		cout << "Dump of idtable for current scope only." << endl;
		if (current_scope >= 0) print_tree(roots[current_scope]);
	} else {
		print_active_scopes("FULL DUMP");
	}
}
