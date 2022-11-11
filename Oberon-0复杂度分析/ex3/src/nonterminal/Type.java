package nonterminal;

import java.util.HashMap;
import java.util.List;
import exceptions.*;
public class Type extends Nonterminal{
	/**
	 * 类型
	 */
	public List<String> type;
	/**
	 * 符号表
	 */
	public HashMap<String,Type> symbol_list;
	/**
	 * 数组类型
	 */
	public Type arr_type;
	/**
	 * 参数列表
	 */
	public List<Type> para;
	/**
	 * 左值
	 */
	public boolean left;
	public Type(){
		left = false;
	}
}
