# lab1 实验报告
> PB20111630 张艺耀

## 实验要求

+ 本次实验需要完成一个完整的 Cminus-f 解析器，包括基于 `flex` 的词法分析器和基于 `bison` 的语法分析器。
+ 根据`Cminus-f`的词法补全`src/parser/lexical_analyzer.l`文件，完成词法分析器。

+ 完成 `src/parser/syntax_analyzer.y` 语法分析器。

## 实验难点

### 词法分析器

个人认为最难的部分是词法分析器中浮点数和关于无嵌套的多行注释的正则表达式的书写。

关于无嵌套的多行注释的正则表达式，我一开始的写法是：

` "\/\*"(([^\/]*[^\/])*([^\/\*])*([^\*]\/[^\*])*)*"\*\/"`

它指的是在前后两个注释符号之间不存在与之相同的符号，这样写较为冗余

之后改为（参考自StackOverflow）：

`\/\*([^*]|(\*+[^*/]))*\*+\/ `

其正确性如下：

```
The full explanation and derivation of that regex is excellently elaborated upon here.
In short:
"/*" marks the start of the comment
( [^*] | (\*+[^*/]) )* says accept all characters that are not * (the [^*] ) or accept a sequence of one or more * as long as the sequence does not have a '*' or a /' following it (the (*+[^*/])). This means that all ******... sequences will be accepted except for *****/ since you can't find a sequence of * there that isn't followed by a * or a /.
The *******/ case is then handled by the last bit of the RegEx which matches any number of * followed by a / to mark the end of the comment i.e \*+\/
```

以及在flex模式+动作的书写过程中，先后顺序很重要。

比如匹配一个特定的字符串的正则表达式要尽量写在前面，否则会出现`warning: rule can not be matched`的情况，这是指我们书写的这些表达式会被之前的正则表达式匹配到。

又如如下三行代码，如果把53行与55行顺序调换，则会先匹配到整数再匹配浮点数，这样会多出来一个句子。

![geNropIMd1WXqFO](https://s2.loli.net/2022/09/20/geNropIMd1WXqFO.png)

### 语法分析器

词法分析器的写法较为简单，只需要无脑套公式就行了。

注意参数个数问题防止漏参数（实验过程中由于漏了param的第四个参数导致bug）

![aj2qC5HtJ8Q7RrW](https://s2.loli.net/2022/09/20/aj2qC5HtJ8Q7RrW.png)

## 实验设计

根据实验文档补全`lexical_analyzer.l`和`syntax_analyzer.y`其中需要补全正则表达式的书写，编写token和type以及对应规则，补全union中的node定义。

最后通过给出的测试脚本分别测试easy normal hard的测试样例。

## 实验结果验证

<img src="https://s2.loli.net/2022/09/20/ZMh6uq85nGXVwKT.png" alt="ZMh6uq85nGXVwKT" style="zoom:50%;" />

<img src="/Users/fluegelcat/Library/Application Support/typora-user-images/image-20220920225244565.png" alt="image-20220920225244565" style="zoom:50%;" />

自行设计的测试：

```c
/* I am NO.1 */
int main(void) {
  int a;
  int b;
  float c;
  a = 1;
  b = 2;
  c = 1.1;

  a = a + b;

  return a;

}
```

lexer输出如下：

```
Token	      Text	Line	Column (Start,End)
262  	       int	1	(1,4)
285  	      main	1	(5,9)
279  	         (	1	(9,10)
264  	      void	1	(10,14)
280  	         )	1	(14,15)
283  	         {	1	(16,17)
262  	       int	2	(3,6)
285  	         a	2	(7,8)
277  	         ;	2	(8,9)
262  	       int	3	(3,6)
285  	         b	3	(7,8)
277  	         ;	3	(8,9)
266  	     float	4	(3,8)
285  	         c	4	(9,10)
277  	         ;	4	(10,11)
285  	         a	5	(3,4)
276  	         =	5	(5,6)
286  	         1	5	(7,8)
277  	         ;	5	(8,9)
285  	         b	6	(3,4)
276  	         =	6	(5,6)
286  	         2	6	(7,8)
277  	         ;	6	(8,9)
285  	         c	7	(3,4)
276  	         =	7	(5,6)
287  	       1.1	7	(7,10)
277  	         ;	7	(10,11)
285  	         a	9	(3,4)
276  	         =	9	(5,6)
285  	         a	9	(7,8)
259  	         +	9	(9,10)
285  	         b	9	(11,12)
277  	         ;	9	(12,13)
263  	    return	11	(3,9)
285  	         a	11	(10,11)
277  	         ;	11	(11,12)
284  	         }	13	(1,2)
```

parser输出如下：

```
>--+ program
|  >--+ declaration-list
|  |  >--+ declaration
|  |  |  >--+ fun-declaration
|  |  |  |  >--+ type-specifier
|  |  |  |  |  >--* int
|  |  |  |  >--* main
|  |  |  |  >--* (
|  |  |  |  >--+ params
|  |  |  |  |  >--* void
|  |  |  |  >--* )
|  |  |  |  >--+ compound-stmt
|  |  |  |  |  >--* {
|  |  |  |  |  >--+ local-declarations
|  |  |  |  |  |  >--+ local-declarations
|  |  |  |  |  |  |  >--+ local-declarations
|  |  |  |  |  |  |  |  >--+ local-declarations
|  |  |  |  |  |  |  |  |  >--* epsilon
|  |  |  |  |  |  |  |  >--+ var-declaration
|  |  |  |  |  |  |  |  |  >--+ type-specifier
|  |  |  |  |  |  |  |  |  |  >--* int
|  |  |  |  |  |  |  |  |  >--* a
|  |  |  |  |  |  |  |  |  >--* ;
|  |  |  |  |  |  |  >--+ var-declaration
|  |  |  |  |  |  |  |  >--+ type-specifier
|  |  |  |  |  |  |  |  |  >--* int
|  |  |  |  |  |  |  |  >--* b
|  |  |  |  |  |  |  |  >--* ;
|  |  |  |  |  |  >--+ var-declaration
|  |  |  |  |  |  |  >--+ type-specifier
|  |  |  |  |  |  |  |  >--* float
|  |  |  |  |  |  |  >--* c
|  |  |  |  |  |  |  >--* ;
|  |  |  |  |  >--+ statement-list
|  |  |  |  |  |  >--+ statement-list
|  |  |  |  |  |  |  >--+ statement-list
|  |  |  |  |  |  |  |  >--+ statement-list
|  |  |  |  |  |  |  |  |  >--+ statement-list
|  |  |  |  |  |  |  |  |  |  >--+ statement-list
|  |  |  |  |  |  |  |  |  |  |  >--* epsilon
|  |  |  |  |  |  |  |  |  |  >--+ statement
|  |  |  |  |  |  |  |  |  |  |  >--+ expression-stmt
|  |  |  |  |  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* a
|  |  |  |  |  |  |  |  |  |  |  |  |  >--* =
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ simple-expression
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ integer
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* 1
|  |  |  |  |  |  |  |  |  |  |  |  >--* ;
|  |  |  |  |  |  |  |  |  >--+ statement
|  |  |  |  |  |  |  |  |  |  >--+ expression-stmt
|  |  |  |  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  |  |  |  >--+ var
|  |  |  |  |  |  |  |  |  |  |  |  |  >--* b
|  |  |  |  |  |  |  |  |  |  |  |  >--* =
|  |  |  |  |  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ simple-expression
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ integer
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* 2
|  |  |  |  |  |  |  |  |  |  |  >--* ;
|  |  |  |  |  |  |  |  >--+ statement
|  |  |  |  |  |  |  |  |  >--+ expression-stmt
|  |  |  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  |  |  >--+ var
|  |  |  |  |  |  |  |  |  |  |  |  >--* c
|  |  |  |  |  |  |  |  |  |  |  >--* =
|  |  |  |  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  |  |  |  >--+ simple-expression
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ float
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* 1.1
|  |  |  |  |  |  |  |  |  |  >--* ;
|  |  |  |  |  |  |  >--+ statement
|  |  |  |  |  |  |  |  >--+ expression-stmt
|  |  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  |  >--+ var
|  |  |  |  |  |  |  |  |  |  |  >--* a
|  |  |  |  |  |  |  |  |  |  >--* =
|  |  |  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  |  |  >--+ simple-expression
|  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ additive-expression
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* a
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ addop
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* +
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ term
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
|  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* b
|  |  |  |  |  |  |  |  |  >--* ;
|  |  |  |  |  |  >--+ statement
|  |  |  |  |  |  |  >--+ return-stmt
|  |  |  |  |  |  |  |  >--* return
|  |  |  |  |  |  |  |  >--+ expression
|  |  |  |  |  |  |  |  |  >--+ simple-expression
|  |  |  |  |  |  |  |  |  |  >--+ additive-expression
|  |  |  |  |  |  |  |  |  |  |  >--+ term
|  |  |  |  |  |  |  |  |  |  |  |  >--+ factor
|  |  |  |  |  |  |  |  |  |  |  |  |  >--+ var
|  |  |  |  |  |  |  |  |  |  |  |  |  |  >--* a
|  |  |  |  |  |  |  |  >--* ;
|  |  |  |  |  >--* }
```

## 实验反馈

建议把正则表达式书写语法详细介绍一下，比如使用`[] " " `和不加任何修饰符的表达式存在何种区别等。
