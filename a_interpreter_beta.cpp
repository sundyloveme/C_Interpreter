#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char *src;//指向源文件

//text

int basetype;

//存放词汇的一张表
int *current_id;
int *symbols;
//词汇表的列
enum {Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize};
int token_val;//数值

int token;//标记 关于如下enum.
//token的标记有如下：
enum {
    Num = 128, Fun, Sys, Glo, Loc, Id,
    Char, Else, Enum, If, Int, Return, Sizeof, While,
    Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

enum { CHAR, INT, PTR };

// 词法分析器
// 传参给token
int next() {
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
            if(token_val>0){
                while(*src>='0' && *src<='9' ){
                    token_val = token_val * 10 +
                        (*src-'0');
                        src++;
                }
            }

            token=Num; return;
        }


        //分析等于'='
        else if(token=='='){
            if(*src=='='){//下一个还是'='，那么是'=='.
                src++;
                token=Eq;
            }
            else{
                token=Assign;
            }
            return;
        }

    }


}

int match(int tk){
    if(token==tk){
        next();
    }
    else{
        printf("%d: Sundy:expected token: %d\n", line, tk);
        exit(-1);
    }
}

int function_body(){
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
    while(token==Int || token ==Char ){
        basetype=(token==Int)?INT:CHAR;
        match(token);

        while(token!=';'){
            type=basetype;

           match(Id);


        }

    }



}

int function_declaration(){
    // type func_name (...) {...}
    //              ->|         |<-
    //              this part
    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();

}

int global_declaration(){
     // int [*]id [; | (...) {...}]

     int type;
     int i;

     basetype = INT;

     if(token==Int){
        match(Int);
     }

     //分析是变量还是函数
     while(token!=';' && token!='}' ){
        type=basetype;

        if(token!=Id){
            printf("%d: 为什么不是Id呢？\n", line);
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;

        //后有'('显然是函数
        if(token=='('){
            current_id[Class]=Fun;
            current_id[Value]=(long long)(text+1);
            function_declaration();
        }
        else
        {
            //对变量进行处理
        }
     }

     next();
}

int program(){
    next();
    while(token>0){
        global_declaration();
    }

}


int main() {
    //...
    //int char 等关键字入 symbols.
    return 0;
}
