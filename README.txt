# Myshell命令解释器
（Advanced Programming in the UNIX Environment 上机作业）

README文件更新日期：2020.6.19
README文件版本：1.0.0

本程序一种类似shell命令解释器，shell 是一个用 C 语言编写的程序，它是用户使用 Linux 的桥梁。命令解释器shell提供了一个界面，用户通过这个界面访问操作系统内核的服务。本程序从控制台启动后可以完成用户输入的各种命令。

初次使用请先阅读/doc目录下的用户手册！

主要特点：
1. 支持自定义命令提示符；
2. 支持快捷键快速结束程序；
3. 支持任务后台运行机制；
4. 支持输入输出重定向。

运行环境：
Linux操作系统（支持64位）
▶ Ubuntu
▶ Debian
▶ Fedora Core/CentOS
▶ SuSE Linux Enterprise Desktop
▶ FreeBSD
▶ Solaris
▶ MacOS

测试环境：
Linux ubuntu 5.3.0-40-generic #32-Ubuntu SMP Fri Jan 31 20:24:34 UTC 2020 x86_64 x86_64 x86_64 GNU/Linux

代码包文件结构：
-codes 代码文件夹
  --myshell.c 程序源代码C语言文件
  --myshell Linux平台可执行程序（由myshell.c在上述测试环境下编译得到）
-docs 文档文件夹
  --测试报告.md
  --详细设计文档.md
  --需求分析文档.md
  --用户手册.md
  --总结报告.md
-figures 图片文件夹（部分文档依赖的图片，如：代码架构和流程图等）

作者：Cielover

有关常见问题解答、设计文档和测试报告，请参见同目录下doc文件夹。

请将问题、建议和错误报告写在项目讨论区中。
