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

// ���ݽṹ����
struct FileControlBlock;//�ļ����ƿ�
struct Directory;//Ŀ¼
struct User;//�û�
struct Disk;//����

struct FileControlBlock {//�ļ����ƿ�
	string fileName;
	bool isDirectory;
	string content;
	size_t readWritePointer; // �ļ���дָ��
	bool isLocked; // �ļ�����״̬

	FileControlBlock(const string& name = "", bool dir = false, const string& cont = "", size_t rwPointer = 0, bool locked = false)
		: fileName(name), isDirectory(dir), content(cont), readWritePointer(rwPointer), isLocked(locked) {}
};


struct Directory {//Ŀ¼
	shared_ptr<FileControlBlock> fileControlBlock; // Ŀ¼���ļ����ƿ�
	vector<shared_ptr<Directory>> children; // ��Ŀ¼
	vector<shared_ptr<FileControlBlock>> files; // �ļ�
	weak_ptr<Directory> parentDirectory; // ��Ŀ¼
};

struct User {//�û�
	string username;
	string password;
	shared_ptr<Directory> rootDirectory;
};

struct Disk {//����
	shared_ptr<Directory> rootDirectory; // ��Ŀ¼
	map<string, shared_ptr<User>> users; // �û�
};

// ȫ�ֱ�������
extern shared_ptr<Disk> diskData;
extern shared_ptr<Directory> currentDirectory;
extern shared_ptr<User> currentUser;
extern const string SAVE_PATH;
extern shared_ptr<FileControlBlock> copiedFile;//ȫ�ֱ��������濽�����ļ���Ϣ
extern set<string> openFiles; // �����Ѵ��ļ��ļ���
extern string openFileName; // ���浱ǰ�򿪵��ļ���
extern mutex diskMutex;
extern condition_variable cv;
extern bool exitFlag;

// ��������
void printHelp();//��ӡ������Ϣ
vector<string> inputResolve(const string& input);//��������
bool loadDisk(const string& path);//���ش���
bool saveDisk(const string& path);//�������
void initDisk();//��ʼ������
void registerUser(const string& username, const string& password);//ע���û�
bool loginUser(const string& username, const string& password);//��¼�û�
void logoutUser();//�ǳ��û�
void makeDirectory(const string& dirname);//����Ŀ¼
void changeDirectory(const string& dirname);//�л�Ŀ¼
void showDirectory();//��ʾ��ǰĿ¼
void createFile(const string& filename);//�����ļ�
void deleteFile(const string& filename);//ɾ���ļ�
void writeFile();//д�ļ�
void readFile();//���ļ�
void listUsers();//�г��û� 
bool isValidName(const string& name);//�ж��ļ����Ƿ�Ϸ�
void copyFile(const string& filename);//�����ļ�
void pasteFile();//ճ���ļ�
string getCurrentPath(); // ��ȡ��ǰ·��
void removeDirectory(const string& dirname); // ɾ��Ŀ¼
void moveFile(const string& filename, const string& destDir); // �ƶ��ļ�
void openFile(const string& filename); // ���ļ�
void closeFile(); // �ر��ļ�
void lseekFile(int offset); // �����ļ���дָ���ƶ�
void flockFile(const string& filename); // �ļ������ͽ���
void headFile(int num); // ��ʾ�ļ���ǰ num ��
void tailFile(int num); // ��ʾ�ļ�β���ϵ� num ��
void importFile(const string& localPath, const string& virtualName);
void exportFile(const string& virtualName, const string& localPath);
void userInteraction();//�û�����
void diskOperation();//���̽���
void reloadDisk(const string& path);//���¼��ش���
string getCurrentTime();//��ȡ��ǰʱ��

#endif // FILE_SYSTEM_H
