
---

## 一、什么是折叠表达式？
折叠表达式是C++17为参数包量身定制的展开语法，本质是：**将一个二元运算符（比如`+`、`&&`、`<<`）自动应用到参数包的所有元素上，按“左结合”或“右结合”的方式完成“累积式”展开**。

### 核心优势（对比传统展开方式）
| 展开方式       | 代码量 | 可读性 | C++版本 | 核心问题                  |
|----------------|--------|--------|---------|---------------------------|
| 递归展开       | 多     | 一般   | C++11+  | 需要写终止函数，代码冗余  |
| 初始化列表展开 | 中     | 差     | C++11+  | 依赖辅助结构，逻辑绕      |
| 折叠表达式     | 极少   | 极高   | C++17+  | 一行搞定，逻辑直观        |

举个通俗类比：
- 递归展开：手动逐个拆包裹，拆一个看一个；
- 折叠表达式：用机器自动拆所有包裹，还能按规则把包裹里的东西合并。

---

## 二、折叠表达式的分类
折叠表达式分为**一元折叠**（核心，无初始值）和**二元折叠**（带初始值），其中一元折叠是日常开发中最常用的。

### 1. 一元折叠（核心）
适用于“直接对参数包所有元素应用运算符”的场景，又分**左折叠**和**右折叠**（区别是运算符的**结合方向**）。

| 类型       | 语法格式                | 展开逻辑（以参数包`pack=(a,b,c)`为例） | 结合方向 |
|------------|-------------------------|----------------------------------------|----------|
| 一元左折叠 | `(pack op ...)`         | `((a op b) op c)`                      | 左结合   |
| 一元右折叠 | `(... op pack)`         | `(a op (b op c))`                      | 右结合   |

#### 关键规则：
- `pack`：参数包名（比如`args`）；
- `op`：支持的二元运算符（见下表）；
- **括号必须加**：折叠表达式的括号是语法要求，缺失会编译报错；
- 结合方向影响：对满足“结合律”的运算符（如`+`、`*`、`&&`），左/右折叠结果一致；对不满足的（如`-`、`/`），结果会不同。

### 2. 二元折叠（带初始值）
适用于“需要给参数包展开加一个初始值”的场景（比如累加时初始值为10），同样分左/右折叠：

| 类型         | 语法格式                  | 展开逻辑（pack=(a,b), init=10） |
|--------------|---------------------------|---------------------------------|
| 二元左折叠   | `(pack op ... op init)`   | `((a op b) op init)`            |
| 二元右折叠   | `(init op ... op pack)`   | `(init op (a op b))`            |

### 支持的运算符
折叠表达式支持以下二元运算符（覆盖绝大多数实用场景）：
| 类别       | 运算符                          | 示例场景                  |
|------------|---------------------------------|---------------------------|
| 算术运算   | `+`、`-`、`*`、`/`、`%`、`^`等  | 累加、累乘、位运算        |
| 逻辑运算   | `&&`、`||`                      | 全为true、至少一个true    |
| 比较运算   | `==`、`!=`、`<`、`>`、`<=`、`>=` | 所有元素相等、大小比较    |
| 其他       | `,`（逗号）、`<<`（输出）       | 执行多个表达式、打印参数  |

---

## 三、一元折叠表达式（核心）：实战示例
下面通过5个典型示例，覆盖不同运算符场景，代码均可直接编译运行（需开启C++17）。

### 示例1：累加所有参数（`+`运算符，右折叠）
`+`满足结合律，左/右折叠结果一致，右折叠更符合直觉：
```cpp
#include <iostream>

// 一元右折叠：(... + args) → (a + (b + c))
template <typename... Args>
auto sum(Args&&... args) {
    return (... + args); // 等价于：((args1 + args2) + args3)（左折叠）或 (args1 + (args2 + args3))（右折叠）
}

int main() {
    std::cout << sum(1, 2, 3, 4) << std::endl; // 输出：10
    std::cout << sum(1.5, 2.5, 3.0) << std::endl; // 输出：7.0
    return 0;
}
```

### 示例2：判断所有参数是否为真（`&&`运算符，左折叠）
`&&`是左结合运算符，左折叠更高效（只要有一个false就提前终止）：
```cpp
#include <iostream>

// 一元左折叠：(args && ...) → ((a && b) && c)
template <typename... Args>
bool all_true(Args&&... args) {
    return (args && ...);
}

int main() {
    std::cout << std::boolalpha; // 输出true/false而非1/0
    std::cout << all_true(true, 1, 3 > 2) << std::endl; // 输出：true
    std::cout << all_true(true, 0, "hello") << std::endl; // 输出：false
    return 0;
}
```

### 示例3：打印所有参数（`<<`运算符，右折叠）
这是最常用的场景之一，一行代码完成参数包打印：
```cpp
#include <iostream>
#include <string>

// 一元右折叠：(std::cout << ... << args) → ((cout << a) << b) << c
template <typename... Args>
void print_all(Args&&... args) {
    (std::cout << " " << ... << args) << std::endl; // 加" "分隔参数
}

int main() {
    print_all(1, 3.14, std::string("hello"), true); // 输出： 1 3.14 hello true
    return 0;
}
```
解释：`(std::cout << " " << ... << args)` 展开后等价于：  
`((std::cout << " " << 1) << " " << 3.14) << " " << "hello") << " " << true`。

### 示例4：逗号表达式展开（执行多个函数/表达式）
逗号运算符会按顺序执行所有表达式，返回最后一个表达式的结果：
```cpp
#include <iostream>

void func(int x) { std::cout << "func(" << x << ") "; }

// 一元右折叠：(func(args), ...) → (func(a), func(b), func(c))
template <typename... Args>
void call_funcs(Args&&... args) {
    (func(args), ...); // 逗号表达式：依次执行func(args1), func(args2)...
    std::cout << std::endl;
}

int main() {
    call_funcs(1, 2, 3); // 输出：func(1) func(2) func(3)
    return 0;
}
```

### 示例5：空参数包的处理
一元折叠对部分运算符，空参数包会编译报错（比如`+`、`-`），但对`&&`、`||`、逗号运算符有默认值：
| 运算符 | 空参数包结果 | 示例               |
|--------|--------------|--------------------|
| `&&`   | `true`       | `all_true()` → true |
| `||`   | `false`      | `any_true()` → false |
| `,`    | `void`       | `call_funcs()` → 无操作 |
| `+`    | 编译报错     | `sum()` → 报错     |

```cpp
#include <iostream>

template <typename... Args>
bool all_true(Args&&... args) {
    return (args && ...); // 空参数包返回true
}

template <typename... Args>
auto sum(Args&&... args) {
    return (... + args); // 空参数包编译报错！
}

int main() {
    std::cout << std::boolalpha;
    std::cout << all_true() << std::endl; // 输出：true
    // std::cout << sum() << std::endl; // 编译报错：no matching function for call to 'sum()'
    return 0;
}
```

---

## 四、二元折叠表达式（带初始值）
解决一元折叠“空参数包报错”的问题，通过指定**初始值**，让空参数包也能正常运行。

### 语法回顾
- 二元左折叠：`(pack op ... op init)` → `((a op b) op init)`
- 二元右折叠：`(init op ... op pack)` → `(init op (a op b))`

### 示例：累加带初始值（避免空参数包报错）
```cpp
#include <iostream>

// 二元右折叠：(0 + ... + args) → (0 + (a + (b + c)))
template <typename... Args>
auto sum_with_init(Args&&... args) {
    return (0 + ... + args); // 初始值0，空参数包返回0
}

int main() {
    std::cout << sum_with_init() << std::endl; // 空参数包，输出：0
    std::cout << sum_with_init(1, 2, 3) << std::endl; // 输出：6
    return 0;
}
```

### 示例：带初始值的打印（避免空参数包无输出）
```cpp
#include <iostream>

template <typename... Args>
void print_with_init(Args&&... args) {
    // 二元右折叠：(std::cout << ... << args) 带初始值"打印结果："
    (std::cout << "打印结果：" << ... << args) << std::endl;
}

int main() {
    print_with_init(); // 输出：打印结果：
    print_with_init(1, 3.14); // 输出：打印结果：13.14
    return 0;
}
```

---

## 五、关键注意事项
### 1. 括号是必选项
折叠表达式必须用括号包裹，否则编译报错：
```cpp
// 错误写法：缺少括号
template <typename... Args>
auto bad_sum(Args&&... args) {
    return ... + args; // 编译报错：expected '(' before '...' token
}

// 正确写法：带括号
template <typename... Args>
auto good_sum(Args&&... args) {
    return (... + args); // 正确
}
```

### 2. 结合方向的影响（仅对非结合律运算符）
对`+`、`*`、`&&`、`||`等满足结合律的运算符，左/右折叠结果一致；对`-`、`/`等不满足的，结果不同：
```cpp
#include <iostream>

// 一元左折叠：(args - ...) → ((a - b) - c)
template <typename... Args>
auto left_sub(Args&&... args) {
    return (args - ...);
}

// 一元右折叠：(... - args) → (a - (b - c))
template <typename... Args>
auto right_sub(Args&&... args) {
    return (... - args);
}

int main() {
    std::cout << left_sub(10, 5, 2) << std::endl;  // ((10-5)-2)=3
    std::cout << right_sub(10, 5, 2) << std::endl; // (10-(5-2))=7
    return 0;
}
```

### 3. 编译器版本要求
折叠表达式是C++17特性，需确保编译器开启C++17模式：
- GCC/Clang：编译命令加 `-std=c++17`（或 `-std=c++20`）；
- Visual Studio：项目属性→C/C++→语言→C++标准→选择“ISO C++17 标准”。

### 4. 避免运算符重载冲突
如果参数包中的类型重载了运算符（比如自定义类重载`+`），折叠表达式会自动调用重载后的运算符，需确保重载逻辑符合预期。

---

## 总结
1. 折叠表达式是C++17简化参数包展开的核心特性，分**一元折叠**（无初始值，核心）和**二元折叠**（带初始值，解决空参数包问题）；
2. 一元左/右折叠的区别是运算符结合方向，对多数运算符（`+`、`&&`、`<<`），右折叠更符合直觉且常用；
3. 折叠表达式必须用括号包裹，支持多种运算符，空参数包需结合二元折叠或运算符特性处理；
4. 相比传统递归/初始化列表展开，折叠表达式代码量少、可读性高，是C++17+处理参数包的首选方式。
