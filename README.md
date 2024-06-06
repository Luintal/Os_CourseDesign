# Os_CourseDesign
# 2024年操作系统课程设计

# 课程设计内容

设计一个文件系统，要求实现文件的创建、删除、修改和查询等功能，以及用户的注册和登录功能。用户之间的文件系统互不干扰，文件系统采用两级文件目录结构，第一级为用户账号，第二级为用户账号下的文件系统。为了简便文件系统的设计，可以不实现文件共享、文件系统的安全、管道文件与设备文件等特殊内容。





## 课程设计任务完成情况

## 用户类命令

| 命令 | 描述 | 完成情况 |
| --- | --- | --- |
| `register` | 注册用户 | 已完成 |
| `login` | 登录用户 | 已完成 |
| `logout` | 退出登录 | 已完成 |
| `listUsers` | 显示用户账号及密码 | 已完成 |

## 文件类命令

| 命令 | 描述 | 完成情况 |
| --- | --- | --- |
| `create` | 创建文件 | 已完成 |
| `delete` | 删除文件 | 已完成 |
| `open` | 打开文件 | 已完成 |
| `close` | 关闭文件 | 已完成 |
| `read` | 读文件 | 已完成 |
| `write` | 写文件 | 已完成 |
| `move` | 移动文件 | 已完成 |
| `copy` | 拷贝文件 | 已完成 |
| `paste` | 粘贴文件 | 已完成 |
| `flock` | 文件加锁，需要实现加锁和解锁功能 | 已完成 |
| `head -num` | 显示文件的前num行 | 已完成 |
| `tail -num` | 显示文件尾巴上的num行 | 已完成 |
| `lseek` | 文件读写指针的移动 | 已完成 |

## 目录类命令

| 命令 | 描述 | 完成情况 |
| --- | --- | --- |
| `cd` | 进入目录 | 已完成 |
| `dir` | 显示当前目录 | 已完成 |
| `mkdir` | 创建目录 | 已完成 |
| `rmdir` | 删除目录 | 已完成 |

## 本地磁盘与虚拟磁盘驱动器的内容复制

| 命令 | 描述 | 完成情况 |
| --- | --- | --- |
| `import` | 将本地磁盘的内容导入到虚拟磁盘驱动器 | 已完成 |
| `export` | 将虚拟磁盘驱动器的内容导出到本地磁盘 | 已完成 |

## 多线程支持

| 描述 | 完成情况 |
| --- | --- |
| 用一个线程与用户进行交互，接受请求并将请求转换为对应的消息，通知后台维护虚拟磁盘驱动器的线程 | 已完成 |
| 程序可同时运行多次，每个程序都可接受用户的请求，但后台只有一个线程在维护虚拟磁盘驱动器上的内容 | 已完成 |

---
