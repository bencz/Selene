#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char look;
int lblcount; /* indicates the current label */

#define SYMTBL_SZ 1000
#define KWLIST_SZ 9

#define MAXTOKEN 16

int nsym; /* number of entries in the symbol table */

char *symtbl[SYMTBL_SZ]; /* symbol table */

char *kwlist[KWLIST_SZ] = {"IF", "ELSE", "ENDIF", "WHILE", "ENDWHILE",
			   "VAR", "BEGIN", "END", "PROGRAM"};
/* lista de palavras-chave */

char *kwcode = "ilewevbep";

char token; /* code of the current token */
char value[MAXTOKEN+1]; /* text of current token */

/* prototypes */
void init();
void nextchar();
void error(char *s);
void fatal(char *s);
void expected(char *s);
void match(char c);
void getname();
int getnum();
int isaddop(char c);
int ismulop(char c);
int isorop(char c);
int isrelop(char c);
void skipwhite();
void newline();

int newlabel();

void prog();
void header();
void prolog();
void epilog();
void mainblock();
void topdecls();
void decl();
void allocvar(char *name);
int intable(char *name);
void addsymbol(char *name);

void block();

void asm_clear();
void asm_negative();
void asm_loadconst(int i);
void asm_loadvar(char *name);
void asm_push();
void asm_popadd();
void asm_popsub();
void asm_popmul();
void asm_popdiv();
void asm_store(char *name);

void factor();
void negfactor();
void firstfactor();
void multiply();
void term1();
void term();
void firstterm();
void add();
void subtract();
void expression();
void assignment();

void asm_not();
void asm_popand();
void asm_popor();
void asm_popxor();
void asm_popcompare();
void asm_relop(char op);

void relation();
void notfactor();
void boolterm();
void boolor();
void boolxor();
void boolexpression();

void asm_jmp(int label);
void asm_jmpfalse(int label);
void doif();
void dowhile();

int lookup(char *s, char *list[], int size);
void scan();
void matchstring(char *s);

int main()
{
	init();
        prog();

        if (look != '\n')
                fatal("Unexpected data after \'.\'");

	return 0;
}

void init()
{
	nsym = 0;

	nextchar();
	scan();
}

void nextchar()
{
	look = getchar();
}

void error(char *s)
{
	fprintf(stderr, "Error: %s\n", s);
}

void fatal(char *s)
{
	error(s);
	exit(1);
}

void expected(char *s)
{
	fprintf(stderr, "Error: %s expected\n", s);
	exit(1);
}

/* warns about an unknown identifier */
void undefined(char *name)
{
        int i;
	fprintf(stderr, "Error: Undefined identifier %s\n", name);
        fprintf(stderr, "Symbol table:\n");
        for (i = 0; i < nsym; i++)
                fprintf(stderr, "%d: %s\n", i, symtbl[i]);
	exit(1);
}

void match(char c)
{
	char s[2];

        newline();
	if (look == c)
		nextchar();
	else {
		s[0] = c; /* conversion a quick (and ugly) */
		s[1] = '\0'; /* a character in string */
		expected(s);
	}
        skipwhite();
}

void getname()
{
	int i;
	
	newline();
	if (!isalpha(look))
		expected("Name");
	for (i = 0; isalnum(look) && i < MAXTOKEN; i++) {
		value[i] = toupper(look);
		nextchar();
	}
	value[i] = '\0';
        token = 'x';
        skipwhite();
}

int getnum()
{
	int i;

	i = 0;

        newline();
	if (!isdigit(look))
		expected("Integer");
	while (isdigit(look)) {
		i *= 10;
		i += look - '0';
		nextchar();
	}
        skipwhite();
	
        return i;
}

int isaddop(char c)
{
        return (c == '+' || c == '-');
}

int ismulop(char c)
{
        return (c == '*' || c == '/');
}

int isorop(char c)
{
        return (c == '|' || c == '~');
}

int isrelop(char c)
{
	return (strchr("=#<>", c) != NULL);
}

void skipwhite()
{
	while (look == ' ' || look == '\t')
		nextchar();
}

void newline()
{
	while (look == '\n') {
		nextchar();
		skipwhite();
	}
}

int newlabel()
{
	return lblcount++;
}

void prog()
{
	matchstring("PROGRAM");
	header();
        topdecls();
        mainblock();
	match('.');
}

void header()
{
	printf("\t.model small\n");
	printf("\t.stack\n");
	printf("\t.code\n");
        printf("PROG segment byte public\n");
        printf("\tassume cs:PROG,ds:PROG,es:PROG,ss:PROG\n");
}

void prolog()
{
        printf("MAIN:\n");
}

void epilog()
{
        printf("\tmov ax,4C00h\n");
        printf("\tint 21h\n");
        printf("PROG ends\n");
        printf("\tend MAIN\n");
}

void mainblock()
{
	matchstring("BEGIN");
	prolog();
        block();
	matchstring("END");
	epilog();
}

void topdecls()
{
        scan();
        while (token != 'b') {
                switch (token) {
                  case 'v':
                        decl();
                        break;
                  default:
                        error("Unrecognized keyword.");
                        expected("BEGIN");
                        break;
                }
                scan();
        }
}

/* Parse and Translate a statement */
void decl()
{
	for (;;) {
		getname();
	        allocvar(value);
                newline();
	        if (look != ',')
	        	break;
	        match(',');
	}
}

/* Parse and Translate a statement */
void allocvar(char *name)
{
	int value = 0, signal = 1;

        addsymbol(name);

        newline();
	if (look == '=') {
		match('=');
                newline();
                if (look == '-') {
                        match('-');
                        signal = -1;
                }
		value = signal * getnum();
	}	
	
        printf("%s:\tdw %d\n", name, value);
}

int intable(char *name)
{
        if (lookup(name, symtbl, nsym) < 0)
                return 0;
        return 1;
}

void addsymbol(char *name)
{
        char *newsym;

        if (intable(name)) {
                fprintf(stderr, "Duplicated variable name: %s", name);
                exit(1);
        }

        if (nsym >= SYMTBL_SZ) {
                fatal("Symbol table full!");
        }

        newsym = (char *) malloc(sizeof(char) * (strlen(name) + 1));
        if (newsym == NULL)
                fatal("Out of memory!");

        strcpy(newsym, name);

        symtbl[nsym++] = newsym;
}

void block()
{
	int follow = 0;
	
	do {
                scan();
		switch (token) {
		  case 'i':
		  	doif();
		  	break;
		  case 'w':
		  	dowhile();
		  	break;
		  case 'e':
		  case 'l':
		  	follow = 1;
		  	break;
		  default:
		  	assignment();
		  	break;
		}
	} while (!follow);
}


/* resets the primary register */
void asm_clear()
{
	printf("\txor ax, ax\n");
}

/* negatively reg. primary */
void asm_negative()
{
	printf("\tneg ax\n");
}

/* carries a numeric constant in reg. prim. */
void asm_loadconst(int i)
{
	printf("\tmov ax, %d\n", i);
}

/* carries a variable in the reg. prim. */
void asm_loadvar(char *name)
{
	if (!intable(name))
		undefined(name);
	printf("\tlea bx, [%s]\n", name);
	printf("\tmov ax, [bx]\n", name);
}

/* puts reg. prim. stack */
void asm_push()
{
	printf("\tpush ax\n");
}

/* adds the top of the stack to reg. prim. */
void asm_popadd()
{
	printf("\tpop bx\n");
	printf("\tadd ax, bx\n");
}

/* subtracts the reg. prim. the top of the stack */
void asm_popsub()
{
	printf("\tpop bx\n");
	printf("\tsub ax, bx\n");
	printf("\tneg ax\n");
}

/* multiplies the top of the stack by reg. prim. */
void asm_popmul()
{
	printf("\tpop bx\n");
	printf("\tmul bx\n");
}

/* divides the top of the stack by reg. prim. */
void asm_popdiv()
{
	printf("\tpop bx\n");
	printf("\txchg ax, bx\n");
	printf("\tdiv bx\n");
}

/* store reg. prim. in variable */
void asm_store(char *name)
{
	if (!intable(name))
		undefined(name);
	printf("\tlea bx, [%s]\n", name);
   	printf("\tmov [bx], ax\n");
}

/* Parse and Translate a mathematical factor */
void factor()
{
        newline();
	if (look == '(') {
		match('(');
		boolexpression();
		match(')');
	} else if (isalpha(look)) {
		getname();
		asm_loadvar(value);
	} else
		asm_loadconst(getnum());
}

/* analyzes and reflects a negative factor */
void negfactor()
{
	match('-');
	if (isdigit(look))
		asm_loadconst(-getnum());
	else {
		factor();
		asm_negative();
	}
}

/* analyzes and translates an initial factor */
void firstfactor()
{
        newline();
	switch (look) {
	  case '+':
		match('+');
		factor();
		break;
	  case '-':
		negfactor();
		break;
	  default:
		factor();
		break;
	}
}

/* recognizes and reflects a multiplication */
void multiply()
{
	match('*');
	factor();
	asm_popmul();
}

/* recognizes and reflects a division */
void divide()
{
	match('/');
	factor();
	asm_popdiv();
}

/* common code used by "term" and "firstterm" */
void term1()
{
        newline();
	while (ismulop(look))  {
		asm_push();
		switch (look) {
		  case '*':
			multiply();
			break;
		  case '/':
			divide();
			break;
		}
                newline();
	}
}

/* Parse and Translate a mathematical term */
void term()
{
	factor();
	term1();
}

/* analyzes and translates an initial term */
void firstterm()
{
	firstfactor();
	term1();
}

/* recognizes and translates an addition */
void add()
{
	match('+');
	term();
	asm_popadd();
}



/* recognizes and reflects a subtraction */
void subtract()
{
	match('-');
	term();
	asm_popsub();
}

/* analyze and translate a mathematical expression */
void expression()
{
	firstterm();
        newline();
	while (isaddop(look))  {
		asm_push();
		switch (look) {
		  case '+':
			add();
			break;
		  case '-':
			subtract();
			break;
		}
                newline();
	}
}

/* Parse and Translate an assignment */
void assignment()
{
	char name[MAXTOKEN+1];

	strcpy(name, value);
	match('=');
	boolexpression();
	asm_store(name);
}


/* reverses reg. prim. */
void asm_not()
{
	 printf("\tnot ax\n");
}

/* "E" from the top of the stack with reg. prim. */
void asm_popand()
{
	 printf("\tpop bx\n");
	 printf("\tand ax, bx\n");
}

/* "OR" from the top of the stack with reg. prim. */
void asm_popor()
{
	 printf("\tpop bx\n");
	 printf("\tor ax, bx\n");
}

/* "Exclusive or" top of the stack with reg. prim. */
void asm_popxor()
{
	 printf("\tpop bx\n");
	 printf("\txor ax, bx\n");
}

/* compares the top of the stack with reg. prim. */
void asm_popcompare()
{
	 printf("\tpop bx\n");
	 printf("\tcmp bx, ax\n");
}

/* amending reg. primary (and flags, indirectly) as compared */
void asm_relop(char op)
{
	char *jump;
        int l1, l2;

        l1 = newlabel();
        l2 = newlabel();

	switch (op) {
	  case '=': jump = "je"; break;
	  case '#': jump = "jne"; break;
	  case '<': jump = "jl"; break;
	  case '>': jump = "jg"; break;
	  case 'L': jump = "jle"; break;
	  case 'G': jump = "jge"; break;
	}

	printf("\t%s L%d\n", jump, l1);
	printf("\txor ax, ax\n");
	printf("\tjmp L%d\n", l2);
	printf("L%d:\n", l1);
	printf("\tmov ax, -1\n");
	printf("L%d:\n", l2);
}

/* analyzes and reflects a relationship */
void relation()
{
	char op;

	expression();
	if (isrelop(look)) {
		op = look;
		match(op); /* only to remove the operator of the way */
                if (op == '<') {
                        if (look == '>') { /* <> */
                                match('>');
                                op = '#';
                        } else if (look == '=') {
                                match('=');
                                op = 'L';
                        }

                } else if (op == '>' && look == '=') {
                        match('=');
                        op = 'G';
                }
		asm_push();
		expression();
		asm_popcompare();
		asm_relop(op);
	}
}

/* Parse and Translate a Boolean factor with NOT starting */
void notfactor()
{
	if (look == '!') {
		match('!');
		relation();
		asm_not();
	} else
		relation();
}

/* Parse and Translate a Boolean term */
void boolterm()
{
	notfactor();
        newline();
	while (look == '&') {
		asm_push();
		match('&');
		notfactor();
		asm_popand();
                newline();
	}
}

/* recognizes and reflects an "OR" */
void boolor()
{
	match('|');
	boolterm();
	asm_popor();
}

/* recognizes and reflects a "xor" */
void boolxor()
{
	match('~');
	boolterm();
	asm_popxor();
}

/* Parse and Translate a Boolean expression */
void boolexpression()
{
	boolterm();
        newline();
	while (isorop(look)) {
		asm_push();
		switch (look) {
		  case '|':
		  	boolor();
		  	break;
		  case '~':
		  	boolxor();
		  	break;
		}
                newline();
	}
}

/* unconditional */
void asm_jmp(int label)
{
	printf("\tjmp L%d\n", label);
}

/* deviation is false (0) */
void asm_jmpfalse(int label)
{
	printf("\tjz L%d\n", label);
}

void doif()
{
	int l1, l2;
	
	boolexpression();
	l1 = newlabel();
	l2 = l1;
	asm_jmpfalse(l1);
	block();
	if (token == 'l') {
		l2 = newlabel();
		asm_jmp(l2);
		printf("L%d:\n", l1);
		block();
	}
	printf("L%d:\n", l2);
	matchstring("ENDIF");
}

void dowhile()
{
	int l1, l2;

	l1 = newlabel();
	l2 = newlabel();
	printf("L%d:\n", l1);
	boolexpression();
	asm_jmpfalse(l2);
	block();
	matchstring("ENDWHILE");
	asm_jmp(l1);
	printf("L%d:\n", l2);
}

int lookup(char *s, char *list[], int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (strcmp(list[i], s) == 0)
			return i;
	}

	return -1;
}

void scan()
{
        int kw;

	getname();
        kw = lookup(value, kwlist, KWLIST_SZ);
        if (kw == -1)
                token = 'x';
        else
                token = kwcode[kw];
}

void matchstring(char *s)
{
        if (strcmp(value, s) != 0)
                expected(s);
}

