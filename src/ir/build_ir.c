#include "build_ir.h"
#include "common/debug.h"
#include "common/utlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// struct IR_BUILDER_TEMP_OPERAND:
// typedef struct IR_BUILDER_TEMP_OPERAND IR_BUILDER_TEMP_OPERAND_t;
// struct IR_BUILDER_TEMP_OPERAND {
//     ir_operand_t* operand;
//     IR_BUILDER_TEMP_OPERAND_t* next;
// };

// struct IR_BUILDER:
// typedef struct IR_BUILDER IR_BUILDER_t;
// struct IR_BUILDER {
//     ir_opcode_t _op;
//     IR_BUILDER_TEMP_OPERAND_t* _opers;
//     IR_BUILDER_t* (*INST)(ir_opcode_t);
//     IR_BUILDER_t* (*VREG)(ir_register_t*);
//     IR_BUILDER_t* (*IMM)(long);
//     IR_BUILDER_t* (*TGT)(ir_instruction_t*);
//     ir_instruction_t* (*GET)();
// };
// IR_BUILDER_t* IR_BUILDER_INST(ir_opcode_t op) {

// }
// IR_BUILDER_t* IR_BUILDER() {
//     IR_BUILDER_t* builder = malloc(sizeof(*builder));
//     builder->_op = ir_NOP;
//     builder->_opers = NULL;
//     builder->INST =
// }

// void propogateSymbolInStatement(ast_stmt_t* stmt, symbol_table_entry_t* ste)
// {
//     if(stmt == NULL)
//         return;
//     // if the names match, set the symbol
//     if(stmt->type == ast_SYMBOL &&
//        strcmp(stmt->data.symbol->name, ste->symbol->name) == 0) {
//         stmt->data.symbol = ste->symbol;
//     }

//     if(stmt->left != NULL) {
//         propogateSymbolInStatements(stmt->left, ste);
//     }
//     if(stmt->right != NULL) {
//         propogateSymbolInStatements(stmt->right, ste);
//     }
// }
// // returns id of list head
// void propogateSymbolInStatements(ast_stmt_t* stmts, symbol_table_entry_t*
// ste) {
//     ast_stmt_t* temp = NULL;
//     LL_FOREACH(stmts, temp) { propogateSymbolInStatement(temp, ste); }
// }

ir_instruction_t* get_ir_instruction(ast_stmt_t* stmt, ir_memorymap_t* mm);

// expressions are either SYMBOLS, NUMBERS, or some sequence of instructions to
// result in a symbol
// always returns a symbol, sometimes makes an instruction
ir_operand_t* get_ir_expression(
    ast_stmt_t* stmt, ir_memorymap_t* mm, ir_instruction_t** inst) {
    ir_operand_t* oper = NULL;
    if(stmt->type == ast_SYMBOL) {
        *inst = NULL;
        oper = ir_build_operand_vregister(ir_get_irreg(mm, stmt->data.symbol));
    } else if(stmt->type == ast_NUMBER) {
        *inst = NULL;
        oper = ir_build_operand_immediate(stmt->data.num);
    } else {
        *inst = get_ir_instruction(stmt, mm);
        oper = (*inst)->operands[0];
    }
    return oper;
}

ir_opcode_t get_irop(ast_stmt_type_t tt) {
    switch(tt) {
        case ast_DEQUALS: return ir_CMP;
        case ast_PLUS: return ir_ADD;
        case ast_MINUS: return ir_SUB;
        default: return ir_NOP;
    }
}

ir_instruction_t* get_ir_instruction(ast_stmt_t* stmt, ir_memorymap_t* mm) {
    if(stmt == NULL) return NULL;

    // handle stmt
    ir_instruction_t* inst = NULL;
    switch(stmt->type) {
        case ast_EQUALS:
        case ast_DEQUALS:
        case ast_PLUS:
        case ast_MINUS: {
            ir_instruction_t* left_inst = NULL;
            ir_operand_t* left_oper =
                get_ir_expression(stmt->left, mm, &left_inst);
            ir_instruction_t* right_inst = NULL;
            ir_operand_t* right_oper =
                get_ir_expression(stmt->right, mm, &right_inst);

            ir_instruction_t* dummy_assign = NULL;
            if(stmt->type == ast_EQUALS) {
                dummy_assign =
                    ir_build_instruction(ir_COPY, left_oper, right_oper, NULL);
            } else {
                ir_operand_t* dummy_location =
                    ir_build_operand_vregister(ir_get_temp_irreg(mm));
                dummy_assign = ir_build_instruction(
                    get_irop(stmt->type), dummy_location, left_oper, right_oper,
                    NULL);
            }
            if(left_inst) DL_CONCAT_OR_APPEND(inst, left_inst);
            if(right_inst) DL_CONCAT_OR_APPEND(inst, right_inst);
            DL_CONCAT_OR_APPEND(inst, dummy_assign);

            break;
        }
        case ast_IF:
        case ast_WHILE: {
            ir_instruction_t* cond_inst = NULL;
            ir_operand_t* cond_oper =
                get_ir_expression(stmt->left, mm, &cond_inst);

            ir_instruction_t* jmp_target = ir_build_instruction(ir_NOP, NULL);

            ir_instruction_t* cjmp_inst = ir_build_instruction(
                ir_CJMP, cond_oper, ir_build_operand_target(jmp_target), NULL);

            ir_instruction_t* body_inst = get_ir_instruction(stmt->right, mm);

            if(cond_inst) DL_CONCAT_OR_APPEND(inst, cond_inst);
            DL_CONCAT_OR_APPEND(inst, cjmp_inst);
            DL_CONCAT_OR_APPEND(inst, body_inst);

            // if its a while loop, need to add the return to the loop top
            if(stmt->type == ast_WHILE) {
                ir_operand_t* jb_trgt = malloc(sizeof(*jb_trgt));
                jb_trgt->type = ir_JMP_TARGET;
                free(jb_trgt);
                // jb_trgt->target = inst;
                //ir_build_operand_target(inst);
                ir_instruction_t* jmp_back_inst =
                     ir_build_instruction(ir_JMP, NULL);
                DL_CONCAT_OR_APPEND(inst, jmp_back_inst);
            }

            DL_CONCAT_OR_APPEND(inst, jmp_target);

            break;
        }

        case ast_RETURN: {
            ir_instruction_t* ret_val_inst = NULL;
            ir_operand_t* ret_oper =
                get_ir_expression(stmt->left, mm, &ret_val_inst);

            ir_instruction_t* ret_inst =
                ir_build_instruction(ir_RET, ret_oper, NULL);
            if(ret_val_inst) DL_CONCAT_OR_APPEND(inst, ret_val_inst);
            DL_CONCAT_OR_APPEND(inst, ret_inst);
            break;
        }
        case ast_CALL: {
            // TODO: result, target func, operands
            // should push a return address to the stack
            // this should be an operands
            // but how do we know the address of other functions
            // make first instruction of function a nop? that is the symbol of
            // the function means the memmap would need to be exapanded to hold
            // all function memory

            // for now fall through
        }
        case ast_NOP: {
            ir_instruction_t* dummy = ir_build_instruction(ir_NOP, NULL);
            DL_CONCAT_OR_APPEND(inst, dummy);
            break;
        }
        case ast_NUMBER:
        case ast_SYMBOL:
            fprintf(
                stderr,
                "Terminal Statement reached with no known conversion\n");
            exit(2);
        default: fprintf(stderr, "Unknown Statement Type\n"); exit(2);
    }

    // handle the rest of list
    if(stmt->next != NULL) {
        ir_instruction_t* new_inst = get_ir_instruction(stmt->next, mm);
        DL_CONCAT_OR_APPEND(inst, new_inst);
    }
    return inst;
}

ir_function_t* get_ir_function(function_t* function) {
    ir_function_t* ir_func = malloc(sizeof(*ir_func));
    ir_func->ir = NULL;
    ir_func->mm = ir_init_mm();
    ir_func->name = strdup(function->symbol->name);

    symbol_list_t* symbols = NULL;

    // allocate all the params
    DL_FOREACH(function->params, symbols) {
        ir_get_irreg(ir_func->mm, symbols->symbol);
    }

    // allocate all the locals
    DL_FOREACH(function->locals, symbols) {
        ir_get_irreg(ir_func->mm, symbols->symbol);
    }

    ir_func->ir = get_ir_instruction(function->stmts, ir_func->mm);

    return ir_func;
}

ir_function_list_t* ast_to_ir(module_t* module) {
    function_list_t* funcs = NULL;
    ir_function_list_t* ir_funcs = NULL;
    DL_FOREACH(module->functions, funcs) {
        ir_function_list_t* new_elm = malloc(sizeof(*new_elm));
        new_elm->ir_function = get_ir_function(funcs->function);
        ;
        new_elm->next = NULL;
        new_elm->prev = NULL;
        DL_APPEND(ir_funcs, new_elm);
    }
    return ir_funcs;
}