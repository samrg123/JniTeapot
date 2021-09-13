# JniTeapot C++

JniTeapot C++ is a VSCode extension that extends the syntax of C++ to include support JniTeapot annotations, types, and OpenGl syntax written inside of the `Shader` macro.

<br><br>

## Section 1: Supported C++ attributes
JniTeapot C++ supports tokenizing the following C++ attributes
|Attribute              |Textmate Scope                      |
|----------------------:|:-----------------------------------|
|`alignas`              | `jniteapot-cpp.attribute.alignas`  |

<br><br>

## Section 2: Supported Comment Annotations
JniTeapot C++ supports tokenizing C++ comments with the following keywords:
|Keyword                                |Textmate Scope         |
|--------------------------------------:|:----------------------|
|`NOTE`, `Note`                         | `jniteapot-cpp.note`  |
|`TODO`, `Todo`                         | `jniteapot-cpp.todo`  |
|`FIXME`, `Fixme`                       | `jniteapot-cpp.fixme` |
|`BUG`, `Bug`                           | `jniteapot-cpp.bug`   |
|`WARN`, `Warn`, `WARNING`, `Warning`   | `jniteapot-cpp.warn`  |
|`ERR`, `Err `, `ERROR`, `Error`        | `jniteapot-cpp.error` |

Each keyword can be followed by an optional name in parenthesis and message led with an optional colon.
For example:
```C++
// TODO(Sam): Fix this bug
```
will have the keyword: `"TODO(Sam)"`, name: `"Sam"`, and message: `"Fix this bug"` which can be further stylized with the respective textmate scopes:
|Field                  |Textmate Scope                 |
|----------------------:|:------------------------------|
|`keyword`              | `jniteapot-cpp.todo.keyword`  |
|`name`                 | `jniteapot-cpp.todo.name`     |
|`message`              | `jniteapot-cpp.todo.message`  |


<br><br>


## Section 3: Support with GLSL
JniTeapot C++ supports embedding GLSL language syntax and highlighting when using the `Shader` macro.

<br><br>


## Section 4: Planned updates
- Custom highlighting for GLSL types inside of Shader macro
- Code completion for GLSL types inside of Shader macro

<br><br>

