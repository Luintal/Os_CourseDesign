#include "File_System.h"

shared_ptr<Disk> diskData;
shared_ptr<Directory> currentDirectory;
shared_ptr<User> currentUser;
const string SAVE_PATH = "disk.dat";
shared_ptr<FileControlBlock> copiedFile = nullptr;//初始化全局变量来保存拷贝的文件信息
set<string> openFiles; // 初始化已打开文件的集合
string openFileName = ""; // 初始化当前打开的文件名
mutex diskMutex;
condition_variable cv;
bool exitFlag = false;
queue<string> commandQueue;

// 打印帮助信息
void printHelp() {
    cout << "可用命令及用法:\n";
    cout << "  register <用户名> <密码>                  - 注册新用户\n";
    cout << "  login <用户名> <密码>                     - 登录用户\n";
    cout << "  logout                                    - 退出当前用户\n";
    cout << "  listUsers                                 - 显示账号\n";
    cout << "  mkdir <目录名>                            - 创建目录\n";
    cout << "  cd <目录名>                               - 切换目录\n";
    cout << "  rmdir <目录名>                            - 删除目录\n";
    cout << "  dir                                       - 显示当前目录内容\n";
    cout << "  create <文件名>                           - 创建文件\n";
    cout << "  delete <文件名>                           - 删除文件\n";
    cout << "  open <文件名>                             - 打开文件\n";
    cout << "  close <文件名>                            - 关闭文件\n";
    cout << "  write                                     - 写入文件\n";
    cout << "  read                                      - 读取文件内容\n";
    cout << "  head <行数>                               - 显示文件头部\n";
    cout << "  tail <行数>                               - 显示文件尾部\n";
    cout << "  lseek <偏移量>                            - 移动文件读写指针\n";
    cout << "  move <文件名> <目标目录>                  - 移动文件\n";
    cout << "  copy <文件名>                             - 拷贝文件\n";
    cout << "  paste                                     - 粘贴文件\n";
    cout << "  flock <文件名>                            - 锁定/解锁文件\n";
    cout << "  import <本地文件路径> <虚拟磁盘文件名>    - 导入文件\n";
    cout << "  export <虚拟磁盘文件名> <本地目录路径>    - 导出文件\n";
    cout << "  help                                      - 显示帮助信息\n";
    cout << "  exit                                      - 退出程序\n";
    cout << endl;
}

// 解析用户输入的命令
vector<string> inputResolve(const string& input) {
    vector<string> result;
    istringstream iss(input);
    string token;
    while (iss >> token) {
        result.push_back(token);
    }
    return result;
}

// 递归保存目录和文件
void saveDirectory(ofstream& file, shared_ptr<Directory> directory) {
    size_t dirCount = directory->children.size();
    size_t fileCount = directory->files.size();
    file.write(reinterpret_cast<const char*>(&dirCount), sizeof(dirCount));
    for (const auto& dir : directory->children) {
        size_t dirNameLen = dir->fileControlBlock->fileName.size();
        file.write(reinterpret_cast<const char*>(&dirNameLen), sizeof(dirNameLen));
        file.write(dir->fileControlBlock->fileName.c_str(), dirNameLen);
        saveDirectory(file, dir); // 递归保存子目录
    }
    file.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));
    for (const auto& fcb : directory->files) {
        size_t fileNameLen = fcb->fileName.size();
        size_t contentLen = fcb->content.size();
        file.write(reinterpret_cast<const char*>(&fileNameLen), sizeof(fileNameLen));
        file.write(fcb->fileName.c_str(), fileNameLen);
        file.write(reinterpret_cast<const char*>(&contentLen), sizeof(contentLen));
        file.write(fcb->content.c_str(), contentLen);
        file.write(reinterpret_cast<const char*>(&fcb->readWritePointer), sizeof(fcb->readWritePointer));
        file.write(reinterpret_cast<const char*>(&fcb->isLocked), sizeof(fcb->isLocked));
    }
}

// 递归加载目录和文件，并设置父目录指针
void loadDirectory(ifstream& file, shared_ptr<Directory> directory) {
    size_t dirCount, fileCount;
    file.read(reinterpret_cast<char*>(&dirCount), sizeof(dirCount));

    for (size_t i = 0; i < dirCount; ++i) {
        string dirName;
        size_t dirNameLen;
        file.read(reinterpret_cast<char*>(&dirNameLen), sizeof(dirNameLen));
        dirName.resize(dirNameLen);
        file.read(&dirName[0], dirNameLen);

        auto dir = make_shared<Directory>();
        dir->fileControlBlock = make_shared<FileControlBlock>();
        dir->fileControlBlock->fileName = dirName;
        dir->fileControlBlock->isDirectory = true;
        dir->parentDirectory = directory; // 设置父目录指针

        directory->children.push_back(dir);

        loadDirectory(file, dir); // 递归加载子目录
    }

    file.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    for (size_t i = 0; i < fileCount; ++i) {
        string fileName, content;
        size_t fileNameLen, contentLen;

        file.read(reinterpret_cast<char*>(&fileNameLen), sizeof(fileNameLen));
        fileName.resize(fileNameLen);
        file.read(&fileName[0], fileNameLen);

        file.read(reinterpret_cast<char*>(&contentLen), sizeof(contentLen));
        content.resize(contentLen);
        file.read(&content[0], contentLen);

        auto fcb = make_shared<FileControlBlock>();
        fcb->fileName = fileName;
        fcb->isDirectory = false;
        fcb->content = content;

        file.read(reinterpret_cast<char*>(&fcb->readWritePointer), sizeof(fcb->readWritePointer));
        file.read(reinterpret_cast<char*>(&fcb->isLocked), sizeof(fcb->isLocked));

        directory->files.push_back(fcb);
    }
}

bool saveDisk(const string& path) {
    ofstream file(path, ios::binary);
    if (!file.is_open()) {
        cerr << "无法打开文件进行保存: " << path << endl;
        return false;
    }
    size_t usersCount = diskData->users.size();
    file.write(reinterpret_cast<const char*>(&usersCount), sizeof(usersCount));
    for (const auto& [username, user] : diskData->users) {
        size_t usernameLen = username.size();
        size_t passwordLen = user->password.size();
        file.write(reinterpret_cast<const char*>(&usernameLen), sizeof(usernameLen));
        file.write(username.c_str(), usernameLen);
        file.write(reinterpret_cast<const char*>(&passwordLen), sizeof(passwordLen));
        file.write(user->password.c_str(), passwordLen);

        saveDirectory(file, user->rootDirectory);
    }
    file.close();
    //cout << "磁盘保存成功: " << path << endl;
    return true;
}


bool loadDisk(const string& path) {
    ifstream file(path, ios::binary);
    if (!file.is_open()) {
        cerr << "无法打开磁盘进行加载: " << path << endl;
        return false;
    }

    // 清空现有数据
    diskData = make_shared<Disk>();
    size_t usersCount;
    file.read(reinterpret_cast<char*>(&usersCount), sizeof(usersCount));

    for (size_t i = 0; i < usersCount; ++i) {
        string username, password;
        size_t usernameLen, passwordLen;

        // 读取用户名和密码
        file.read(reinterpret_cast<char*>(&usernameLen), sizeof(usernameLen));
        username.resize(usernameLen);
        file.read(&username[0], usernameLen);

        file.read(reinterpret_cast<char*>(&passwordLen), sizeof(passwordLen));
        password.resize(passwordLen);
        file.read(&password[0], passwordLen);

        auto user = make_shared<User>();
        user->username = username;
        user->password = password;

        // 创建用户根目录
        user->rootDirectory = make_shared<Directory>();
        auto rootDir = user->rootDirectory;
        rootDir->fileControlBlock = make_shared<FileControlBlock>();
        rootDir->fileControlBlock->fileName = "/";
        rootDir->fileControlBlock->isDirectory = true;
        rootDir->parentDirectory.reset(); // 根目录没有父目录

        // 加载用户目录和文件
        loadDirectory(file, rootDir);

        diskData->users[username] = user;
    }

    file.close();
    //cout << "磁盘加载成功: " << path << endl;
    return true;
}

// 重新加载磁盘
void reloadDisk(const string& path) {
    // 保存当前用户信息、目录信息、打开文件的信息、复制文件的信息
    auto savedUser = currentUser;
    auto savedDirectory = currentDirectory;
    string savedOpenFileName = openFileName;
    size_t savedReadWritePointer = 0;
    auto savedCopiedFile = copiedFile;

    if (!savedOpenFileName.empty()) {
        for (const auto& file : currentDirectory->files) {
            if (file->fileName == savedOpenFileName) {
                savedReadWritePointer = file->readWritePointer;
                break;
            }
        }
    }

    // 清空全局变量等所有暂存量
    diskData = nullptr;
    currentDirectory = nullptr;
    currentUser = nullptr;
    copiedFile = nullptr;
    openFiles.clear();
    openFileName = "";

    // 重新加载磁盘数据
    if (!loadDisk(path)) {
        cout << "重新加载磁盘数据失败。" << endl;
        return;
    }

    // 恢复当前用户信息和目录信息
    if (savedUser) {
        currentUser = diskData->users[savedUser->username];
        if (currentUser) {
            // 查找并恢复当前目录
            auto it = std::find_if(
                currentUser->rootDirectory->children.begin(),
                currentUser->rootDirectory->children.end(),
                [&savedDirectory](const shared_ptr<Directory>& dir) {
                    return dir->fileControlBlock->fileName == savedDirectory->fileControlBlock->fileName;
                });

            if (it != currentUser->rootDirectory->children.end()) {
                currentDirectory = *it;
            }
            else {
                currentDirectory = currentUser->rootDirectory;
            }
        }
    }

    // 恢复打开的文件信息
    if (!savedOpenFileName.empty()) {
        for (const auto& file : currentDirectory->files) {
            if (file->fileName == savedOpenFileName) {
                openFiles.insert(savedOpenFileName);
                openFileName = savedOpenFileName;
                file->readWritePointer = savedReadWritePointer;
                break;
            }
        }
    }

    // 恢复复制文件的信息
    if (savedCopiedFile) {
        copiedFile = savedCopiedFile;
    }

    //cout << "磁盘数据已重新加载。" << endl;
}



// 获取当前时间字符串
string getCurrentTime() {
    time_t now = time(0);
    tm localtm;
    localtime_s(&localtm, &now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localtm);
    return string(buffer);
}




// 初始化磁盘
void initDisk() {
    diskData = make_shared<Disk>();
    auto rootDir = make_shared<Directory>();
    auto rootFCB = make_shared<FileControlBlock>();
    rootFCB->fileName = "/";
    rootFCB->isDirectory = true;
    rootDir->fileControlBlock = rootFCB;
    diskData->users["root"] = make_shared<User>();
    diskData->users["root"]->username = "root";
    diskData->users["root"]->password = "root";
    diskData->users["root"]->rootDirectory = rootDir;
    currentDirectory = rootDir;
    currentUser = diskData->users["root"];
    cout << "新磁盘已初始化。" << endl;
}

// 用户注册
void registerUser(const string& username, const string& password) {
    if (diskData->users.find(username) != diskData->users.end()) {
        cout << "用户名已存在。" << endl;
        return;
    }
    auto user = make_shared<User>();
    user->username = username;
    user->password = password;
    user->rootDirectory = make_shared<Directory>();
    user->rootDirectory->fileControlBlock = make_shared<FileControlBlock>();
    user->rootDirectory->fileControlBlock->fileName = "/";
    user->rootDirectory->fileControlBlock->isDirectory = true;
    diskData->users[username] = user;
    cout << "用户注册成功。" << endl;
}

// 用户登录
bool loginUser(const string& username, const string& password) {
    if (diskData->users.find(username) == diskData->users.end() || diskData->users[username]->password != password) {
        cout << "用户名或密码错误。" << endl;
        return false;
    }
    currentUser = diskData->users[username];
    currentDirectory = currentUser->rootDirectory;
    cout << "用户登录成功。" << endl;
    return true;
}

// 用户注销
void logoutUser() {
    if (!currentUser) {
        cout << "当前没有用户登录。" << endl;
        return;
    }
    currentUser = nullptr;
    currentDirectory = nullptr;
    cout << "用户已注销。" << endl;
}

// 创建目录
void makeDirectory(const string& dirname) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (!isValidName(dirname)) {
        cout << "无效的目录名。" << endl;
        return;
    }
    for (const auto& dir : currentDirectory->children) {
        if (dir->fileControlBlock->fileName == dirname) {
            cout << "目录已存在。" << endl;
            return;
        }
    }
    auto dir = make_shared<Directory>();
    dir->fileControlBlock = make_shared<FileControlBlock>();
    dir->fileControlBlock->fileName = dirname;
    dir->fileControlBlock->isDirectory = true;
    dir->parentDirectory = currentDirectory; // 设置父目录指针
    currentDirectory->children.push_back(dir);
    cout << "目录创建成功。" << endl;
    saveDisk(SAVE_PATH);
}


// 切换目录
void changeDirectory(const string& dirname) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (dirname == "..") {
        if (auto parent = currentDirectory->parentDirectory.lock()) {
            currentDirectory = parent;
        }
        else {
            cout << "已在根目录，无法再向上一级。" << endl;
        }
        return;
    }
    if (!isValidName(dirname)) {
        cout << "无效的目录名。" << endl;
        return;
    }
    for (const auto& dir : currentDirectory->children) {
        if (dir->fileControlBlock->fileName == dirname) {
            currentDirectory = dir;
            return;
        }
    }
    cout << "目录不存在。" << endl;
}

// 显示当前目录内容
void showDirectory() {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }

    if (currentDirectory == currentUser->rootDirectory) {
        // 当前在用户根目录下，输出所有子目录名
        cout << "用户 " << currentUser->username << " 根目录下的目录: ";
        for (const auto& dir : currentDirectory->children) {
            cout << dir->fileControlBlock->fileName << " ";
        }
        cout << endl << "文件: ";
        for (const auto& file : currentDirectory->files) {
            cout << file->fileName << " ";
        }
        cout << endl;
    }
    else {
        // 当前在子目录下，输出当前目录名及其下所有文件名
        cout << "当前目录: " << currentDirectory->fileControlBlock->fileName << endl;
        cout << "目录: ";
        for (const auto& dir : currentDirectory->children) {
            cout << dir->fileControlBlock->fileName << " ";
        }
        cout << endl << "文件: ";
        for (const auto& file : currentDirectory->files) {
            cout << file->fileName << " ";
        }
        cout << endl;
    }
}


// 创建文件
void createFile(const string& filename) {

    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (!isValidName(filename)) {
        cout << "无效的文件名。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == filename) {
            cout << "文件已存在。" << endl;
            return;
        }
    }
    auto fcb = make_shared<FileControlBlock>();
    fcb->fileName = filename;
    fcb->isDirectory = false;
    fcb->readWritePointer = 0; // 初始化读写指针
    fcb->isLocked = false; // 初始化锁定状态
    currentDirectory->files.push_back(fcb);
    cout << "文件创建成功。" << endl;

}




// 删除文件
void deleteFile(const string& filename) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (!isValidName(filename)) {
        cout << "无效的文件名。" << endl;
        return;
    }
    for (auto it = currentDirectory->files.begin(); it != currentDirectory->files.end(); ++it) {
        if ((*it)->fileName == filename) {
            currentDirectory->files.erase(it);
            cout << "文件删除成功。" << endl;
            return;
        }
    }
    cout << "文件不存在。" << endl;
    saveDisk(SAVE_PATH);
}


// 写入文件
void writeFile() {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "当前没有打开的文件。" << endl;
        return;
    }
    cout << "请从第二行开始输入内容（输入 'END' 表示结束）:" << endl;
    string content;
    string line;
    while (true) {
        getline(cin, line);
        if (line == "END") {
            break;
        }
        content += line + "\n";
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == openFileName) {
            file->content.insert(file->readWritePointer, content);
            file->readWritePointer += content.size(); // 更新读写指针位置
            cout << "文件写入成功。" << endl;
            return;
        }
    }
    cout << "文件不存在。" << endl;
}




// 读取文件
void readFile() {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "当前没有打开的文件。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == openFileName) {
            if (file->readWritePointer >= file->content.size()) {
                cout << "文件内容读取完毕。" << endl;
            }
            else {
                cout << "文件内容:\n" << file->content.substr(file->readWritePointer) << endl;
                file->readWritePointer = file->content.size(); // 更新读写指针位置到文件尾端
            }
            return;
        }
    }
    cout << "文件不存在。" << endl;
}

// 显示所有用户
void listUsers() {
    if (!diskData) {
        cout << "磁盘未初始化。" << endl;
        return;
    }
    cout << "系统用户列表:" << endl;
    for (const auto& user : diskData->users) {
        cout << "  用户名: " << user.first << " 密码: " << user.second->password << endl;
    }
}

// 判断文件名是否合法
bool isValidName(const string& name) {
    // 检查名称是否为空或包含不允许的字符
    if (name.empty() || name.find_first_of("\\/:*?\"<>|") != string::npos) {
        return false;
    }
    return true;
}

// 拷贝文件
void copyFile(const string& filename) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == filename) {
            copiedFile = make_shared<FileControlBlock>(*file);
            cout << "文件 " << filename << " 已复制。" << endl;
            return;
        }
    }
    cout << "文件 " << filename << " 不存在。" << endl;
}

// 粘贴文件
void pasteFile() {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (!copiedFile) {
        cout << "当前没有被拷贝的文件。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == copiedFile->fileName) {
            cout << "文件 " << copiedFile->fileName << " 已存在。请选择: (1) 覆盖 (2) 取消粘贴: "<<endl;
            string  choice;
            getline(cin, choice); // 使用 getline 读取输入
            if (choice == "1") {
                *file = *copiedFile;
                cout << "文件 " << copiedFile->fileName << " 已覆盖。" << endl;
            }
            else if(choice == "2") {
                cout << "粘贴取消。" << endl;
            }
            return;
        }
    }
    // 如果没有重名文件，则直接粘贴
    currentDirectory->files.push_back(make_shared<FileControlBlock>(*copiedFile));
    cout << "文件 " << copiedFile->fileName << " 已粘贴。" << endl;
    copiedFile = nullptr; // 释放复制文件信息
}

// 获取当前路径
string getCurrentPath() {
    if (!currentUser) {
        return ">";
    }
    string path = "";
    auto dir = currentDirectory;
    while (dir && dir->fileControlBlock->fileName != "/") {
        path = dir->fileControlBlock->fileName + "\\" + path;
        dir = dir->parentDirectory.lock();
    }
    return path;
}

// 删除目录及其所有子目录和文件
void removeDirectory(const string& dirname) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (!isValidName(dirname)) {
        cout << "无效的目录名。" << endl;
        return;
    }
    auto it = std::find_if(currentDirectory->children.begin(), currentDirectory->children.end(),
        [&dirname](const shared_ptr<Directory>& dir) {
            return dir->fileControlBlock->fileName == dirname;
        });
    if (it == currentDirectory->children.end()) {
        cout << "目录不存在。" << endl;
        return;
    }
    currentDirectory->children.erase(it);
    cout << "目录删除成功。" << endl;
    saveDisk(SAVE_PATH); // 删除目录后立即保存磁盘状态
}


//移动文件
void moveFile(const string& filename, const string& destDir) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }

    shared_ptr<FileControlBlock> fileToMove = nullptr;
    auto fileIt = currentDirectory->files.end();

    // 查找文件并移除
    for (auto it = currentDirectory->files.begin(); it != currentDirectory->files.end(); ++it) {
        if ((*it)->fileName == filename) {
            fileToMove = *it;
            fileIt = it;
            break;
        }
    }

    if (!fileToMove) {
        cout << "文件不存在。" << endl;
        return;
    }

    shared_ptr<Directory> destDirectory = nullptr;

    // 处理移动到上一级目录的情况
    if (destDir == "..") {
        destDirectory = currentDirectory->parentDirectory.lock();
        if (!destDirectory) {
            cout << "当前目录已是根目录，无法再向上一级。" << endl;
            return;
        }
    }
    else {
        // 查找目标目录
        for (const auto& dir : currentDirectory->children) {
            if (dir->fileControlBlock->fileName == destDir) {
                destDirectory = dir;
                break;
            }
        }

        if (!destDirectory) {
            cout << "目标目录不存在。" << endl;
            return;
        }
    }

    // 检查目标目录是否已有重名文件
    for (const auto& file : destDirectory->files) {
        if (file->fileName == filename) {
            cout << "目标目录中已存在文件 " << filename << "。请选择: (1) 覆盖 (2) 取消移动: "<<endl;
            int choice;
           
            cin >> choice;
            if (choice == 1) {
                // 覆盖文件
                *file = *fileToMove;
                currentDirectory->files.erase(fileIt);
                cout << "文件移动并覆盖成功。" << endl;
                saveDisk(SAVE_PATH);
                return;
            }
            else if (choice == 2) {
                cout << "移动取消。" << endl;
                return;
            }
        }
    }

    // 目标目录没有重名文件，直接移动
    destDirectory->files.push_back(fileToMove);
    currentDirectory->files.erase(fileIt);

    cout << "文件移动成功。" << endl;
    saveDisk(SAVE_PATH);
}


void openFile(const string& filename) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == filename) {
            if (file->isLocked) {
                cout << "文件 " << filename << " 已被锁定，无法打开。" << endl;
                return;
            }
            openFiles.insert(filename);
            openFileName = filename;
            file->readWritePointer = 0; // 读指针指向文件首端
            cout << "文件 " << filename << " 已打开。" << endl;
            return;
        }
    }
    cout << "文件不存在。" << endl;
}


void closeFile() {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "当前没有打开的文件。" << endl;
        return;
    }
    openFiles.erase(openFileName);
    openFileName = "";
    cout << "文件已关闭。" << endl;
    saveDisk(SAVE_PATH);
}

void lseekFile(int offset) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "当前没有打开的文件。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == openFileName) {
            if ((int)file->readWritePointer + offset < 0 || file->readWritePointer + offset > file->content.size()) {
                cout << "无效的移动量。" << endl;
            }
            else {
                file->readWritePointer += offset;
                cout << "文件读写指针已移动到位置 " << file->readWritePointer << endl;
            }
            return;
        }
    }
    cout << "文件不存在。" << endl;
}

void flockFile(const string& filename) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == filename) {
            if (file->isLocked) {
                file->isLocked = false;
                cout << "文件 " << filename << " 已解锁。" << endl;
            }
            else {
                file->isLocked = true;
                cout << "文件 " << filename << " 已加锁。" << endl;
            }
            return;
        }
    }
    cout << "文件不存在。" << endl;
}


void headFile(int num) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "当前没有打开的文件。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == openFileName) {
            istringstream stream(file->content);
            string line;
            int lineCount = 0;
            while (lineCount < num && getline(stream, line)) {
                cout << line << endl;
                lineCount++;
            }
            return;
        }
    }
    cout << "文件不存在。" << endl;
}

void tailFile(int num) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "当前没有打开的文件。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == openFileName) {
            istringstream stream(file->content);
            vector<string> lines;
            string line;
            while (getline(stream, line)) {
                lines.push_back(line);
            }
            int totalLines = lines.size();
            int startLine = max(0, totalLines - num);
            for (int i = startLine; i < totalLines; i++) {
                cout << lines[i] << endl;
            }
            return;
        }
    }
    cout << "文件不存在。" << endl;
}

void importFile(const string& localPath, const string& virtualName) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }

    // 调试信息：显示试图打开的文件路径
    cout << "试图打开本地文件: " << localPath << endl;

    ifstream inFile(localPath, ios::binary);
    if (!inFile) {
        cout << "无法打开本地文件 " << localPath << endl;
        return;
    }

    stringstream buffer;
    buffer << inFile.rdbuf();
    string content = buffer.str();
    inFile.close();

    if (!isValidName(virtualName)) {
        cout << "无效的文件名。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == virtualName) {
            cout << "文件已存在。" << endl;
            return;
        }
    }
    auto fcb = make_shared<FileControlBlock>();
    fcb->fileName = virtualName;
    fcb->isDirectory = false;
    fcb->content = content;
    fcb->readWritePointer = 0;
    fcb->isLocked = false;
    currentDirectory->files.push_back(fcb);
    cout << "文件 " << virtualName << " 已成功导入虚拟磁盘的当前目录。" << endl;
}



void exportFile(const string& virtualName, const string& localPath) {
    if (!currentUser) {
        cout << "请先登录。" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == virtualName) {
            ofstream outFile(localPath + "\\" + virtualName, ios::binary);
            if (!outFile) {
                cout << "无法创建本地文件 " << localPath << "\\" << virtualName << endl;
                return;
            }
            outFile << file->content;
            outFile.close();
            cout << "文件 " << virtualName << " 已成功导出到 " << localPath << endl;
            return;
        }
    }
    cout << "文件 " << virtualName << " 不存在于虚拟磁盘。" << endl;
}

const string GREEN_TEXT = "\033[1;32m";
const string RESET_TEXT = "\033[0m";

// 显示提示符
void showPrompt() {
    if (currentUser) {
        cout << GREEN_TEXT << currentUser->username << "\\" << getCurrentPath();
        if (!openFileName.empty()) {
            cout << openFileName;
        }
        cout << "> " << RESET_TEXT;
    }
    else {
        cout << GREEN_TEXT << "> " << RESET_TEXT;
    }
}

// 用户交互线程
void userInteraction() {
    string input;
    int first = 0;
    while (!exitFlag) {
        if (first == 0) {
            cout << GREEN_TEXT << ">" << RESET_TEXT;
            first = 1;
        }
        getline(cin, input);
        {
            std::lock_guard<std::mutex> lock(diskMutex);
            if (input == "exit") {
                exitFlag = true;
                break;
            }
            commandQueue.push(input);
        }

        cv.notify_one();

    }
}

void diskOperation() {
    while (!exitFlag) {
        string command;
        {
            std::unique_lock<std::mutex> lock(diskMutex);
            cv.wait(lock, [] { return !commandQueue.empty() || exitFlag; });
            if (exitFlag && commandQueue.empty()) break;
            command = commandQueue.front();
            commandQueue.pop();
            lock.unlock();
        }
        {
            std::lock_guard<std::mutex> disklock(diskMutex);

            string savedOpenFileName = openFileName;
            size_t savedReadWritePointer = 0;
            auto savedCopiedFile = copiedFile;

            if (!savedOpenFileName.empty()) {
                for (const auto& file : currentDirectory->files) {
                    if (file->fileName == savedOpenFileName) {
                        savedReadWritePointer = file->readWritePointer;
                        break;
                    }
                }
            }

            reloadDisk(SAVE_PATH);

            if (!savedOpenFileName.empty()) {
                for (const auto& file : currentDirectory->files) {
                    if (file->fileName == savedOpenFileName) {
                        openFiles.insert(savedOpenFileName);
                        openFileName = savedOpenFileName;
                        file->readWritePointer = savedReadWritePointer;
                        break;
                    }
                }
            }

            if (savedCopiedFile) {
                copiedFile = savedCopiedFile;
            }

            vector<string> commandTokens = inputResolve(command);
            if (commandTokens.empty()) continue;

            if (!openFileName.empty()) {
                // Handle file operations
                if (commandTokens[0] == "read") readFile();
                else if (commandTokens[0] == "write") writeFile();
                else if (commandTokens[0] == "close") closeFile();
                else if (commandTokens[0] == "lseek") {
                    if (commandTokens.size() < 2) {
                        cout << "用法: lseek <移动量>\n";
                        continue;
                    }
                    int offset = stoi(commandTokens[1]);
                    lseekFile(offset);
                }
                else if (commandTokens[0] == "head") {
                    if (commandTokens.size() < 2) {
                        cout << "用法: head <行数>\n";
                        continue;
                    }
                    int num = stoi(commandTokens[1]);
                    headFile(num);
                }
                else if (commandTokens[0] == "tail") {
                    if (commandTokens.size() < 2) {
                        cout << "用法: tail <行数>\n";
                        continue;
                    }
                    int num = stoi(commandTokens[1]);
                    tailFile(num);
                }
                else {
                    cout << "当前有文件打开，只能使用 read、write、close、lseek、head 和 tail 命令。\n";
                }
                saveDisk(SAVE_PATH);
                showPrompt(); // 显示提示符
                continue;
            }

            if (commandTokens[0] == "register") {
                if (commandTokens.size() < 3) {
                    cout << "用法: register <用户名> <密码>\n";
                    continue;
                }
                registerUser(commandTokens[1], commandTokens[2]);
            }
            else if (commandTokens[0] == "login") {
                if (commandTokens.size() < 3) {
                    cout << "用法: login <用户名> <密码>\n";
                    continue;
                }
                loginUser(commandTokens[1], commandTokens[2]);
            }
            else if (commandTokens[0] == "logout") {
                logoutUser();
            }
            else if (commandTokens[0] == "mkdir") {
                if (commandTokens.size() < 2) {
                    cout << "用法: mkdir <目录名>\n";
                    continue;
                }
                makeDirectory(commandTokens[1]);
            }
            else if (commandTokens[0] == "cd") {
                if (commandTokens.size() < 2) {
                    cout << "用法: cd <目录名>\n";
                    continue;
                }
                changeDirectory(commandTokens[1]);
            }
            else if (commandTokens[0] == "dir") showDirectory();
            else if (commandTokens[0] == "create") {
                if (commandTokens.size() < 2) {
                    cout << "用法: create <文件名>\n";
                    continue;
                }
                createFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "delete") {
                if (commandTokens.size() < 2) {
                    cout << "用法: delete <文件名>\n";
                    continue;
                }
                deleteFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "help") printHelp();
            else if (commandTokens[0] == "listUsers") listUsers();
            else if (commandTokens[0] == "copy") {
                if (commandTokens.size() < 2) {
                    cout << "用法: copy <文件名>\n";
                    continue;
                }
                copyFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "paste") pasteFile();
            else if (commandTokens[0] == "rmdir") {
                if (commandTokens.size() < 2) {
                    cout << "用法: rmdir <目录名>\n";
                    continue;
                }
                removeDirectory(commandTokens[1]);
            }
            else if (commandTokens[0] == "move") {
                if (commandTokens.size() < 3) {
                    cout << "用法: move <文件名> <目标目录>\n";
                    continue;
                }
                moveFile(commandTokens[1], commandTokens[2]);
            }
            else if (commandTokens[0] == "open") {
                if (commandTokens.size() < 2) {
                    cout << "用法: open <文件名>\n";
                    continue;
                }
                openFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "close") closeFile();
            else if (commandTokens[0] == "lseek") {
                if (commandTokens.size() < 2) {
                    cout << "用法: lseek <移动量>\n";
                    continue;
                }
                int offset = stoi(commandTokens[1]);
                lseekFile(offset);
            }
            else if (commandTokens[0] == "flock") {
                if (commandTokens.size() < 2) {
                    cout << "用法: flock <文件名>\n";
                    continue;
                }
                flockFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "head") {
                if (commandTokens.size() < 2) {
                    cout << "用法: head <行数>\n";
                    continue;
                }
                int num = stoi(commandTokens[1]);
                headFile(num);
            }
            else if (commandTokens[0] == "tail") {
                if (commandTokens.size() < 2) {
                    cout << "用法: tail <行数>\n";
                    continue;
                }
                int num = stoi(commandTokens[1]);
                tailFile(num);
            }
            else if (commandTokens[0] == "import") {
                if (commandTokens.size() < 3) {
                    cout << "用法: import <本地文件路径> <虚拟磁盘文件名>\n";
                    continue;
                }
                importFile(commandTokens[1], commandTokens[2]);
            }
            else if (commandTokens[0] == "export") {
                if (commandTokens.size() < 3) {
                    cout << "用法: export <虚拟磁盘文件名> <本地目录路径>\n";
                    continue;
                }
                exportFile(commandTokens[1], commandTokens[2]);
            }
            else {
                cout << "非法命令。请输入\"help\"查看可用命令。\n";
            }

            // 每次操作后保存磁盘
            saveDisk(SAVE_PATH);
            showPrompt(); // 显示提示符
        }
    }
}

