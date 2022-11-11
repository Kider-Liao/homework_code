import exceptions.*;
import java_cup.runtime.*;
import java.io.*;
import nonterminal.*;
import java.util.HashMap;
import java.util.List;
import java.io.File;

public class Main {
    public static void main(String argv[]) throws Exception {
        File file = new File("../testcases");
        File[] fileList = file.listFiles();
        for (int i = 0; i < fileList.length; i++) {
            File pre = fileList[i];
            String path = "../testcases/" + pre.getName();
            // 执行parsing
            OberonScanner obj = new OberonScanner(new java.io.FileReader(path));
            OberonParser p = new OberonParser(obj);
            try {
                p.parse();
                System.out.println(p.get_res());
            } catch (Exception ex) {
                System.out.println(obj.get_line() + " line " + obj.get_column() + " column");
                System.out.println(ex.getMessage());
                System.out.println("");
            }
        }
    }
}