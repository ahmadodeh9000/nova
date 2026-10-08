/**
 * @author: Ahmad Odeh
 * @about: nova is a basic lisp interpreter
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpc.h"

#define NOVA_VERSION    "Nova 0.0.1"
#define NOVA_ABOUT      "Made with love"
#define BUFFER_SIZE     1 << 11


typedef struct lval {
    int type;
    union {
        long num;
        char* err;
        char* sym;
    };

    int count;
    struct lval** cell;

} lval;

enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR};
enum { NOVA_DIV_ZERO, NOVA_BAD_OP, NOVA_BAD_NUM };


static char input[BUFFER_SIZE];


static lval eval_op(lval x, char* op, lval y);
static lval eval(mpc_ast_t* t);
static int ipow (int base, int exp);

static lval* lval_num(long x);
static lval* lval_err(char* x);
static lval* lval_sym(char* s);
static lval* lval_sexpr(void); 

static void lval_del(lval* v) ;
static lval* lval_read_num(mpc_ast_t* t);
static lval* lval_read(mpc_ast_t* t) ;
static lval* lval_add(lval* v, lval* x);

static void lval_expr_print(lval* v, char open, char closed);
static void lval_print(lval* v);
static void lval_println(lval* v);


int 
main (int argc, char** argv) {
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Symbol    = mpc_new("symbol");
    mpc_parser_t* Sexpr     = mpc_new("sexpr");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* nova      = mpc_new("nova");

    /* Define them with the following Language */

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                          \
            number : /-?[0-9]+/ ;                    \
            symbol : '+' | '-' | '*' | '/' | '%' | '^' ;         \
            sexpr  : '(' <expr>* ')' ;               \
            expr   : <number> | <symbol> | <sexpr> ; \
            nova  : /^/ <expr>* /$/ ;               \
        ",
    Number, Symbol, Sexpr, Expr, nova);


    printf("%s\n%s\n",NOVA_VERSION,NOVA_ABOUT);


    while (1) {

        fputs("nova> ",stdout);
        fgets(input,BUFFER_SIZE,stdin);

        mpc_result_t r;

        if (mpc_parse("<stdin>",input,nova,&r)) {
            lval* x = lval_read(r.output);
            lval_println(x);
            lval_del(x);
        }

        else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

    }

    /* cleaning up */
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, nova);
    return 0;
}



// lval stuff

static void 
lval_del(lval* v) {

  switch (v->type) {
    /* Do nothing special for number type */
    case LVAL_NUM: break;

    /* For Err or Sym free the string data */
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;

    /* If Sexpr then delete all elements inside */
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      /* Also free the memory allocated to contain the pointers */
      free(v->cell);
    break;
  }

  /* Free the memory allocated for the "lval" struct itself */
  free(v);
}


static lval* 
lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}


static lval* 
lval_read(mpc_ast_t* t) {

  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* If root (>) or sexpr then create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }

  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

static lval* 
lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

static int 
ipow (int base, int exp) {
    int res = 1;

    for (;;) {
        if (exp & 1)
            res *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return res;
}

/*
static lval 
eval_op(lval x, char* op, lval y) {

    if (LVAL_ERR == x.type) return x;
    if (LVAL_ERR == y.type) return y;

    if (0 == strcmp(op,"+")) return lval_num(x.num + y.num);
    if (0 == strcmp(op,"-")) return lval_num(x.num - y.num);
    if (0 == strcmp(op,"*")) return lval_num(x.num * y.num);
    if (0 == strcmp(op,"%")) return lval_num(x.num % y.num);
    if (0 == strcmp(op,"^")) return lval_num(ipow(x.num,y.num));

    if (0 == strcmp(op,"/")) {
        return y.num == 0
            ? lval_err(NOVA_DIV_ZERO)
            : lval_num(x.num / y.num);

    }


    return lval_err(NOVA_BAD_OP);
}

static lval 
eval(mpc_ast_t* t) {

  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(NOVA_BAD_NUM);
  }

  char* op = t->children[1]->contents;
  lval x = eval(t->children[2]);

  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}
*/

lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));

    if (NULL == v) {
        fprintf(
            stderr,
            "FAILED TO ALLOCATE FROM MEMORY ALL IS LOST !!!\n"
        );
        return NULL;
    }

    v->num   = x;
    v->type  = LVAL_NUM;
    return v; 
}

lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));

    if (NULL == v) {
        fprintf(
            stderr,
            "FAILED TO ALLOCATE FROM MEMORY ALL IS LOST !!!\n"
        );
        return NULL;
    }

    v->err   = malloc(strlen(m) + 1);
    v->type  = LVAL_ERR;

    if (NULL == v->err) {
        fprintf(
            stderr,
            "FAILED TO ALLOCATE FROM MEMORY\nALL IS LOST !!!\n"
        );
        return NULL;
    }
    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));

    if (NULL == v) {
        fprintf(
            stderr,
            "FAILED TO ALLOCATE FROM MEMORY ALL IS LOST !!!\n"
        );
        return NULL;
    }

    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);

    if (NULL == v->sym) {
        fprintf(
            stderr,
            "FAILED TO ALLOCATE FROM MEMORY ALL IS LOST !!!\n"
        );
        return NULL;
    }

    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    
    if (NULL == v) {
        fprintf(
            stderr,
            "FAILED TO ALLOCATE FROM MEMORY ALL IS LOST !!!\n"
        );
        return NULL;
    }
    
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}


static void 
lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  }
}

static void 
lval_println(lval* v) { 
    lval_print(v); 
    putchar('\n'); 
}


static void lval_expr_print(lval* v, char open, char close) {
    
    if (NULL == v) {
        return ;
    }
    
    putchar(open);
    for (int i = 0; i < v->count; ++i) {
        lval_print(v->cell[i]);

        if (i != (v->count - 1)) {
            putchar(' ');
        }

    }
    putchar(close);

}



