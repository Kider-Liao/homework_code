
import java.io.StringReader;
import java_cup.runtime.*;
import exceptions.*;

/**
 * 词法分析器测试主程序
 */
public class LexicalMainDemo {
    // Scanner使用测试方式
    /**
     * 词法分析测试main函数
     * 
     * @param args 输入的参数
     * @throws Exception 词法分析抛出的异常
     */
    public static void main(String[] args) throws Exception {
        // 源码数据输入与正确结果（可以使用其他方式读取）
        String code = "MODULE Sample;\n\tVAR a:INTEGER;\n\tPROCEDURE Test;\n\tEND Test;\nBEGIN\n\tRead(a);\nEND Sample.";
        StringReader reader = new StringReader(code);
        String answer = "(#1) (#41)[Sample] (#34) (#6) (#41)[a] (#35) (#41)[INTEGER] (#34) (#16) (#41)[Test] (#34) (#3) (#41)[Test] (#34) (#2) (#41)[Read] (#31) (#41)[a] (#32) (#34) (#3) (#41)[Sample] (#33) (#0)";

        // 创建由JFlex生成的Scanner类实例
        OberonScanner scanner = new OberonScanner(reader);
        StringBuilder builder = new StringBuilder();
        while (!scanner.yyatEOF()) {// 获取Scanner输出的token
            java_cup.runtime.Symbol s = scanner.next_token();
            // 记录token
            if (s.value != null) {
                builder.append("(" + s.toString() + ")[" + s.value.toString() + "]");
            } else {
                System.out.println((String) s.value);
                builder.append("(" + s.toString() + ")");
            }
            builder.append(" ");
        }
        // 比较Scanner的输出和正确结果
        String result = builder.toString().trim();
        System.out.println(result);
        if (answer.equals(result)) {
            System.out.println("check success");
        } else {
            System.out.println("check fail");
        }
    }
}
