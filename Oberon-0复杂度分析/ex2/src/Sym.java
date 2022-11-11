/**
 * 词法分析符号表
 */
public class Sym {
  public static final int EOF = 0;
  public static final int MODULE = 1;
  public static final int BEGIN = 2;
  public static final int END = 3;
  public static final int CONST = 4;
  public static final int TYPE = 5;
  public static final int VAR = 6;
  public static final int RECORD = 7;
  public static final int ARRAY = 8;
  public static final int OF = 9;
  public static final int WHILE = 10;
  public static final int DO = 11;
  public static final int IF = 12;
  public static final int THEN = 13;
  public static final int ELSIF = 14;
  public static final int ELSE = 15;
  public static final int PROCEDURE = 16;
  public static final int EQUAL = 17;
  public static final int NEQUAL = 18;
  public static final int LESS = 19;
  public static final int ELESS = 20;
  public static final int MORE = 21;
  public static final int EMORE = 22;
  public static final int ADD = 23;
  public static final int MINUS = 24;
  public static final int OR = 25;
  public static final int MUL = 26;
  public static final int DIV = 27;
  public static final int MOD = 28;
  public static final int AND = 29;
  public static final int NOT = 30;
  public static final int LPATH = 31;
  public static final int RPATH = 32;
  public static final int DOT = 33;
  public static final int SEMI = 34;
  public static final int COLON = 35;
  public static final int COMMA = 36;
  public static final int ASSIGN = 37;
  public static final int LBRACK = 38;
  public static final int RBRACK = 39;
  public static final int DIGIT = 40;
  public static final int IDENTIFIER = 41;
  /**
   * 登记对应的符号名
   */
  public static final String[] terminalNames = new String[] {
      "EOF",
      "MODULE",
      "BEGIN",
      "END",
      "CONST",
      "TYPE",
      "VAR",
      "RECORD",
      "ARRAY",
      "OF",
      "WHILE",
      "DO",
      "IF",
      "THEN",
      "ELIF",
      "ELSE",
      "PROCEDURE",
      "EQUAL",
      "NEQUAL",
      "LESS",
      "ELESS",
      "MORE",
      "EMORE",
      "ADD",
      "MINUS",
      "OR",
      "MUL",
      "DIV",
      "MOD",
      "AND",
      "NOT",
      "LPATH",
      "RPATH",
      "DOT",
      "SEMI",
      "COLON",
      "COMMA",
      "ASSIGN",
      "LBRACK",
      "RBRACK",
      "DIGIT",
      "IDENTIFIER"
  };
}
