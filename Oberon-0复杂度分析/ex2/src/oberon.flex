import exceptions.*;
import java_cup.runtime.*;
import java.io.*;

%%


%public 
/*声明的类是public*/
%class OberonScanner 
/*声明的类的名称*/
%cupsym Sym 
/*采用的字符集*/
%cup 
/*运行预定义的指令*/
%unicode 
/*采用UTF-8的字符集*/
%ignorecase 
/*忽略大小写*/
%eofval{
    return CreateSYM(Sym.EOF);
%eofval}
/*文件末尾的返回情况*/
%yylexthrow LexicalException 
/*抛出词法错误的异常*/
%line
%column


%{
int get_line(){return yyline;}
int get_column() {return yycolumn;}


private java_cup.runtime.Symbol CreateSYM(int type)
{
    return new java_cup.runtime.Symbol(type);
}
private java_cup.runtime.Symbol CreateSYM(int type, Object value)
{
    return new java_cup.runtime.Symbol(type, value);
}
%}

WhiteSpace 	= " "|\t|\b|\f|\r|\n|\r\n
Identifier		= [:letter:]+([:letter:]|[:digit:])*
Constant = [1-9][0-9]* | 0 [0-7]*
Comment = "(*" [^*] ~"*)" | "(*" "*"+ ")"
IllegalConstant = [:digit:]+[:letter:]+
IllegalOctal = 0[0-7]*[89]
IllegalComment = "(*"

%%


<YYINITIAL>
{
	"MODULE"	{return CreateSYM(Sym.MODULE); }
	"BEGIN"		{return CreateSYM(Sym.BEGIN); }
	"END"		{return CreateSYM(Sym.END); }
	"CONST"		{return CreateSYM(Sym.CONST); }
	"TYPE"	{return CreateSYM(Sym.TYPE); }
    "VAR"	{return CreateSYM(Sym.VAR); }
    "RECORD"         {return CreateSYM(Sym.RECORD); } 
    "ARRAY"		    {return CreateSYM(Sym.ARRAY); }    
    "OF"         {return CreateSYM(Sym.OF); }  
    "WHILE"		    {return CreateSYM(Sym.WHILE); }
    "DO"		    {return CreateSYM(Sym.DO); }
    "IF"	{return CreateSYM(Sym.IF); }
	"THEN"		{return CreateSYM(Sym.THEN); }
	"ELSIF"		{return CreateSYM(Sym.ELSIF); }
	"ELSE"		{return CreateSYM(Sym.ELSE); }
	"PROCEDURE"	{return CreateSYM(Sym.PROCEDURE); }
    "="	{return CreateSYM(Sym.EQUAL); }
    "#"         {return CreateSYM(Sym.NEQUAL); } 
    "<"		    {return CreateSYM(Sym.LESS); }    
    "<="         {return CreateSYM(Sym.ELESS); }  
    ">"		    {return CreateSYM(Sym.MORE); }
    ">="		    {return CreateSYM(Sym.EMORE); }
    "+"	{return CreateSYM(Sym.ADD); }
	"-"		{return CreateSYM(Sym.MINUS); }
	"OR"		{return CreateSYM(Sym.OR); }
	"*"		{return CreateSYM(Sym.MUL); }
	"DIV"	{return CreateSYM(Sym.DIV); }
    "MOD"	{return CreateSYM(Sym.MOD); }
    "&"         {return CreateSYM(Sym.AND); } 
    "~"		    {return CreateSYM(Sym.NOT); }    
    "("         {return CreateSYM(Sym.LPATH); }  
    ")"		    {return CreateSYM(Sym.RPATH); }
    "."		    {return CreateSYM(Sym.DOT); }
    ";"	{return CreateSYM(Sym.SEMI); }
	":"		{return CreateSYM(Sym.COLON); }
	","		{return CreateSYM(Sym.COMMA); }
	":="		{return CreateSYM(Sym.ASSIGN); }
	"["	{return CreateSYM(Sym.LBRACK); }
    "]"	{return CreateSYM(Sym.RBRACK); }
    {WhiteSpace} {}
    {Comment} {}
    {Identifier} {
        if (yylength() > 24)
            throw new IllegalIdentifierLengthException(yytext());
        return CreateSYM(Sym.IDENTIFIER, yytext());
    }
    {Constant} {
        if (yylength() > 12)
            throw new IllegalIntegerRangeException(yytext());
        return CreateSYM(Sym.DIGIT, yytext());
    }
    {IllegalConstant} {
        throw new IllegalIntegerException(yytext());
    }
    {IllegalOctal} {
        throw new IllegalOctalException(yytext());
    }
    {IllegalComment} {
        throw new MismatchedCommentException(yytext());
    }
}

[^] { throw new IllegalSymbolException(yytext()); }