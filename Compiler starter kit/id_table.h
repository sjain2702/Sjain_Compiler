/*
 * idtable.h
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
 *   The identifier table is implemented as an ARRAY OF BINARY SEARCH
 *   TREES.  roots[k] is the root of the BST holding every identifier
 *   declared at scope level k; the array index IS the scope level, and
 *   current_scope is the index of the innermost active scope.
 *
 *   Operations:
 *     enter_scope()   - activate the next scope level with an empty BST.
 *     exit_scope()    - print the whole table (all active levels, including
 *                       the one being left), THEN free that level's BST and
 *                       deactivate the level.
 *     insert()        - BST insert into the CURRENT scope only (fails if the
 *                       name is already declared in this scope).
 *     lookup()        - search the current scope's BST, then each enclosing
 *                       scope outward (innermost declaration wins).
 *   Every entry stores name, type, kind, scope level, line, position, the
 *   constant/initial value, and for routines the parameter types/modes and
 *   the return type.
 * ---------------------------------------------------------------------
 */

#ifndef ID_TABLE_H_
#define ID_TABLE_H_

#include <iostream>
#include <string>
#include <vector>

#include "token.h"
#include "error_handler.h"
#include "lille_type.h"
#include "lille_kind.h"

using namespace std;

class id_table {
private:
	// One node of a scope's binary search tree (ordered by identifier name).
	struct entry {
		string name; lille_type type; lille_kind kind; int scope, line, position;
		string value;                     // constant / initial value, if any
		entry* left = nullptr; entry* right = nullptr;
		vector<lille_type::lille_ty> parameter_types;   // routines: formal types in order
		vector<lille_kind::lille_knd> parameter_modes;  // routines: VALUE_PARAM / REF_PARAM
		lille_type::lille_ty return_type = lille_type::type_unknown; // functions only
		bool predefined = false;          // true for built-in library routines
		entry(const string& n, lille_type t, lille_kind k, int s, int l, int p, const string& v)
			: name(n), type(t), kind(k), scope(s), line(l), position(p), value(v) {}
	};

	static constexpr int MAX_SCOPES = 128;
	entry* roots[MAX_SCOPES]{};      // roots[level] = BST of identifiers declared at that level
	int current_scope = -1;          // innermost active scope level (-1 = none yet)
	error_handler* error;

	entry* find_in_scope(entry* root, const string& name) const;   // BST search in ONE scope
	entry* find_visible(const string& name) const;                 // search current scope outward
	void free_tree(entry* root);                                   // post-order delete of a BST
	void print_tree(entry* root) const;                            // INORDER traversal + print
	void print_entry(const entry* e) const;                        // one table row
	void print_active_scopes(const string& banner);                // whole table, levels 0..current

public:
	id_table(error_handler* err);
	~id_table();

	void dump_id_table(bool dump_all = true);   // dump_all: every active level; false: current level only
	void enter_scope();
	void exit_scope();                          // prints the table, then removes the scope
	int scope_level() const;
	bool insert(const string& name, lille_type type, lille_kind kind, int line, int position, const string& value = "");
	bool contains(const string& name) const;
	bool lookup(const string& name, lille_type& type, lille_kind& kind) const;
	void set_signature(const string& name, const vector<lille_type::lille_ty>& types, const vector<lille_kind::lille_knd>& modes);
	bool signature(const string& name, vector<lille_type::lille_ty>& types, vector<lille_kind::lille_knd>& modes) const;
	void set_value(const string& name, const string& value);
	void set_return_type(const string& name, lille_type::lille_ty type);
	lille_type::lille_ty function_return_type(const string& name) const;
	void declare_builtins();                    // pre-declare the Lille library functions in the current scope
};

#endif /* ID_TABLE_H_ */
