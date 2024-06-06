#pragma once
#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <list>
#include <ctime>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <sstream>

using namespace std;

// 数据结构声明
struct FileControlBlock;//文件控制块
struct Directory;//目录
struct User;//用户
struct Disk;//磁盘

struct FileControlBlock {//文件控制块
	string fileName;
	bool isDirectory;
	string content;
	size_t readWritePointer; // 文件读写指针
	bool isLocked; // 文件锁定状态

	FileControlBlock(const string& name = "", bool dir = false, const string& cont = "", size_t rwPointer = 0, bool locked = false)
		: fileName(name), isDirectory(dir), content(cont), readWritePointer(rwPointer), isLocked(locked) {}
};


struct Directory {//目录
	shared_ptr<FileControlBlock> fileControlBlock; // 目录的文件控制块
	vector<shared_ptr<Directory>> children; // 子目录
	vector<shared_ptr<FileControlBlock>> files; // 文件
	weak_ptr<Directory> parentDirectory; // 父目录
};

struct User {//用户
	string username;
	string password;
	shared_ptr<Directory> rootDirectory;
};

struct Disk {//磁盘
	shared_ptr<Directory> rootDirectory; // 根目录
	map<string, shared_ptr<User>> users; // 用户
};

// 全局变量声明
extern shared_ptr<Disk> diskData;
extern shared_ptr<Directory> currentDirectory;
extern shared_ptr<User> currentUser;
extern const string SAVE_PATH;
extern shared_ptr<FileControlBlock> copiedFile;//全局变量来保存拷贝的文件信息
extern set<string> openFiles; // 保存已打开文件的集合
extern string openFileName; // 保存当前打开的文件名
extern mutex diskMutex;
extern condition_variable cv;
extern bool exitFlag;

// 函数声明
void printHelp();//打印帮助信息
vector<string> inputResolve(const string& input);//解析输入
bool loadDisk(const string& path);//加载磁盘
bool saveDisk(const string& path);//保存磁盘
void initDisk();//初始化磁盘
void registerUser(const string& username, const string& password);//注册用户
bool loginUser(const string& username, const string& password);//登录用户
void logoutUser();//登出用户
void makeDirectory(const string& dirname);//创建目录
void changeDirectory(const string& dirname);//切换目录
void showDirectory();//显示当前目录
void createFile(const string& filename);//创建文件
void deleteFile(const string& filename);//删除文件
void writeFile();//写文件
void readFile();//读文件
void listUsers();//列出用户 
bool isValidName(const string& name);//判断文件名是否合法
void copyFile(const string& filename);//拷贝文件
void pasteFile();//粘贴文件
string getCurrentPath(); // 获取当前路径
void removeDirectory(const string& dirname); // 删除目录
void moveFile(const string& filename, const string& destDir); // 移动文件
void openFile(const string& filename); // 打开文件
void closeFile(); // 关闭文件
void lseekFile(int offset); // 控制文件读写指针移动
void flockFile(const string& filename); // 文件加锁和解锁
void headFile(int num); // 显示文件的前 num 行
void tailFile(int num); // 显示文件尾巴上的 num 行
void importFile(const string& localPath, const string& virtualName);
void exportFile(const string& virtualName, const string& localPath);
void userInteraction();//用户交互
void diskOperation();//磁盘交互
void reloadDisk(const string& path);//重新加载磁盘
string getCurrentTime();//获取当前时间

#endif // FILE_SYSTEM_H
