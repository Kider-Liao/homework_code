import java.io.InputStream;
import java.io.StringReader;
import java_cup.runtime.*;
import exceptions.*;
import complexity.Calculator;
import complexity.ComplexityTester;

/**
 * 词法分析测试程序，利用自定义测试用例进行测试
 */
public class LexicalTestDemo implements Calculator {
    /**
     * 测试的main函数
     * 
     * @param args 传入参数
     * @throws Exception 其他错误
     */
    public static void main(String[] args) throws Exception {
        ComplexityTester tester = new ComplexityTester(new LexicalTestDemo());
        // 自定义测试
        InputStream in = LexicalTestDemo.class.getResourceAsStream("testcase.xml");
        tester.test(in);

    }

    /**
     * 计算对应的词法单元，出错时输出词法错误的类别
     * 
     * @param arg0 词法分析输入
     * @return 词法分析结果
     */
    @Override
    public String calculate(String arg0) {
        try {
            String code = arg0;
            StringReader reader = new StringReader(code);
            // 创建由JFlex生成的Scanner类实例
            OberonScanner scanner = new OberonScanner(reader);
            StringBuilder builder = new StringBuilder();
            while (!scanner.yyatEOF()) {// 获取Scanner输出的token
                java_cup.runtime.Symbol s = scanner.next_token();
                // 记录token
                if (s.value != null) {
                    builder.append("(" + s.toString() + ")[" + (String) s.value + "]");
                } else {
                    builder.append("(" + s.toString() + ")");
                }
                builder.append(" ");
            }
            // 比较Scanner的输出和正确结果
            String result = builder.toString().trim();
            return result;
        } catch (Exception e) {
            return e.getClass().getName();
        }
    }

}
