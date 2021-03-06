
#include "common/args/arg_creation.h"
#include "common/args/file_helper.h"
#include "parser/parser.h"
#include "sema/ast/ast.h"
#include "sema/ast/pt_conversion.h"
#include "sema/pass_manager/pass_manager.h"
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char* symbolColors(symbol_t* sym) {
    char buf[128];
    int i = 0;
    memset(buf, 0, 128);

    i += sprintf(buf + i, "style=filled;");

    if(sym->flags & st_PARAM) {
        i += sprintf(buf + i, "fillcolor=lightblue");
    } else if(sym->flags & st_LOCAL) {
        i += sprintf(buf + i, "fillcolor=lightgreen");
    } else if(sym->flags & st_FUNCTION) {
        i += sprintf(buf + i, "fillcolor=darkolivegreen");
    } else {
        i += sprintf(buf + i, "fillcolor=red");
    }
    return strdup(buf);
}

void symbolListToDot(char* name, symbol_list_t* sym_list, FILE* f) {
    fprintf(f, "symList_%ld[label=\"%s\"];\n", (intptr_t)sym_list, name);
    symbol_list_t* temp = sym_list;
    while(temp != NULL) {
        fprintf(
            f, "sym_%ld_%ld[label=\"%s\";%s];\n", (intptr_t)(sym_list),
            (intptr_t)(temp->symbol), temp->symbol->name,
            symbolColors(temp->symbol));
        fprintf(
            f, "symList_%ld->sym_%ld_%ld;\n", (intptr_t)sym_list,
            (intptr_t)(sym_list), (intptr_t)(temp->symbol));
        temp = temp->next;
    }
}

intptr_t statementsToDot(ast_stmt_t* stmts, FILE* f);

void statementToDot(ast_stmt_t* stmt, FILE* f) {
    if(stmt == NULL) return;
    fprintf(f, "stmt_%ld", (intptr_t)(stmt));
    switch(stmt->type) {
        case ast_NUMBER: fprintf(f, "[label=\"%d\"];\n", stmt->data.num); break;

        case ast_SYMBOL:
            fprintf(
                f, "[label=\"%s\";%s];\n", stmt->data.symbol->name,
                symbolColors(stmt->data.symbol));
            break;
        case ast_EQUALS:
        case ast_DEQUALS:
        case ast_PLUS:
        case ast_MINUS:
        case ast_IF:
        case ast_WHILE:
        case ast_RETURN:
        case ast_CALL:
        case ast_PRINT:
            fprintf(f, "[label=\"%s\"];\n", getASTTypeString(stmt->type));
            break;

        case ast_NOP:
            fprintf(f, ";\n");
            fprintf(
                stderr, "Unhandled statement type %s\n",
                getASTTypeString(stmt->type));
            break;
    }

    if(stmt->left != NULL) {
        fprintf(
            f, "stmt_%ld->stmt_%ld;\n", (intptr_t)(stmt),
            (intptr_t)(stmt->left));
        statementsToDot(stmt->left, f);
    }
    if(stmt->right != NULL) {
        fprintf(
            f, "stmt_%ld->stmt_%ld;\n", (intptr_t)(stmt),
            (intptr_t)(stmt->right));
        statementsToDot(stmt->right, f);
    }
}

// returns id of list head
intptr_t statementsToDot(ast_stmt_t* stmts, FILE* f) {
    intptr_t headId = (intptr_t)stmts;
    intptr_t prevId = 0;
    ast_stmt_t* temp = stmts;
    while(temp != NULL) {
        statementToDot(temp, f);
        if(prevId) {
            fprintf(
                f, "stmt_%ld->stmt_%ld[label=next;color=crimson];\n", prevId,
                (intptr_t)(temp));
        }
        prevId = (intptr_t)(temp);
        temp = temp->next;
    }
    return headId;
}

void functionToDot(function_t* func, FILE* f) {
    fprintf(
        f, "subgraph cluster_func_%ld {\nlabel=\"function: %s\";\n",
        (intptr_t)func, func->symbol->name);

    fprintf(
        f, "func_%ld[label=\"%s\";%s];\n", (intptr_t)func, func->symbol->name,
        symbolColors(func->symbol));

    if(func->params) {
        symbolListToDot("params", func->params, f);
        fprintf(
            f, "func_%ld->symList_%ld;\n", (intptr_t)func,
            (intptr_t)func->params);
    }
    if(func->locals) {
        symbolListToDot("locals", func->locals, f);
        fprintf(
            f, "func_%ld->symList_%ld;\n", (intptr_t)func,
            (intptr_t)func->locals);
    }
    if(func->stmts) {
        intptr_t id = statementsToDot(func->stmts, f);
        if(id) {
            fprintf(f, "func_%ld->stmt_%ld;\n", (intptr_t)(func), (intptr_t)id);
        }
    }

    fprintf(f, "}\n");
}

void moduleToDot(module_t* module, FILE* f) {
    fprintf(f, "digraph ast {\n");
    fprintf(f, "compound=true;\n");
    function_list_t* functions = module->functions;
    function_list_t* temp = functions;
    while(temp != NULL) {
        functionToDot(temp->function, f);
        temp = temp->next;
    }
    fprintf(f, "}\n");
}

int _debug_mode = 0;

int main(int argc, char** argv) {

    char* inFileName = NULL;
    char* outFileName = NULL;
#define ARGS(WITH_ARG, WITH_BOOL, WITH_CUSTOM)                                 \
    WITH_ARG(i, inFileName)                                                    \
    WITH_ARG(o, outFileName)                                                   \
    WITH_BOOL(d, _debug_mode)

    MAKE_ARGS(argc, argv, ARGS);

    FILE* inFile = NULL;
    OPEN_FILE_OR_STDIN(inFile, inFileName);
    pt_t* root = parse(inFile);
    fclose(inFile);
    module_t* module = pt_to_ast(root);
    runPass(module, buildPass(PASS_build_symbol_table));

    FILE* outFile = NULL;
    OPEN_FILE_OR_STDOUT(outFile, outFileName);
    moduleToDot(module, outFile);
    fclose(outFile);

    return 0;
}
