package nonterminal;

public class Expression extends Nonterminal{
	/**
	 * 表达式类型
	 */
	String type;
	/**
	 * 获取类型
	 * @return 表达式类型
	 */
	public String get_type() {
		return type;
	}
	/**
	 * 设置表达式类型
	 * @param t 类型
	 */
	public void set_type(String t) {
		type = t;
	}
	/**
	 * 左值
	 */
	String left;
	/**
	 * left为空表示不是左值，为var表示是左值
	 * @return 是否为左值
	 */
	public String get_left() {
		return left;
	}
	/**
	 * 设置为左值
	 */
	public void set_left() {
		left = "var";
	}
	/**
	 * 构造函数
	 */
	public Expression() {
		left = "";
		type = "";
	}
}
