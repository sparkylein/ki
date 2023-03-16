
#include "../all.h"

UsageLine *usage_line_init(Allocator *alc, Scope *scope, Decl *decl) {
    //
    UsageLine *v = al(alc, sizeof(UsageLine));
    v->init_scope = scope;
    v->first_move = NULL;
    v->follow_up = NULL;
    v->moves_max = 0;
    v->moves_min = 0;
    v->reads_after_move = 0;

    if (scope->usage_keys == NULL) {
        scope->usage_keys = array_make(alc, 8);
        scope->usage_values = array_make(alc, 8);
    }

    int index = array_find(scope->usage_keys, decl, arr_find_adr);
    if (index > -1) {
        array_set_index(scope->usage_keys, index, decl);
        array_set_index(scope->usage_values, index, v);
    } else {
        array_push(scope->usage_keys, decl);
        array_push(scope->usage_values, v);
    }

    return v;
}

UsageLine *usage_line_get(Scope *scope, Decl *decl) {
    //
    while (scope) {
        if (scope->usage_keys) {
            int index = array_find(scope->usage_keys, decl, arr_find_adr);
            if (index > -1) {
                return array_get_index(scope->usage_values, index);
            }
        }
        scope = scope->parent;
    }

    printf("Usage line not found (compiler bug)\n");
    raise(11);
}

bool is_moved_once(UsageLine *ul) {
    //
    while (ul->follow_up) {
        ul = ul->follow_up;
    }
    // printf("%s\n", ul->init_scope->func->dname);
    // printf("%d,%d\n", ul->moves_min, ul->moves_max);
    return ul->moves_min == 1 && ul->moves_max == 1 && ul->reads_after_move == 0;
}

void usage_read_value(Allocator *alc, Scope *scope, Value *val) {
    //
}

Value *usage_move_value(Allocator *alc, Chunk *chunk, Scope *scope, Value *val) {
    //
    int vt = val->type;
    if (vt == v_var) {
        Var *var = val->item;
        Decl *decl = var->decl;
        UsageLine *ul = usage_line_get(scope, decl);

        // Check if in loop
        bool in_loop = false;
        Scope *s = scope;
        while (true) {
            if (s == decl->scope) {
                break;
            }
            if (s->type == sct_loop) {
                in_loop = true;
                break;
            }
            s = s->parent;
        }

        if (in_loop) {
            ul->moves_max += 2;
            ul->moves_min += 2;
        } else {
            ul->moves_max++;
            ul->moves_min++;
        }

        if (!ul->first_move) {
            ul->first_move = chunk_clone(alc, chunk);
        }

        Type *type = val->rett;
        Class *class = type->class;
        if (class && class->must_ref) {

            Scope *sub = scope_init(alc, sct_default, scope, true);
            Value *val = value_init(alc, v_decl, decl, type);
            class_ref_change(alc, sub, val, 1);

            if (sub->ast->length > 0) {
                array_push(scope->ast, tgen_exec_unless_moved_once(alc, sub, ul));
            }
        }
    } else {
        // TODO : upref class prop access that have a type class with must_ref = true
        if (vt == v_class_pa) {
            Class *class = val->rett->class;
            if (class && class->must_ref) {
            }
        }
    }

    //
    return val;
}

Scope *usage_scope_init(Allocator *alc, Scope *parent, int type) {
    //
    Scope *scope = scope_init(alc, type, parent, true);

    if (parent->usage_keys) {
        // Clone all usage lines from parent
        Array *keys = array_make(alc, 8);
        Array *vals = array_make(alc, 8);

        Array *par_keys = parent->usage_keys;
        Array *par_vals = parent->usage_values;
        for (int i = 0; i < par_keys->length; i++) {
            Decl *decl = array_get_index(par_keys, i);
            UsageLine *par_ul = array_get_index(par_vals, i);
            UsageLine *ul = al(alc, sizeof(UsageLine));
            *ul = *par_ul;

            array_push(keys, decl);
            array_push(vals, ul);
        }

        scope->usage_keys = keys;
        scope->usage_values = vals;
    }

    return scope;
}

void usage_collect_used_decls(Allocator *alc, Scope *left, Scope *right, Array **list_) {
    //
    if (!left->usage_keys) {
        return;
    }

    Array *list = *list_;

    Array *lkeys = left->usage_keys;
    Array *lvals = left->usage_values;
    Array *rkeys = right->usage_keys;
    Array *rvals = right->usage_values;
    int i = 0;
    while (i < lkeys->length) {
        Decl *decl = array_get_index(lkeys, i);
        UsageLine *l_ul = array_get_index(lvals, i);
        UsageLine *r_ul = array_get_index(rvals, i);

        bool left_ok = is_moved_once(l_ul);
        bool right_ok = is_moved_once(r_ul);

        if (r_ul->moves_max > l_ul->moves_max && right_ok) {
            if (!list) {
                list = array_make(alc, 20);
                *list_ = list;
            }
            array_push_unique(list, decl);
        }
        i++;
    }
}

void usage_merge_scopes(Allocator *alc, Scope *left, Scope *right, Array *used_decls) {
    //
    // 1. merge left lines with right && set follow up on right
    // 2. check if right scope has new lines, if so, merge them if they arent declare (look at init_scope)

    if (!left->usage_keys) {
        return;
    }

    Array *lkeys = left->usage_keys;
    Array *lvals = left->usage_values;
    Array *rkeys = right->usage_keys;
    Array *rvals = right->usage_values;
    int i = 0;
    while (i < lkeys->length) {
        Decl *decl = array_get_index(lkeys, i);
        UsageLine *l_ul = array_get_index(lvals, i);
        UsageLine *r_ul = array_get_index(rvals, i);

        bool right_ok = is_moved_once(r_ul);

        l_ul->moves_max = max_num(l_ul->moves_max, r_ul->moves_max);
        l_ul->moves_min = min_num(l_ul->moves_min, r_ul->moves_min);
        l_ul->reads_after_move = max_num(l_ul->reads_after_move, r_ul->reads_after_move);

        if (used_decls && array_contains(used_decls, decl, arr_find_adr)) {
            l_ul->moves_min = 1;

            if (!right_ok) {
                Type *type = decl->type;
                Class *class = type->class;
                if (class && class->must_deref) {
                    Scope *sub = scope_init(alc, sct_default, right, true);
                    Value *val = value_init(alc, v_decl, decl, decl->type);
                    class_ref_change(alc, sub, val, -1);
                    array_shift(right->ast, tgen_exec_if_moved_once(alc, sub, l_ul));
                }
            }
        }

        r_ul->follow_up = l_ul;
        i++;
    }
    while (i < rkeys->length) {
        Decl *decl = array_get_index(rkeys, i);
        UsageLine *r_ul = array_get_index(rvals, i);
        i++;

        if (decl->scope == right) {
            continue;
        }

        int index = array_find(lkeys, decl, arr_find_adr);
        if (index < 0) {
            printf("Var name: %s\n", decl->name);
            die("Missing left side usage line (compiler bug)\n");
        }
        UsageLine *l_ul = array_get_index(lvals, index);

        l_ul->moves_max = max_num(l_ul->moves_max, r_ul->moves_max);
        l_ul->moves_min = min_num(l_ul->moves_min, r_ul->moves_min);
        l_ul->reads_after_move = max_num(l_ul->reads_after_move, r_ul->reads_after_move);

        r_ul->follow_up = l_ul;
    }
}

void deref_scope(Allocator *alc, Scope *scope_, Scope *until) {
    Scope *scope = scope_;
    while (true) {
        Array *decls = scope->usage_keys;
        if (decls) {
            for (int i = 0; i < decls->length; i++) {

                Decl *decl = array_get_index(decls, i);
                UsageLine *ul = array_get_index(scope->usage_values, i);

                if (ul->init_scope != scope)
                    continue;

                Type *type = decl->type;
                Class *class = type->class;

                if (!class->must_deref) {
                    continue;
                }

                Scope *sub = scope_init(alc, sct_default, scope_, true);
                Value *val = value_init(alc, v_decl, decl, type);
                class_ref_change(alc, sub, val, -1);

                if (sub->ast->length > 0) {
                    array_push(scope_->ast, tgen_exec_unless_moved_once(alc, sub, ul));
                }
            }
        }

        if (scope == until)
            break;
        scope = scope->parent;
    }
}