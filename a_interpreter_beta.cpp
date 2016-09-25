#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
using namespace std;

int debug;    // print the executed instructions
int assembly; // print out the assembly and source

int token; // current token

// instructions
// 指令集
enum { LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT };

// tokens and classes (operators last and in precedence order)
// copied from c4
// token可能等于一下：
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// fields of identifier
// symbols表的字段
enum {Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize};


// types of variable/function
enum { CHAR, INT, PTR };

// type of declaration.
enum {Global, Local};

int *text, // text segment
    *stack;// stack
int * old_text; // for dump text segment
char *data; // data segment
int *idmain;

char *src, *old_src;  // pointer to source code string;

int poolsize; // default size of text/data/stack
int *pc, *bp, *sp, ax, cycle; // virtual machine registers

int *current_id, // current parsed ID
    *symbols,    // symbol table
    line,        // line number of source code
    token_val;   // value of current token (mainly for number)

int basetype;    // the type of a declaration, make it global for convenience
int expr_type;   // the type of an expression

// function frame
//
// 0: arg 1
// 1: arg 2
// 2: arg 3
// 3: return address
// 4: old bp pointer  <- index_of_bp
// 5: local var 1
// 6: local var 2
int index_of_bp; // index of bp pointer on stack

int eval() {
    int op, *tmp;
    cycle = 0;
    while (1) {
        cycle ++;
        op = *pc++; // get next operation code

        // print debug info
        if (debug) {
            printf("%d> %.4s", cycle,
                   & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                   "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                   "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT"[op * 5]);
            if (op <= ADJ)
                printf(" %d\n", *pc);
            else
                printf("\n");
        }
        if (op == IMM)       {ax = *pc++;}                                     // load immediate value to ax
        else if (op == LC)   {ax = *(char *)ax;}                               // load character to ax, address in ax
        else if (op == LI)   {ax = *(int *)ax;}                                // load integer to ax, address in ax
        else if (op == SC)   {ax = *(char *)*sp++ = ax;}                       // save character to address, value in ax, address on stack
        else if (op == SI)   {*(int *)*sp++ = ax;}                             // save integer to address, value in ax, address on stack
        else if (op == PUSH) {*--sp = ax;}                                     // push the value of ax onto the stack
        else if (op == JMP)  {pc = (int *)*pc;}                                // jump to the address
        else if (op == JZ)   {pc = ax ? pc + 1 : (int *)*pc;}                   // jump if ax is zero
        else if (op == JNZ)  {pc = ax ? (int *)*pc : pc + 1;}                   // jump if ax is zero
        else if (op == CALL) {*--sp = (long long)(pc+1); pc = (int *)*pc;}           // call subroutine
        //else if (op == RET)  {pc = (int *)*sp++;}                              // return from subroutine;
        else if (op == ENT)  {*--sp = (long long)bp; bp = sp; sp = sp - *pc++;}      // make new stack frame
        else if (op == ADJ)  {sp = sp + *pc++;}                                // add esp, <size>
        else if (op == LEV)  {sp = bp; bp = (int *)*sp++; pc = (int *)*sp++;}  // restore call frame and PC
        else if (op == LEA)  {ax = (long long)(bp + *pc++);}                         // load address for arguments.

        else if (op == OR)  ax = *sp++ | ax;
        else if (op == XOR) ax = *sp++ ^ ax;
        else if (op == AND) ax = *sp++ & ax;
        else if (op == EQ)  ax = *sp++ == ax;
        else if (op == NE)  ax = *sp++ != ax;
        else if (op == LT)  ax = *sp++ < ax;
        else if (op == LE)  ax = *sp++ <= ax;
        else if (op == GT)  ax = *sp++ >  ax;
        else if (op == GE)  ax = *sp++ >= ax;
        else if (op == SHL) ax = *sp++ << ax;
        else if (op == SHR) ax = *sp++ >> ax;
        else if (op == ADD) ax = *sp++ + ax;
        else if (op == SUB) ax = *sp++ - ax;
        else if (op == MUL) ax = *sp++ * ax;
        else if (op == DIV) ax = *sp++ / ax;
        else if (op == MOD) ax = *sp++ % ax;

        else if (op == EXIT) { printf("exit(%d)", *sp); return *sp;}
        else if (op == OPEN) { ax = open((char *)sp[1], sp[0]); }
        else if (op == CLOS) { ax = close(*sp);}
        else if (op == READ) { ax = read(sp[2], (char *)sp[1], *sp); }
        else if (op == PRTF) { tmp = sp + pc[1]; ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); }

        //why is change to (int)? why not is (*int)?
        //else if (op == MALC) { ax = (int)malloc(*sp);}
       // else if (op == MSET) { ax = (int)memset((char *)sp[2], sp[1], *sp);}
        else if (op == MCMP) { ax = memcmp((char *)sp[2], (char *)sp[1], *sp);}
        else {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
}

// 词法分析器
// 传参给token
void next() {
    char *last_pos;//词汇的开头位置 词汇包括：数字，标识符和关键字等
    int hash=0;    //哈希值

    while(token=*src) {
        src++;

        //分析换行符'\n'
        if(token=='\n') {
            line++;
        }


        //分析词汇.
        //词汇:变量名，关键字等.
        else if(  (token>='a' && token<='z')  ||  (token>='A' && token<='Z')
                  || (token=='_') ) {

            last_pos=src-1;
            hash=token;

            //整个词汇的哈希值
            while( (*src>='a' && *src<='z')  ||  (*src>='A' && *src<='Z')
                    || (*src>='0' && *src<='9')  ||  (*src=='_')  ) {
                hash=hash * 147 + *src;
                src++;
            }

            //根据哈希值,搜索词汇.
            current_id=symbols;
            while(current_id[Token]) { //这行有值,再执行.
                if(current_id[Hash]==hash) {
                    token=current_id[Token];
                    return;//循环结束
                }
                current_id=current_id+IdSize;
            }

            //没有找到，新建一行.
            current_id[Name]=(long long)last_pos;
            current_id[Hash]=hash;
            token=current_id[Token]=Id;
            return;
        }

        //分析数字
        else if( token>='0' && token<='9' ) {
            token_val=token - '0';

            //十进制数字
            if(token_val>0) {
                while(*src>='0' && *src<='9' ) {
                    token_val = token_val * 10 +
                                (*src-'0');
                    src++;
                }
            }

            token=Num;
            return;
        }


        //分析等于'='
        else if(token=='=') {
            if(*src=='=') { //下一个还是'='，那么是'=='.
                src++;
                token=Eq;
            } else {
                token=Assign;
            }
            return;
        }

        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':') {
            // directly return the character as token;
            return;
        }
    }


}

int match(int tk) {
    if(token==tk) {
        next();
    } else {
        printf("%d: Sundy:expected token: %d\n", line, tk);
        cout<<(char)tk;
        exit(-1);
    }
}

int expression(int level) {
    int *id;
    int tmp;
    int *addr;

    {
        if (!token) {
            printf("%d: unexpected token EOF of expression\n", line);
            exit(-1);
        }
        if (token == Num) {//处理数字
            match(Num);
            *++text = IMM;
            *++text = token_val;
            expr_type = INT;
        }
        else if (token == Id) {//处理Id
            // there are several type when occurs to Id
            // but this is unit, so it can only be
            // 1. function call
            // 2. Enum variable
            // 3. global/local variable
            match(Id);

            id = current_id;

            if (token == '(') {
                // function call
                match('(');

                // pass in arguments
                tmp = 0; // number of arguments
                while (token != ')') {
                    expression(Assign);
                    *++text = PUSH;
                    tmp ++;

                    if (token == ',') {
                        match(',');
                    }

                }
                match(')');

                // emit code
                if (id[Class] == Sys) {
                    // system functions
                    *++text = id[Value];
                }
                else if (id[Class] == Fun) {
                    // function call
                    *++text = CALL;
                    *++text = id[Value];
                }
                else {
                    printf("%d: bad function call\n", line);
                    exit(-1);
                }

                // clean the stack for arguments
                if (tmp > 0) {
                    *++text = ADJ;
                    *++text = tmp;
                }
                expr_type = id[Type];
            }
            else if (id[Class] == Num) {
                // enum variable
                *++text = IMM;
                *++text = id[Value];
                expr_type = INT;
            }
            else {
                // variable
                if (id[Class] == Loc) {
                    *++text = LEA;
					//栈往下增长
                    *++text = index_of_bp - id[Value];
                }
                else if (id[Class] == Glo) {
                    *++text = IMM;
                    *++text = id[Value];
                }
                else {
                    printf("%d: undefined variable\n", line);
                    exit(-1);
                }

                // emit code, default behaviour is to load the value of the
                // address which is stored in `ax`
                expr_type = id[Type];
                *++text = (expr_type == Char) ? LC : LI;
            }
        }
        else {
            printf("%d: bad expression\n", line);
            exit(-1);
        }
    }

    {
        while(token>=level) {
            tmp=expr_type;
            if(token==Assign) {

                match(Assign);

                if(*text==LC||*text==LI) {
                    *text=PUSH;
                }

                expression(Assign);

                expr_type=tmp;
                *++text=(expr_type==CHAR)?
                        SC:SI;
            }
            else {
                printf("%d: compiler error, token = %d\n", line, token);
                exit(-1);
            }
        }

    }


}

void statement() {
    //if..
    //else..

    if(token==';') {
            cout<<"wa"<<endl;
        match(';');
    } else {
        expression(Assign);

        cout<<"wa2"<<endl;
        match(';');
    }

}

int function_body() {
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }

    int pos_local;
    int type;

    pos_local=index_of_bp;

    //定义变量
    while(token==Int || token ==Char ) {
        basetype=(token==Int)?INT:CHAR;
        match(token);

        while(token!=';') {
            type=basetype;

            match(Id);

            //========
            current_id[BClass] = current_id[Class];
            current_id[Class]  = Loc;
            current_id[BType]  = current_id[Type];
            current_id[Type]   = type;
            current_id[BValue] = current_id[Value];
            current_id[Value]  = ++pos_local;   // index of current parameter
        }
        match(';');
    }

    *++text=ENT;
    *++text=pos_local-index_of_bp;//分配的尺寸

    while(token != '}') {
        statement();
        cout<<"loop"<<endl;
    }


    *++text=LEV;
    cout<<"arrived ";

}

void function_parameter(){
    //..
}

void function_declaration() {
    // type func_name (...) {...}
    //              ->|         |<-
    //              this part
    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();

}

int global_declaration() {
    // int [*]id [; | (...) {...}]

    int type;
    int i;

    basetype = INT;

    if(token==Int) {
             //cout<<"token="<<token<<endl;
        match(Int);
    }

    //分析是变量还是函数
    while(token!=';' && token!='}' ) {
        type=basetype;

        if(token!=Id) {
            printf("%d: Sundy:why not is Id?\n", line);
            //cout<<"token="<<token<<endl;
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;

        //后有'('显然是函数
        if(token=='(') {
            current_id[Class]=Fun;
            current_id[Value]=(long long)(text+1);
            function_declaration();
        }
        else {
            //对变量进行处理
        }
    }

    next();
}

int program() {
    next();
    while(token>0) {
        global_declaration();
    }
    cout<<"arrived2";
}


int main(int argc,char **argv) {

    int i, fd;
    int *tmp;

    argc--;
    argv++;

    // parse arguments
    if (argc > 0 && **argv == '-' && (*argv)[1] == 's') {
        assembly = 1;
        --argc;
        ++argv;
    }
    if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') {
        debug = 1;
        --argc;
        ++argv;
    }
    if (argc < 1) {
        printf("usage: xc [-s] [-d] file ...\n");
        return -1;
    }

    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    poolsize = 256 * 1024; // arbitrary size
    line = 1;

    // allocate memory
    if (!(text = (int *)malloc(poolsize))) {
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    if (!(data = (char *)malloc(poolsize))) {
        printf("could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = (int *)malloc(poolsize))) {
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    if (!(symbols = (int *)malloc(poolsize))) {
        printf("could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }

    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    memset(symbols, 0, poolsize);

    old_text = text;

    src = "char else enum if int return sizeof while "
          "open read close printf malloc memset memcmp exit void main";

     // add keywords to symbol table
    i = Char;
    while (i <= While) {
        next();
        current_id[Token] = i++;
    }

    // add library to symbol table
    i = OPEN;
    while (i <= EXIT) {
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }

    next(); current_id[Token] = Char; // handle void type
    next(); idmain = current_id; // keep track of main

    if (!(src = old_src = (char*)malloc(poolsize))) {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }
    // read the source file
    if ((i = read(fd, src, poolsize-1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0; // add EOF character
    close(fd);

    program();

    if (!(pc = (int *)idmain[Value])) {
        printf("main() not defined\n");
        return -1;
    }

    // dump_text();
    if (assembly) {
        // only for compile
        return 0;
    }

    // setup stack
    sp = (int *)((long long)stack + poolsize);
    *--sp = EXIT; // call exit if main returns
    *--sp = PUSH; tmp = sp;
    *--sp = argc;
    *--sp = (long long)argv;
    *--sp = (long long)tmp;


    cout<<"-----text------"<<endl;
    for (int i=0;i<=15 ;i++ ){
        cout<<old_text[i]<<endl;
    }
    cout<<"-----text------"<<endl;

    eval();


    return 0;
}
