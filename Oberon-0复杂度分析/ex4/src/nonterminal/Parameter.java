package nonterminal;

import java.util.List;
import exceptions.*;
public class Parameter extends Nonterminal{
	/**
	 * 参数列表，记录参数的类型
	 */
	public List<Type> para;
	/**
	 * 变量名列表
	 */
	public List<String> var;
}
