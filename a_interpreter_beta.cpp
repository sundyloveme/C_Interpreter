// 作者:Sundy
// 根据C-interpreter简化的一个编译器

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
using namespace std;

int debug;    // 当等于1时, 程序进入debug模式
int assembly; // 一种程序功能, 暂时不知道什么功能
int token; // 标记

// 汇编指令集
enum {
	LEA,IMM,JMP,CALL,JZ,JNZ,ENT,ADJ,LEV,LI,LC,SI,SC,PUSH,
	OR,XOR,AND,EQ,NE,LT,GT,LE,GE,SHL,SHR,ADD,SUB,MUL,DIV,MOD,
	OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT
};

// token标记集
enum {
	Num = 128, Fun, Sys, Glo, Loc, Id,
	Char, Else, Enum, If, Int, Return, Sizeof, While,
	Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// symbols表字段
enum {
	Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize
};

// types of variable/function
enum { CHAR, INT, PTR };

// type of declaration.
enum {Global, Local};

int *text, // text segment
    *stack;// stack
int * old_text; // for dump text segment
char *data; // data segment
int *idmain;

// 栈的原始指针
int *old_sp;

char *src, *old_src;  // pointer to source code string;

int poolsize; // 最大尺寸
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
		if (op == IMM)       {
			ax = *pc++;   // load immediate value to ax
		} else if (op == LC)   {
			ax = *(char *)ax;   // load character to ax, address in ax
		} else if (op == LI)   {
			ax = *(int *)ax;   // load integer to ax, address in ax
		} else if (op == SC)   {
			ax = *(char *)*sp++ = ax;   // save character to address, value in ax, address on stack
		} else if (op == SI)   {
			*(int *)*sp++ = ax;   // save integer to address, value in ax, address on stack
		} else if (op == PUSH) {
			*--sp = ax;   // push the value of ax onto the stack
		} else if (op == JMP)  {
			pc = (int *)*pc;   // jump to the address
		} else if (op == JZ)   {
			pc = ax ? pc + 1 : (int *)*pc;   // jump if ax is zero
		} else if (op == JNZ)  {
			pc = ax ? (int *)*pc : pc + 1;   // jump if ax is zero
		} else if (op == CALL) {
			*--sp = (long long)(pc+1);    // call subroutine
			pc = (int *)*pc;
		}
		//else if (op == RET)  {pc = (int *)*sp++;}                              // return from subroutine;
		else if (op == ENT)  {
			*--sp = (long long)bp;    // make new stack frame
			bp = sp;
			sp = sp - *pc++;
		} else if (op == ADJ)  {
			sp = sp + *pc++;   // add esp, <size>
		} else if (op == LEV)  {
			sp = bp;    // restore call frame and PC
			bp = (int *)*sp++;
			pc = (int *)*sp++;
		} else if (op == LEA)  {
			ax = (long long)(bp + *pc++);   // load address for arguments.
		}

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

		else if (op == EXIT) {
			printf("exit(%d)", *sp);
			return *sp;
		} else if (op == OPEN) {
			ax = open((char *)sp[1], sp[0]);
		} else if (op == CLOS) {
			ax = close(*sp);
		} else if (op == READ) {
			ax = read(sp[2], (char *)sp[1], *sp);
		} else if (op == PRTF) {
			// printf("%d",var);
			tmp = sp + pc[1];
			ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);
		}

		//why is change to (int)? why not is (*int)?
		//else if (op == MALC) { ax = (int)malloc(*sp);}
		// else if (op == MSET) { ax = (int)memset((char *)sp[2], sp[1], *sp);}
		else if (op == MCMP) {
			ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
		} else {
			printf("unknown instruction:%d\n", op);
			return -1;
		}
	}
}

// 词法分析器
// 传参给token
void next() {
	char *last_pos;// 词汇的开头位置 词汇包括：数字，标识符和关键字等
	int hash=0;    // 哈希值

	while(token=*src) {
		src++;

		if(token=='\n') { 					  // 分析换行符'\n'
			line++;
		}
											  // 分析词汇, 词汇:变量名, 关键字等
		else if(  (token>='a' && token<='z')  ||  (token>='A' && token<='Z')|| (token=='_') ) {
		          

			last_pos=src-1;
			hash=token;

			// 整个词汇的哈希值
			while( (*src>='a' && *src<='z')  ||  (*src>='A' && *src<='Z')
			        || (*src>='0' && *src<='9')  ||  (*src=='_')  ) {
				hash=hash * 147 + *src;
				src++;
			}

			// 根据哈希值, 搜索词汇
			current_id=symbols;
			while(current_id[Token]) { // 这行有值, 再执行
				if(current_id[Hash]==hash) {
					token=current_id[Token];
					return;// 循环结束
				}
				current_id=current_id+IdSize;
			}

			// 没有找到, 新建一行
			current_id[Name]=(long long)last_pos;
			current_id[Hash]=hash;
			token=current_id[Token]=Id;
			return;
		}
		else if( token>='0' && token<='9' ) { // 分析数字
		
			token_val=token - '0';

			// 十进制数字
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
		else if(token=='=') {				  // 分析等于'='
			if(*src=='=') { // 下一个还是'='，那么是'=='.
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
		} else if (token == Id) { //处理Id
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
				} else if (id[Class] == Fun) {
					// function call
					*++text = CALL;
					*++text = id[Value];
				} else {
					printf("%d: bad function call\n", line);
					exit(-1);
				}

				// clean the stack for arguments
				if (tmp > 0) {
					*++text = ADJ;
					*++text = tmp;
				}
				expr_type = id[Type];
			} else if (id[Class] == Num) {
				// enum variable
				*++text = IMM;
				*++text = id[Value];
				expr_type = INT;
			} else {
				// variable
				if (id[Class] == Loc) {
					*++text = LEA;
					//栈往下增长
					*++text = index_of_bp - id[Value];
				} else if (id[Class] == Glo) {
					*++text = IMM;
					*++text = id[Value];
				} else {
					printf("%d: undefined variable\n", line);
					exit(-1);
				}

				// emit code, default behaviour is to load the value of the
				// address which is stored in `ax`
				expr_type = id[Type];
				*++text = (expr_type == Char) ? LC : LI;
			}
		} else {
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
			} else {
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
		//cout<<"wa"<<endl;
		match(';');
	} else {
		expression(Assign);
		//cout<<"wa2"<<endl;
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

	pos_local=index_of_bp;// 初始值为0

	//定义变量
	while(token==Int || token ==Char ) {
		basetype=(token==Int)?INT:CHAR;
		match(token);

		while(token!=';') {
			type=basetype;

			match(Id);

			// current_id为当前变量的行
			current_id[BClass] = current_id[Class];
			current_id[Class]  = Loc;
			current_id[BType]  = current_id[Type];
			current_id[Type]   = type;
			current_id[BValue] = current_id[Value];
			current_id[Value]  = ++pos_local;   // 开辟空间, 实际是栈的相对位置
		}
		match(';');
	}

	*++text=ENT;
	*++text=pos_local-index_of_bp;//分配的尺寸, 根据变量个数来分配空间?

	while(token != '}') {
		statement();
		//cout<<"loop"<<endl;
	}


	*++text=LEV;
	//cout<<"arrived ";

}

void function_parameter() {
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

	// int var;
	//  ^token
	if(token==Int) {
		match(Int);
	}

	// 分析是变量还是函数
	// int var;
	//      ^token
	while(token!=';' && token!='}' ) {
		type=basetype;

		if(token!=Id) {
			printf("%d: Sundy:why not is Id?\n", line);
			//cout<<"token="<<token<<endl;
			exit(-1);
		}
		// int var;
		//        ^token
		match(Id);
		current_id[Type] = type;

		// int fun( ){}
		//        ^
		if(token=='(') {
			current_id[Class]=Fun;
			current_id[Value]=(long long)(text+1);
			function_declaration();
		} else {
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
	//cout<<"arrived2";
}

int main(int argc,char **argv) {

	int i;
	int fd;// 文件句柄
	int *tmp;// 母鸡

	// ...
	argc--;
	argv++;// 要打开的文件名

	// 模式切换.
	// 当输入命令'-s', 调用assembly功能
	if (argc > 0 && **argv == '-' && (*argv)[1] == 's') {
		assembly = 1;
		--argc;
		++argv;
	}
	// 模式切换.
	// 当输入命令'-d', 调用debug功能
	if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') {
		debug = 1;
		--argc;
		++argv;
	}
	// 当用户没有输入文件路径时, 显示提示
	if (argc < 1) {
		printf("usage: xc [-s] [-d] file ...\n");
		return -1;
	}

	// 创建文件句柄
	if ((fd = open(*argv, 0)) < 0) {
		printf("could not open(%s)\n", *argv);
		return -1;
	}

	poolsize = 256 * 1024; // 最大尺寸
	line = 1;

	// 分配空间
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

	// 初始化这一切
	memset(text, 0, poolsize);
	memset(data, 0, poolsize);
	memset(stack, 0, poolsize);
	memset(symbols, 0, poolsize);

	// old_text是text的首地址, text在不停移动
	old_text = text;

	src = "char else enum if int return sizeof while "
	      "open read close printf malloc memset memcmp exit void main";

	// 添加关键字到symbols表中
	i = Char;
	while (i <= While) {
		next();
		current_id[Token] = i++;// 覆盖掉本身的Id类型, 用如:Int,Char等代替
	}

	// 添加另外一组关键字给symbols表, 这组关键字代表着系统函数
	i = OPEN;
	while (i <= EXIT) {// 注意!到exit为止, void main没加入
		next();
		current_id[Class] = Sys;
		current_id[Type] = INT;
		current_id[Value] = i++;
	}

	next();
	current_id[Token] = Char; // void标记为Char. 不懂为何?
	next(); // main
	//  ^token
	idmain = current_id; // 抓住main函数, 当前current_id[Name]='main'

	// src分配空间
	if (!(src = old_src = (char*)malloc(poolsize))) {
		printf("could not malloc(%d) for source area\n", poolsize);
		return -1;
	}
	// fd读到src中
	if ((i = read(fd, src, poolsize-1)) <= 0) {
		printf("read() returned %d\n", i);
		return -1;
	}
	src[i] = 0; // 末尾插入0, next()中的while()才会停止
	close(fd);

	program(); // 分析源代码

	
	
	// -----text-------------------------------
	// 0x309d60 0							  		
	// 0x309d64 6							  
	// 0x309d68 2							  
	// 0x309d6c 0
	// 0x309d70 -1
	// 0x309d74 13
	// 0x309d78 1
	// 0x309d7c 88    
	// 0x309d80 11
	// 0x309d84 0
	// 0x309d88 -2
	// 0x309d8c 13
	// 0x309d90 1
	// 0x309d94 77    
	// 0x309d98 11
	// 0x309d9c 8
	// 0x309da0 6  ----------- main()程序入口↓
	// 0x309da4 1
	// 0x309da8 0
	// 0x309dac -1
	// 0x309db0 13
	// 0x309db4 1
	// 0x309db8 199532
	// 0x309dbc 11
	// ...
	// -----text---------------------------------
	// 从main()函数在text中的位置, 开始执行
	if (!(pc = (int *)idmain[Value])) {
		printf("main() not defined\n");
		return -1;
	}

	// dump_text();暂时母鸡
	if (assembly) {
		// only for compile
		return 0;
	}

	// 不清楚用来干嘛的
	// setup stack
	old_sp = sp = (int *)((long long)stack + poolsize);
	*--sp = EXIT; // call exit if main returns
	*--sp = PUSH;
	tmp = sp;
	*--sp = argc;
	*--sp = (long long)argv;
	*--sp = (long long)tmp;


	eval();

	cout<<endl;
	cout<<"-----text------"<<endl;
	for (int i=0; i<=20 ; i++ ) {
		cout<<&old_text[i]<<" ";
		cout<<old_text[i]<<endl;
	}
	cout<<"-----text------"<<endl<<endl;

	return 0;
}
