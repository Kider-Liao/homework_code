import exceptions.*;
import java_cup.runtime.*;
import java.io.*;
import nonterminal.*;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.ArrayList;

/**
 * 递归下降语法分析器
 */
public class OberonParser {
    /**
     * 符号表列表
     */
    List<HashMap<String, Type>> symbol_list;
    /**
     * if语句层数
     */
    int if_level;
    /**
     * while语句层数
     */
    int while_level;
    /**
     * 当前的符号表位置
     */
    int top;
    /**
     * 词法分析器
     */
    OberonScanner obj;
    /**
     * 结果复杂度
     */
    int res;
    /**
     * 下一个词法单元
     */
    java_cup.runtime.Symbol lookahead;

    /**
     * 初始化函数
     * 
     * @param scanner 对应的词法分析器
     */
    public OberonParser(OberonScanner scanner) {
        obj = scanner;
        if_level = 0;
        while_level = 0;
        symbol_list = new ArrayList<HashMap<String, Type>>();
        HashMap<String, Type> base = new HashMap<String, Type>();
        symbol_list.add(base);
        top = 0;
        res = 0;
    }

    /**
     * 语法分析
     */
    public void parse() throws Exception {
        lookahead = obj.next_token();
        module();
    }

    /**
     * 获得结果
     * 
     * @return 结果复杂度
     */
    public String get_res() {
        return Integer.toString(res);
    }

    public boolean check_type(Type a, Type b) {
        if (a.type.size() != b.type.size()) {
            return false;
        }
        for (int i = 0; i < a.type.size(); i++) {
            String t1 = a.type.get(i);
            String t2 = b.type.get(i);
            if (!t1.equalsIgnoreCase(t2))
                return false;
        }
        if (a.left == true && b.left == false)
            return false;

        if (a.type.get(1).equalsIgnoreCase("array")) {
            return check_type(a.arr_type, b.arr_type);
        } else if (a.type.get(1).equalsIgnoreCase("record")) {
            HashMap<String, Type> map1 = a.symbol_list;
            HashMap<String, Type> map2 = b.symbol_list;
            if (map1.size() != map2.size()) {
                return false;
            }
            Set<String> key = map1.keySet();
            for (String s : key) {
                Type t1 = map1.get(s);
                if (!map2.containsKey(s))
                    return false;
                Type t2 = map2.get(s);
                if (!check_type(t1, t2))
                    return false;
            }
        }
        return true;
    }

    Type select_field(Type type, String select) throws Exception {
        Type pre = type;
        String iden = "";
        for (int i = 0; i < select.length(); i++) {
            if (select.charAt(i) == '.') {
                i++;
                while (i < select.length() && select.charAt(i) != '.' && select.charAt(i) != '[') {
                    iden += select.charAt(i);
                    i++;
                }
                i--;
                if (pre.type.size() > 1 && pre.type.get(1).equalsIgnoreCase("record")
                        && pre.symbol_list.containsKey(iden)) {
                    pre = pre.symbol_list.get(iden);
                    iden = "";
                } else {
                    throw new SemanticException("Identifier hasn't been declared");
                }
            } else if (select.charAt(i) == '[') {
                i++;
                while (i < select.length() && select.charAt(i) != ']') {
                    i++;
                }
                if (pre.type.size() > 1 && pre.type.get(1).equalsIgnoreCase("array")) {
                    pre = pre.arr_type;
                } else {
                    return null;
                }
            }
        }
        return pre;
    }

    /**
     * 匹配操作
     * 
     * @param val 期望的词法单元值
     * @throws Exception 抛出异常
     */
    void match(int val) throws Exception {
        if (lookahead.sym != val) {
            if (val == 35) {
                throw new MissingRightParenthesisException();
            }
            throw new SyntacticException();
        }
        lookahead = obj.next_token();
    }

    void module() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        match(16);
        String id1 = (String) lookahead.value;
        match(44);
        match(37);
        Nonterminal declar = declarations();
        if (lookahead.sym == 17) {
            match(17);
            Nonterminal ss = statement_sequence();
            RESULT.complexity += ss.complexity;
        }
        match(18);
        String id2 = (String) lookahead.value;
        match(44);
        match(36);
        if (!id1.equalsIgnoreCase(id2)) {
            throw new SemanticException("Module name mismatched!");
        }
        RESULT.complexity += declar.complexity;
        res = RESULT.complexity;
    }

    Nonterminal declarations() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        Nonterminal cd = const_declare();
        Nonterminal td = type_declare();
        Nonterminal vd = var_declare();
        Nonterminal pd = procedure_declare();
        RESULT.complexity = cd.complexity + td.complexity + vd.complexity + pd.complexity;
        return RESULT;
    }

    Nonterminal const_declare() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        if (lookahead.sym == 19) {
            match(19);
            Nonterminal ic = identifier_const();
            RESULT.complexity = ic.complexity + 5;
        }
        return RESULT;
    }

    Nonterminal identifier_const() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        if (lookahead.sym == 44) {
            String id = (String) lookahead.value;
            match(44);
            match(2);
            Type e = expression();
            match(37);
            if (symbol_list.get(top).containsKey(id)) {
                throw new SemanticException("Identifier name has been used!");
            }
            symbol_list.get(top).put(id, e);
            Nonterminal ic = identifier_const();
            RESULT.complexity = 1 + e.complexity + ic.complexity;
        }
        return RESULT;
    }

    Nonterminal type_declare() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        if (lookahead.sym == 20) {
            match(20);
            Nonterminal it = identifier_type();
            RESULT.complexity = 10 + it.complexity;
        }
        return RESULT;
    }

    Nonterminal identifier_type() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        if (lookahead.sym == 44) {
            String id = (String) lookahead.value;
            match(44);
            match(2);
            Type t = type();
            match(37);
            t.type.set(0, "type");
            if (symbol_list.get(top).containsKey(id)) {
                throw new SemanticException("Identifier name has been used!");
            }
            symbol_list.get(top).put(id, t);
            Nonterminal it = identifier_type();
            RESULT.complexity = 1 + t.complexity + it.complexity;
        }
        return RESULT;
    }

    Nonterminal var_declare() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        if (lookahead.sym == 21) {
            match(21);
            Nonterminal iv = identifier_var();
            RESULT.complexity = iv.complexity;
        }
        return RESULT;
    }

    Nonterminal identifier_var() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        if (lookahead.sym == 44) {
            Selector il = identifier_list();
            match(38);
            Type t = type();
            match(37);
            String[] i_list = il.value.split(" ");
            /* 定义用于计算的变量 */
            for (int i = 0; i < i_list.length; i++) {
                if (symbol_list.get(top).containsKey(i_list[i])) {
                    throw new SemanticException("Identifier name has been used!");
                }
                symbol_list.get(top).put(i_list[i], t);
            }
            Nonterminal iv = identifier_var();
            RESULT.complexity = il.complexity + t.complexity + iv.complexity;
        }
        return RESULT;
    }

    Nonterminal procedure_declare() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        if (lookahead.sym == 31) {
            Nonterminal pd1 = procedure_declaration();
            match(37);
            Nonterminal pd2 = procedure_declare();
            RESULT.complexity = pd1.complexity + pd2.complexity;
        }
        return RESULT;
    }

    Nonterminal procedure_declaration() throws Exception {
        Nonterminal RESULT = new Nonterminal();
        Procedure ph = procedure_heading();
        match(37);
        Procedure pb = procedure_body();
        if (!ph.name.equalsIgnoreCase(pb.name)) {
            throw new SemanticException("Procedure name isn't matched");
        }
        RESULT.complexity = ph.complexity + pb.complexity;
        return RESULT;
    }

    Nonterminal statement_sequence() throws Exception {
        Nonterminal RESULT;
        Nonterminal s = statement();
        Nonterminal os = optional_statement();
        Nonterminal pre = new Nonterminal();
        pre.complexity = s.complexity + os.complexity;
        RESULT = pre;
        return RESULT;
    }

    Nonterminal optional_statement() throws Exception {
        Nonterminal RESULT;
        if (lookahead.sym == 37) {
            match(37);
            Nonterminal s = statement();
            Nonterminal os = optional_statement();
            Nonterminal pre = new Nonterminal();
            pre.complexity = s.complexity + os.complexity;
            RESULT = pre;
        } else {
            Nonterminal pre = new Nonterminal();
            pre.complexity = 0;
            RESULT = pre;
        }
        return RESULT;
    }

    Nonterminal statement() throws Exception {
        Nonterminal RESULT = null;
        if (lookahead.sym == 44) {
            String id = (String) lookahead.value;
            if (id.equalsIgnoreCase("read") || id.equalsIgnoreCase("write") || id.equalsIgnoreCase("writeln")) {
                Nonterminal p = procedure_call();
                Nonterminal pre = new Nonterminal();
                pre.complexity = p.complexity * (if_level + 1) * (int) Math.pow(2, while_level);
                RESULT = pre;
            } else {
                Type t = null;
                for (int i = top; i >= 0; i--) {
                    if (symbol_list.get(i).containsKey(id)) {
                        t = symbol_list.get(i).get(id);
                    }
                }
                if (t == null) {
                    throw new SemanticException("Identifier hasn't been declared");
                } else if (t.type.get(0).equalsIgnoreCase("procedure")) {
                    Nonterminal p = procedure_call();
                    Nonterminal pre = new Nonterminal();
                    pre.complexity = p.complexity * (if_level + 1) * (int) Math.pow(2, while_level);
                    RESULT = pre;
                } else {
                    Nonterminal a = assignment();
                    Nonterminal pre = new Nonterminal();
                    pre.complexity = a.complexity * (if_level + 1) * (int) Math.pow(2, while_level);
                    RESULT = pre;
                }
            }
        } else if (lookahead.sym == 27) {
            Nonterminal is = if_statement();
            Nonterminal pre = new Nonterminal();
            pre.complexity = is.complexity;
            RESULT = pre;
        } else if (lookahead.sym == 25) {
            Nonterminal ws = while_statement();
            Nonterminal pre = new Nonterminal();
            pre.complexity = ws.complexity;
            RESULT = pre;
        } else {
            Nonterminal pre = new Nonterminal();
            pre.complexity = 0;
            RESULT = pre;
        }
        return RESULT;
    }

    Nonterminal while_statement() throws Exception {
        Nonterminal RESULT;
        match(25);
        while_level += 1;
        Type e = expression();
        match(26);
        Nonterminal ss = statement_sequence();
        match(18);
        if (!e.type.get(1).equalsIgnoreCase("boolean")) {
            throw new TypeMismatchedException();
        }
        RESULT = new Nonterminal();
        RESULT.complexity += e.complexity * (if_level + 1) * (int) Math.pow(2, while_level) + ss.complexity;
        while_level -= 1;
        return RESULT;
    }

    Nonterminal if_statement() throws Exception {
        Nonterminal RESULT;
        match(27);
        if_level += 1;
        Type e = expression();
        match(28);
        Nonterminal ss = statement_sequence();
        Nonterminal els = elsif_statement();
        Nonterminal es = else_statement();
        match(18);
        RESULT = new Nonterminal();
        if (!e.type.get(1).equalsIgnoreCase("boolean")) {
            throw new TypeMismatchedException();
        }
        RESULT.complexity += e.complexity * (if_level + 1) * (int) Math.pow(2, while_level) + ss.complexity
                + els.complexity + es.complexity;
        if_level -= 1;
        return RESULT;
    }

    Nonterminal elsif_statement() throws Exception {
        Nonterminal RESULT;
        if (lookahead.sym == 29) {
            match(29);
            Type e = expression();
            match(28);
            Nonterminal ss = statement_sequence();
            Nonterminal els = elsif_statement();
            RESULT = new Nonterminal();
            if (!e.type.get(1).equalsIgnoreCase("boolean")) {
                throw new TypeMismatchedException();
            }
            RESULT.complexity = e.complexity * (if_level + 1) * (int) Math.pow(2, while_level) + ss.complexity
                    + els.complexity;
        } else {
            RESULT = new Nonterminal();
            RESULT.complexity = 0;
        }
        return RESULT;
    }

    Nonterminal else_statement() throws Exception {
        Nonterminal RESULT;
        if (lookahead.sym == 30) {
            match(30);
            Nonterminal ss = statement_sequence();
            RESULT = new Nonterminal();
            RESULT.complexity = ss.complexity;
        } else {
            RESULT = new Nonterminal();
            RESULT.complexity = 0;
        }
        return RESULT;
    }

    Nonterminal procedure_call() throws Exception {
        Nonterminal RESULT;
        String id = (String) lookahead.value;
        match(44);
        if (lookahead.sym == 34) {
            Parameter ap = actual_parameters();
            if (id.equalsIgnoreCase("read") || id.equalsIgnoreCase("write") || id.equalsIgnoreCase("writeln")) {
                Nonterminal pre = new Nonterminal();
                pre.complexity = 8 + ap.complexity;
                RESULT = pre;
            } else {
                Nonterminal pre = new Nonterminal();
                int pos = -1;
                for (int i = top; i >= 0; i--) {
                    if (symbol_list.get(i).containsKey(id)) {
                        pos = i;
                        break;
                    }
                }
                if (pos == -1) {
                    throw new SemanticException("Identifier isn't been declared!");
                }
                Type t = symbol_list.get(pos).get(id);
                if (!t.type.get(0).equalsIgnoreCase("procedure")) {
                    throw new TypeMismatchedException();
                }
                List<Type> para1 = t.para;
                List<Type> para2 = ap.para;
                if (para1.size() != para2.size()) {
                    throw new ParameterMismatchedException();
                }
                for (int i = 0; i < para1.size(); i++) {
                    if (!check_type(para1.get(i), para2.get(i))) {
                        throw new TypeMismatchedException();
                    }
                }
                pre.complexity = 8 + ap.complexity;
                RESULT = pre;
            }
        } else if (lookahead.sym == 44 || lookahead.sym == 15 || lookahead.sym == 43) {
            throw new MissingLeftParenthesisException();
        } else {
            if (id.equalsIgnoreCase("read") || id.equalsIgnoreCase("write") || id.equalsIgnoreCase("writeln")) {
                Nonterminal pre = new Nonterminal();
                pre.complexity = 8;
                RESULT = pre;
            } else {
                Nonterminal pre = new Nonterminal();
                int pos = -1;
                for (int i = top; i >= 0; i--) {
                    if (symbol_list.get(i).containsKey(id)) {
                        pos = i;
                        break;
                    }
                }
                if (pos == -1) {
                    throw new SemanticException("Identifier isn't been declared!");
                }
                Type t = symbol_list.get(pos).get(id);
                if (!t.type.get(0).equalsIgnoreCase("procedure")) {
                    throw new TypeMismatchedException();
                }
                pre.complexity = 8;
                RESULT = pre;
            }
        }
        return RESULT;
    }

    Nonterminal assignment() throws Exception {
        Nonterminal RESULT;
        String id = (String) lookahead.value;
        match(44);
        Selector s = selector();
        match(40);
        Type e = expression();
        int pos = -1;
        for (int i = top; i >= 0; i--) {
            if (symbol_list.get(i).containsKey(id)) {
                pos = i;
                break;
            }
        }
        if (pos == -1) {
            throw new SemanticException("Identifier isn't been declared!");
        }
        Type pre = symbol_list.get(pos).get(id);
        String select = s.value;
        /* 检查对应的selector是否对应正确的type */
        Type after = select_field(pre, select);
        if (after == null || !check_type(after, e)) {
            throw new TypeMismatchedException();
        }
        RESULT = new Nonterminal();
        RESULT.complexity = s.complexity + e.complexity + 2;
        return RESULT;
    }

    Nonterminal sign() throws Exception {
        Nonterminal RESULT;
        if (lookahead.sym == 8) {
            match(8);
            RESULT = new Nonterminal();
            RESULT.complexity = 2;
        } else if (lookahead.sym == 9) {
            match(9);
            RESULT = new Nonterminal();
            RESULT.complexity = 2;
        } else {
            RESULT = new Nonterminal();
            RESULT.complexity = 0;
        }
        return RESULT;
    }

    Procedure procedure_heading() throws Exception {
        Procedure RESULT = new Procedure();
        match(31);
        String id = (String) lookahead.value;
        match(44);
        if (lookahead.sym == 34) {
            Parameter fp = formal_Parameters();
            Type pre = new Type();
            pre.type = new ArrayList<String>();
            pre.type.add("procedure");
            pre.para = fp.para;
            if (symbol_list.get(top).containsKey(id)) {
                throw new SemanticException("Identifier name has been used!");
            }
            symbol_list.get(top).put(id, pre);
            // System.out.println("succeed!");
            HashMap<String, Type> base = new HashMap<String, Type>();
            symbol_list.add(base);
            top = top + 1;
            RESULT.name = id;
            RESULT.complexity = 20 + fp.complexity;
            /* 将参数列表也插入符号表中 */
            int num = fp.para.size();
            for (int i = 0; i < num; i++) {
                String name = fp.var.get(i);
                Type t = fp.para.get(i);
                if (symbol_list.get(top).containsKey(name)) {
                    throw new SemanticException("Identifier name has been used!");
                }
                symbol_list.get(top).put(name, t);
            }
        } else if (lookahead.sym == 44 || lookahead.sym == 21 || lookahead.sym == 35) {
            // 缺失左括号
            throw new MissingLeftParenthesisException();
        } else {
            RESULT = new Procedure();
            /* 将过程插入符号表 */
            Type pre = new Type();
            pre.type = new ArrayList<String>();
            pre.para = new ArrayList<Type>();
            pre.type.add("procedure");
            if (symbol_list.get(top).containsKey(id)) {
                throw new SemanticException("Identifier name has been used!");
            }
            symbol_list.get(top).put(id, pre);
            // System.out.println("succeed!");
            HashMap<String, Type> base = new HashMap<String, Type>();
            symbol_list.add(base);
            top = top + 1;
            RESULT.name = id;
            RESULT.complexity = 20;
        }
        return RESULT;
    }

    Procedure procedure_body() throws Exception {
        Procedure RESULT = new Procedure();
        Nonterminal declar = declarations();
        if (lookahead.sym == 17) {
            match(17);
            Nonterminal ss = statement_sequence();
            RESULT.complexity += ss.complexity;
        }
        match(18);
        String id = (String) lookahead.value;
        match(44);
        symbol_list.remove(top);
        top = top - 1;
        RESULT.name = id;
        RESULT.complexity += declar.complexity;
        return RESULT;
    }

    Parameter formal_Parameters() throws Exception {
        Parameter RESULT = new Parameter();
        match(34);
        if (lookahead.sym == 44 || lookahead.sym == 21) {
            Parameter fs = fp_section();
            Parameter ofp = optional_fp();
            match(35);
            RESULT.complexity = fs.complexity + ofp.complexity;
            /* 连接对应的参数表,首先检查是否有重复变量名 */
            for (int i = 0; i < fs.var.size(); i++) {
                String pre = fs.var.get(i);
                if (ofp.var.contains(pre)) {
                    throw new SemanticException("Identifier name has been used!");
                }
            }
            fs.var.addAll(ofp.var);
            fs.para.addAll(ofp.para);
            RESULT.var = fs.var;
            RESULT.para = fs.para;
        } else {
            match(35);
            RESULT.para = new ArrayList<Type>();
            RESULT.var = new ArrayList<String>();
        }
        return RESULT;
    }

    Parameter optional_fp() throws Exception {
        Parameter RESULT = new Parameter();
        if (lookahead.sym == 37) {
            match(37);
            Parameter fs = fp_section();
            Parameter ofp = optional_fp();
            RESULT.complexity = fs.complexity + ofp.complexity;
            /* 连接对应的参数表,首先检查是否有重复变量名 */
            for (int i = 0; i < fs.var.size(); i++) {
                String pre = fs.var.get(i);
                if (ofp.var.contains(pre)) {
                    throw new SemanticException("Identifier name has been used!");
                }
            }
            fs.var.addAll(ofp.var);
            fs.para.addAll(ofp.para);
            RESULT.var = fs.var;
            RESULT.para = fs.para;
        } else {
            RESULT.para = new ArrayList<Type>();
            RESULT.var = new ArrayList<String>();
        }
        return RESULT;
    }

    Parameter fp_section() throws Exception {
        Parameter RESULT = new Parameter();
        if (lookahead.sym == 21) {
            match(21);
            Selector il = identifier_list();
            match(38);
            Type t = type();
            RESULT.para = new ArrayList<Type>();
            RESULT.var = new ArrayList<String>();
            t.left = true;
            String[] i_list = il.value.split(" ");
            for (int i = 0; i < i_list.length; i++) {
                if (!RESULT.var.contains(i_list[i])) {
                    RESULT.para.add(t);
                    RESULT.var.add(i_list[i]);
                } else {
                    throw new SemanticException("Identifier name has been used!");
                }
            }
            RESULT.complexity = il.complexity + t.complexity;
        } else {
            Selector il = identifier_list();
            match(38);
            Type t = type();
            RESULT.para = new ArrayList<Type>();
            RESULT.var = new ArrayList<String>();
            String[] i_list = il.value.split(" ");
            for (int i = 0; i < i_list.length; i++) {
                if (!RESULT.var.contains(i_list[i])) {
                    RESULT.para.add(t);
                    RESULT.var.add(i_list[i]);
                } else {
                    throw new SemanticException("Identifier name has been used!");
                }
            }
            RESULT.complexity = il.complexity + t.complexity;
        }
        return RESULT;
    }

    Parameter actual_parameters() throws Exception {
        Parameter RESULT = null;
        match(34);
        if (lookahead.sym == 35) {
            match(35);
            RESULT = new Parameter();
            RESULT.complexity = 0;
            RESULT.para = new ArrayList<Type>();
        } else if (lookahead.sym == 44 || lookahead.sym == 15 || lookahead.sym == 43) {
            Type e = expression();
            Parameter oe = optional_expression();
            match(35);
            RESULT = new Parameter();
            RESULT.complexity = e.complexity + oe.complexity;
            RESULT.para = new ArrayList<Type>();
            RESULT.para.add(e);
            RESULT.para.addAll(oe.para);
        } else {
            throw new MissingRightParenthesisException();
        }
        return RESULT;
    }

    Parameter optional_expression() throws Exception {
        Parameter RESULT;
        if (lookahead.sym == 39) {
            match(39);
            Type e = expression();
            Parameter oe = optional_expression();
            RESULT = new Parameter();
            RESULT.complexity = e.complexity + oe.complexity;
            RESULT.para = new ArrayList<Type>();
            RESULT.para.add(e);
            RESULT.para.addAll(oe.para);
        } else {
            RESULT = new Parameter();
            RESULT.complexity = 0;
            RESULT.para = new ArrayList<Type>();
        }
        return RESULT;
    }

    Type type() throws Exception {
        Type RESULT;
        if (lookahead.sym == 44) {
            String id = (String) lookahead.value;
            match(44);
            int pos = -1;
            for (int i = top; i >= 0; i--) {
                if (symbol_list.get(i).containsKey(id)) {
                    pos = i;
                    break;
                }
            }
            if (pos == -1) {
                throw new SemanticException("Identifier hasn't been declared");
            }
            Type pre = symbol_list.get(pos).get(id);
            if (!pre.type.get(0).equalsIgnoreCase("type")) {
                throw new SemanticException("This identifier isn't a type");
            }
            Type p = new Type();
            p.type = new ArrayList<String>();
            p.type.add("calculate");
            p.type.add(pre.type.get(1));
            p.symbol_list = pre.symbol_list;
            p.arr_type = pre.arr_type;
            p.para = pre.para;
            RESULT = p;
            RESULT.complexity = 2;
        } else if (lookahead.sym == 23) {
            Arr at = array_type();
            Type pre = new Type();
            pre.type = new ArrayList<String>();
            pre.arr_type = at.base_type;
            pre.type.add("calculate");
            pre.type.add("array");
            pre.complexity = at.complexity + 8;
            RESULT = pre;
        } else if (lookahead.sym == 22) {
            Rec rt = record_type();
            Type pre = new Type();
            pre.type = new ArrayList<String>();
            pre.type.add("calculate");
            pre.type.add("record");
            pre.symbol_list = rt.symbol_list;
            pre.complexity = rt.complexity + 3;
            RESULT = pre;
        } else if (lookahead.sym == 32) {
            match(32);
            Type pre = new Type();
            pre.type = new ArrayList<String>();
            pre.type.add("calculate");
            pre.type.add("integer");
            pre.complexity = 1;
            RESULT = pre;
        } else {
            match(33);
            Type pre = new Type();
            pre.type = new ArrayList<String>();
            pre.type.add("calculate");
            pre.type.add("boolean");
            pre.complexity = 1;
            RESULT = pre;
        }
        return RESULT;
    }

    Type expression() throws Exception {
        Type RESULT;
        Type se1 = simple_expression();
        if (lookahead.sym == 2 || lookahead.sym == 3 || lookahead.sym == 4 || lookahead.sym == 5 || lookahead.sym == 6
                || lookahead.sym == 7) {
            match(lookahead.sym);
            Type se2 = simple_expression();
            RESULT = new Type();
            if (!se1.type.get(1).equalsIgnoreCase("integer") || !se2.type.get(1).equalsIgnoreCase("integer")) {
                throw new TypeMismatchedException();
            }
            RESULT.complexity = se1.complexity + se2.complexity + 4;
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add("boolean");
        } else if (lookahead.sym == 44 || lookahead.sym == 15 || lookahead.sym == 43 || lookahead.sym == 34) {
            throw new MissingOperatorException();
        } else {
            RESULT = se1;
        }
        return RESULT;
    }

    Type simple_expression() throws Exception {
        Type RESULT;
        Nonterminal s = sign();
        Type t = term();
        Type ot = optional_term();
        if (ot.type.size() == 0) {
            RESULT = t;
            RESULT.complexity += s.complexity;
        } else {
            RESULT = new Type();
            if (ot.type.size() > 1 && !t.type.get(1).equalsIgnoreCase(ot.type.get(1))) {
                throw new TypeMismatchedException();
            }
            RESULT.complexity = t.complexity + ot.complexity + s.complexity;
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add(t.type.get(1));
        }
        return RESULT;
    }

    Type optional_term() throws Exception {
        Type RESULT;
        if (lookahead.sym == 8 || lookahead.sym == 9) {
            match(lookahead.sym);
            Type t = term();
            Type ot = optional_term();
            RESULT = new Type();
            if (!t.type.get(1).equalsIgnoreCase("integer")) {
                throw new TypeMismatchedException();
            }
            if (ot.type.size() > 1 && !ot.type.get(1).equalsIgnoreCase("integer")) {
                throw new TypeMismatchedException();
            }
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add("integer");
            RESULT.complexity = t.complexity + ot.complexity + 2;
        } else if (lookahead.sym == 10) {
            match(10);
            Type t = term();
            Type ot = optional_term();
            RESULT = new Type();
            if (!t.type.get(1).equalsIgnoreCase("boolean")) {
                throw new TypeMismatchedException();
            }
            if (ot.type.size() > 1 && !ot.type.get(1).equalsIgnoreCase("boolean")) {
                throw new TypeMismatchedException();
            }
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add("boolean");
            RESULT.complexity = t.complexity + ot.complexity + 6;
        } else {
            RESULT = new Type();
            RESULT.type = new ArrayList<String>();
            RESULT.complexity = 0;
        }
        return RESULT;
    }

    Type term() throws Exception {
        Type RESULT;
        Type f = factor();
        Type of = optional_factor();
        if (of.type.size() == 0) {
            RESULT = f;
        } else {
            RESULT = new Type();
            if (of.type.size() > 1 && !f.type.get(1).equalsIgnoreCase(of.type.get(1))) {
                throw new TypeMismatchedException();
            }
            RESULT.complexity = f.complexity + of.complexity;
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add(f.type.get(1));
        }
        return RESULT;
    }

    Type optional_factor() throws Exception {
        Type RESULT;
        if (lookahead.sym == 11 || lookahead.sym == 12 || lookahead.sym == 13) {
            match(lookahead.sym);
            Type f = factor();
            Type of = optional_factor();
            RESULT = new Type();
            if (!f.type.get(1).equalsIgnoreCase("integer")) {
                throw new TypeMismatchedException();
            }
            if (of.type.size() > 1 && !of.type.get(1).equalsIgnoreCase("integer")) {
                throw new TypeMismatchedException();
            }
            RESULT.complexity = f.complexity + of.complexity + 4;
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add("integer");
        } else if (lookahead.sym == 14) {
            match(14);
            Type f = factor();
            Type of = optional_factor();
            RESULT = new Type();
            if (!f.type.get(1).equalsIgnoreCase("boolean")) {
                throw new TypeMismatchedException();
            }
            if (of.type.size() > 1 && !of.type.get(1).equalsIgnoreCase("boolean")) {
                throw new TypeMismatchedException();
            }
            RESULT.complexity = f.complexity + of.complexity + 6;
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add("boolean");
        } else {
            RESULT = new Type();
            RESULT.type = new ArrayList<String>();
            RESULT.complexity = 0;
        }
        return RESULT;
    }

    Type factor() throws Exception {
        Type RESULT = null;
        if (lookahead.sym == 44) {
            String id = (String) lookahead.value;
            match(44);
            Selector s = selector();
            int pos = -1;
            for (int i = top; i >= 0; i--) {
                if (symbol_list.get(i).containsKey(id)) {
                    pos = i;
                    break;
                }
            }
            if (pos == -1) {
                throw new SemanticException("The identifier hasn't been declared");
            }
            Type pre = symbol_list.get(pos).get(id);
            if (pre.type.get(0).equalsIgnoreCase("procedure")) {
                throw new TypeMismatchedException();
            }
            RESULT = new Type();
            Type after = select_field(pre, s.value);
            if (after == null) {
                throw new TypeMismatchedException();
            }
            RESULT.complexity = s.complexity;
            RESULT.type = after.type;
            RESULT.symbol_list = after.symbol_list;
            RESULT.arr_type = after.arr_type;
            RESULT.para = after.para;
            RESULT.left = true;
        } else if (lookahead.sym == 43) {
            match(43);
            RESULT = new Type();
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add("integer");
            RESULT.complexity = 0;
        } else if (lookahead.sym == 34) {
            match(34);
            if (lookahead.sym == 35) {
                throw new MissingOperandException();
            }
            Type e = expression();
            match(35);
            RESULT = e;
            RESULT.complexity += 6;
        } else if (lookahead.sym == 15) {
            match(15);
            Type f = factor();
            if (!f.type.get(1).equalsIgnoreCase("boolean")) {
                throw new TypeMismatchedException();
            }
            RESULT = new Type();
            RESULT.type = new ArrayList<String>();
            RESULT.type.add("calculate");
            RESULT.type.add("boolean");
            RESULT.complexity = f.complexity + 6;
        } else {
            throw new MissingOperandException();
        }
        return RESULT;
    }

    Rec record_type() throws Exception {
        Rec RESULT;
        match(22);
        Rec fl = field_list();
        Rec opf = optional_field();
        match(18);
        Rec pre = new Rec();
        HashMap<String, Type> list1 = fl.symbol_list;
        HashMap<String, Type> list2 = opf.symbol_list;
        /* 查重 */
        Set<String> key = list1.keySet();

        for (String s : key) {
            if (list2.containsKey(s)) {
                throw new SemanticException("Identifier name has been used!");
            }
        }
        list1.putAll(list2);
        pre.symbol_list = list1;
        pre.complexity = fl.complexity + opf.complexity;
        RESULT = pre;
        return RESULT;
    }

    Rec optional_field() throws Exception {
        Rec RESULT;
        if (lookahead.sym == 37) {
            match(37);
            Rec fl = field_list();
            Rec opf = optional_field();
            Rec pre = new Rec();
            HashMap<String, Type> list1 = fl.symbol_list;
            HashMap<String, Type> list2 = opf.symbol_list;
            /* 查重 */
            Set<String> key = list1.keySet();
            for (String s : key) {
                if (list2.containsKey(s)) {
                    throw new SemanticException("Identifier name has been used!");
                }
            }
            list1.putAll(list2);
            pre.symbol_list = list1;
            pre.complexity = fl.complexity + opf.complexity;
            RESULT = pre;
        } else {
            Rec pre = new Rec();
            pre.symbol_list = new HashMap<String, Type>();
            pre.complexity = 0;
            RESULT = pre;
        }
        return RESULT;
    }

    Rec field_list() throws Exception {
        Rec RESULT;
        if (lookahead.sym == 44) {
            Selector il = identifier_list();
            match(38);
            Type t = type();
            Rec pre = new Rec();
            pre.symbol_list = new HashMap<String, Type>();
            String[] i_list = il.value.split(" ");
            for (int i = 0; i < i_list.length; i++) {
                if (!pre.symbol_list.containsKey(i_list[i])) {
                    pre.symbol_list.put(i_list[i], t);
                } else {
                    throw new SemanticException("Identifier name has been used!");
                }
            }
            pre.complexity = il.complexity + t.complexity;
            RESULT = pre;
        } else {
            Rec pre = new Rec();
            pre.symbol_list = new HashMap<String, Type>();
            pre.complexity = 0;
            RESULT = pre;
        }
        return RESULT;
    }

    Arr array_type() throws Exception {
        Arr RESULT;
        match(23);
        Type e = expression();
        match(24);
        Type t = type();
        Arr pre = new Arr();
        if (!e.type.get(0).equalsIgnoreCase("calculate") || !e.type.get(1).equalsIgnoreCase("integer")) {
            throw new TypeMismatchedException();
        }
        pre.base_type = t;
        pre.complexity = e.complexity + t.complexity;
        RESULT = pre;
        return RESULT;
    }

    Selector identifier_list() throws Exception {
        Selector RESULT;
        String id = (String) lookahead.value;
        match(44);
        Selector oi = optional_identifier();
        String pre = id + " " + oi.value;
        oi.value = pre;
        RESULT = oi;
        RESULT.complexity += 1;
        return RESULT;
    }

    Selector optional_identifier() throws Exception {
        Selector RESULT;
        if (lookahead.sym == 39) {
            match(39);
            String id = (String) lookahead.value;
            match(44);
            Selector oi = optional_identifier();
            String pre = id + " " + oi.value;
            oi.value = pre;
            RESULT = oi;
            RESULT.complexity += 1;
        } else {
            Selector pre = new Selector();
            pre.complexity = 0;
            pre.value = "";
            RESULT = pre;
        }
        return RESULT;
    }

    Selector selector() throws Exception {
        Selector RESULT;
        if (lookahead.sym == 36) {
            match(36);
            String id = (String) lookahead.value;
            match(44);
            Selector s = selector();
            s.value = "." + id + s.value;
            RESULT = s;
            RESULT.complexity += 2;
        } else if (lookahead.sym == 41) {
            match(41);
            Type e = expression();
            match(42);
            Selector s = selector();
            if (!e.type.get(1).equalsIgnoreCase("integer")) {
                throw new TypeMismatchedException();
            }
            s.value = "[]" + s.value;
            RESULT = s;
            RESULT.complexity += 2 + e.complexity;
        } else {
            RESULT = new Selector();
            RESULT.value = "";
            RESULT.complexity = 0;
        }
        return RESULT;
    }
}
