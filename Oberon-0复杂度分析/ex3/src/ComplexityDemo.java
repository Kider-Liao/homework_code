import complexity.Calculator;
import complexity.ComplexityTester;
import exceptions.*;
import java_cup.runtime.*;
import java.io.*;
import nonterminal.*;
import java.util.HashMap;
import java.util.List;

public class ComplexityDemo implements Calculator {
    public static void main(String[] args) throws Exception {
        ComplexityTester tester = new ComplexityTester(new ComplexityDemo());
        // 简单测试
        tester.simpleTest();
        // 使用完整用例测试
        tester.fullTest();
        // 获取测试成功率
        double rate = tester.getRate();
    }

    @Override
    public String calculate(String arg0) {
        String code = arg0;
        StringReader reader = new StringReader(code);
        // 创建由JFlex生成的Scanner类实例
        OberonScanner obj = new OberonScanner(reader);
        Parser p = new Parser(obj);
        try {
            p.parse();
            System.out.println(p.get_res());
        } catch (Exception ex) {
            System.out.println(obj.get_line() + " line " + obj.get_column() + " column");
            System.out.println(ex.getMessage());
        }
        return p.get_res();
    }
}