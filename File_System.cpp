#include "File_System.h"

shared_ptr<Disk> diskData;//��������
shared_ptr<Directory> currentDirectory;//��ǰĿ¼
shared_ptr<User> currentUser;//��ǰ�û�
const string SAVE_PATH = "disk.dat";//����������ݵ��ļ�·��
shared_ptr<FileControlBlock> copiedFile = nullptr;//��ʼ��ȫ�ֱ��������濽�����ļ���Ϣ
set<string> openFiles; // ��ʼ���Ѵ��ļ��ļ���
string openFileName = ""; // ��ʼ����ǰ�򿪵��ļ���
mutex diskMutex;//��ʼ���������ݵĻ�����
condition_variable cv;//��ʼ����������
bool exitFlag = false;//��ʼ���Ƴ�����
queue<string> commandQueue;//��ʼ���������

// ��ӡ������Ϣ
void printHelp() {
    cout << "��������÷�:\n";
    cout << "  register <�û���> <����>                  - ע�����û�\n";
    cout << "  login <�û���> <����>                     - ��¼�û�\n";
    cout << "  logout                                    - �˳���ǰ�û�\n";
    cout << "  listUsers                                 - ��ʾ�˺�\n";
    cout << "  mkdir <Ŀ¼��>                            - ����Ŀ¼\n";
    cout << "  cd <Ŀ¼��>                               - �л�Ŀ¼\n";
    cout << "  rmdir <Ŀ¼��>                            - ɾ��Ŀ¼\n";
    cout << "  dir                                       - ��ʾ��ǰĿ¼����\n";
    cout << "  create <�ļ���>                           - �����ļ�\n";
    cout << "  delete <�ļ���>                           - ɾ���ļ�\n";
    cout << "  open <�ļ���>                             - ���ļ�\n";
    cout << "  close <�ļ���>                            - �ر��ļ�\n";
    cout << "  write                                     - д���ļ�\n";
    cout << "  read                                      - ��ȡ�ļ�����\n";
    cout << "  head <����>                               - ��ʾ�ļ�ͷ��\n";
    cout << "  tail <����>                               - ��ʾ�ļ�β��\n";
    cout << "  lseek <ƫ����>                            - �ƶ��ļ���дָ��\n";
    cout << "  move <�ļ���> <Ŀ��Ŀ¼>                  - �ƶ��ļ�\n";
    cout << "  copy <�ļ���>                             - �����ļ�\n";
    cout << "  paste                                     - ճ���ļ�\n";
    cout << "  flock <�ļ���>                            - ����/�����ļ�\n";
    cout << "  import <�����ļ�·��> <��������ļ���>    - �����ļ�\n";
    cout << "  export <��������ļ���> <����Ŀ¼·��>    - �����ļ�\n";
    cout << "  help                                      - ��ʾ������Ϣ\n";
    cout << "  exit                                      - �˳�����\n";
    cout << endl;
}

// �����û����������
vector<string> inputResolve(const string& input) {
    vector<string> result; // ���ڴ洢������ĵ���
    istringstream iss(input);// ʹ�� istringstream ���ַ���ת��Ϊ������
    string token; // ���ڴ洢������������ȡ��ÿ������
    while (iss >> token) {
        result.push_back(token);// ����ȡ�ĵ��ʼ���������
    }
    return result;
}

// �ݹ鱣��Ŀ¼���ļ�
void saveDirectory(ofstream& file, shared_ptr<Directory> directory) {
    // ��ȡ��ǰĿ¼����Ŀ¼�������ļ�����
    size_t dirCount = directory->children.size();
    size_t fileCount = directory->files.size();

    // ����Ŀ¼����д���ļ�
    file.write(reinterpret_cast<const char*>(&dirCount), sizeof(dirCount));

    // ����ÿ����Ŀ¼
    for (const auto& dir : directory->children) {
        // ��ȡ��Ŀ¼���Ƴ��Ȳ�д���ļ�
        size_t dirNameLen = dir->fileControlBlock->fileName.size();
        file.write(reinterpret_cast<const char*>(&dirNameLen), sizeof(dirNameLen));
        // д����Ŀ¼����
        file.write(dir->fileControlBlock->fileName.c_str(), dirNameLen);
        // �ݹ鱣����Ŀ¼
        saveDirectory(file, dir);
    }
    // ���ļ�����д���ļ�
    file.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

    // ����ÿ���ļ�
    for (const auto& fcb : directory->files) {
        // ��ȡ�ļ����Ƴ��Ȳ�д���ļ�
        size_t fileNameLen = fcb->fileName.size();
        file.write(reinterpret_cast<const char*>(&fileNameLen), sizeof(fileNameLen));
        // д���ļ�����
        file.write(fcb->fileName.c_str(), fileNameLen);
        // ��ȡ�ļ����ݳ��Ȳ�д���ļ�
        size_t contentLen = fcb->content.size();
        file.write(reinterpret_cast<const char*>(&contentLen), sizeof(contentLen));
        // д���ļ�����
        file.write(fcb->content.c_str(), contentLen);
        // д���ļ���дָ��λ��
        file.write(reinterpret_cast<const char*>(&fcb->readWritePointer), sizeof(fcb->readWritePointer));
        // д���ļ�����״̬
        file.write(reinterpret_cast<const char*>(&fcb->isLocked), sizeof(fcb->isLocked));
    }
}

// �ݹ����Ŀ¼���ļ��������ø�Ŀ¼ָ��
void loadDirectory(ifstream& file, shared_ptr<Directory> directory) {
    size_t dirCount, fileCount;// ��Ŀ¼�������ļ�����
    file.read(reinterpret_cast<char*>(&dirCount), sizeof(dirCount));// ��ȡ��ǰĿ¼����Ŀ¼����
    // ����ÿ����Ŀ¼
    for (size_t i = 0; i < dirCount; ++i) {
        string dirName;
        size_t dirNameLen;
        file.read(reinterpret_cast<char*>(&dirNameLen), sizeof(dirNameLen));// ��ȡ��Ŀ¼���Ƴ���
        dirName.resize(dirNameLen);
        file.read(&dirName[0], dirNameLen);
        // ������Ŀ¼������������
        auto dir = make_shared<Directory>();
        dir->fileControlBlock = make_shared<FileControlBlock>();
        dir->fileControlBlock->fileName = dirName;
        dir->fileControlBlock->isDirectory = true;
        dir->parentDirectory = directory; // ���ø�Ŀ¼ָ��
        // ����Ŀ¼���뵱ǰĿ¼����Ŀ¼�б�
        directory->children.push_back(dir);
        loadDirectory(file, dir); // �ݹ������Ŀ¼
    }
    // ��ȡ��ǰĿ¼���ļ�����
    file.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    for (size_t i = 0; i < fileCount; ++i) {// ����ÿ���ļ�
        string fileName, content;
        size_t fileNameLen, contentLen;
        // ��ȡ�ļ����Ƴ���
        file.read(reinterpret_cast<char*>(&fileNameLen), sizeof(fileNameLen));
        fileName.resize(fileNameLen);
        file.read(&fileName[0], fileNameLen);
        // ��ȡ�ļ����ݳ���
        file.read(reinterpret_cast<char*>(&contentLen), sizeof(contentLen));
        content.resize(contentLen);
        file.read(&content[0], contentLen);
        // �����ļ����ƿ鲢����������
        auto fcb = make_shared<FileControlBlock>();
        fcb->fileName = fileName;
        fcb->isDirectory = false;
        fcb->content = content;

        file.read(reinterpret_cast<char*>(&fcb->readWritePointer), sizeof(fcb->readWritePointer));
        file.read(reinterpret_cast<char*>(&fcb->isLocked), sizeof(fcb->isLocked));
        // ���ļ����ƿ���뵱ǰĿ¼���ļ��б�
        directory->files.push_back(fcb);
    }
}

// ����������ݵ��ļ�
bool saveDisk(const string& path) {
    ofstream file(path, ios::binary);
    if (!file.is_open()) {
        cerr << "�޷����ļ����б���: " << path << endl;
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
    //cout << "���̱���ɹ�: " << path << endl;
    return true;
}

//���ش�������
bool loadDisk(const string& path) {
    ifstream file(path, ios::binary);
    if (!file.is_open()) {
        cerr << "�޷��򿪴��̽��м���: " << path << endl;
        return false;
    }

    // �����������
    diskData = make_shared<Disk>();
    size_t usersCount;
    file.read(reinterpret_cast<char*>(&usersCount), sizeof(usersCount));

    for (size_t i = 0; i < usersCount; ++i) {
        string username, password;
        size_t usernameLen, passwordLen;

        // ��ȡ�û���������
        file.read(reinterpret_cast<char*>(&usernameLen), sizeof(usernameLen));
        username.resize(usernameLen);
        file.read(&username[0], usernameLen);

        file.read(reinterpret_cast<char*>(&passwordLen), sizeof(passwordLen));
        password.resize(passwordLen);
        file.read(&password[0], passwordLen);

        auto user = make_shared<User>();
        user->username = username;
        user->password = password;

        // �����û���Ŀ¼
        user->rootDirectory = make_shared<Directory>();
        auto rootDir = user->rootDirectory;
        rootDir->fileControlBlock = make_shared<FileControlBlock>();
        rootDir->fileControlBlock->fileName = "/";
        rootDir->fileControlBlock->isDirectory = true;
        rootDir->parentDirectory.reset(); // ��Ŀ¼û�и�Ŀ¼

        // �����û�Ŀ¼���ļ�
        loadDirectory(file, rootDir);

        diskData->users[username] = user;
    }

    file.close();
    //cout << "���̼��سɹ�: " << path << endl;
    return true;
}

// ���¼��ش���
void reloadDisk(const string& path) {
    // ���浱ǰ�û���Ϣ��Ŀ¼��Ϣ�����ļ�����Ϣ�������ļ�����Ϣ
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

    // ���ȫ�ֱ����������ݴ���
    diskData = nullptr;
    currentDirectory = nullptr;
    currentUser = nullptr;
    copiedFile = nullptr;
    openFiles.clear();
    openFileName = "";

    // ���¼��ش�������
    if (!loadDisk(path)) {
        cout << "���¼��ش�������ʧ�ܡ�" << endl;
        return;
    }

    // �ָ���ǰ�û���Ϣ��Ŀ¼��Ϣ
    if (savedUser) {
        currentUser = diskData->users[savedUser->username];
        if (currentUser) {
            // ���Ҳ��ָ���ǰĿ¼
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

    // �ָ��򿪵��ļ���Ϣ
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

    // �ָ������ļ�����Ϣ
    if (savedCopiedFile) {
        copiedFile = savedCopiedFile;
    }

    //cout << "�������������¼��ء�" << endl;
}



// ��ȡ��ǰʱ���ַ���
string getCurrentTime() {
    time_t now = time(0);// ��ȡ��ǰʱ���
    tm localtm;
    localtime_s(&localtm, &now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localtm);
    return string(buffer);
}

// ��ʼ������
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
    cout << "�´����ѳ�ʼ����" << endl;
}

// �û�ע��
void registerUser(const string& username, const string& password) {
    if (diskData->users.find(username) != diskData->users.end()) {
        cout << "�û����Ѵ��ڡ�" << endl;
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
    cout << "�û�ע��ɹ���" << endl;
}

// �û���¼
bool loginUser(const string& username, const string& password) {
    if (diskData->users.find(username) == diskData->users.end() || diskData->users[username]->password != password) {
        cout << "�û������������" << endl;
        return false;
    }
    currentUser = diskData->users[username];
    currentDirectory = currentUser->rootDirectory;
    cout << "�û���¼�ɹ���" << endl;
    return true;
}

// �û�ע��
void logoutUser() {
    if (!currentUser) {
        cout << "��ǰû���û���¼��" << endl;
        return;
    }
    currentUser = nullptr;
    currentDirectory = nullptr;
    cout << "�û���ע����" << endl;
}

// ����Ŀ¼
void makeDirectory(const string& dirname) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (!isValidName(dirname)) {
        cout << "��Ч��Ŀ¼����" << endl;
        return;
    }
    for (const auto& dir : currentDirectory->children) {
        if (dir->fileControlBlock->fileName == dirname) {
            cout << "Ŀ¼�Ѵ��ڡ�" << endl;
            return;
        }
    }
    auto dir = make_shared<Directory>();
    dir->fileControlBlock = make_shared<FileControlBlock>();
    dir->fileControlBlock->fileName = dirname;
    dir->fileControlBlock->isDirectory = true;
    dir->parentDirectory = currentDirectory; // ���ø�Ŀ¼ָ�롣 ����Ŀ¼��ӵ���ǰĿ¼����Ŀ¼�б���
    currentDirectory->children.push_back(dir);
    cout << "Ŀ¼�����ɹ���" << endl;
    saveDisk(SAVE_PATH);
}


// �л�Ŀ¼
void changeDirectory(const string& dirname) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (dirname == "..") {
        if (auto parent = currentDirectory->parentDirectory.lock()) {
            currentDirectory = parent;
        }
        else {
            cout << "���ڸ�Ŀ¼���޷�������һ����" << endl;
        }
        return;
    }
    if (!isValidName(dirname)) {
        cout << "��Ч��Ŀ¼����" << endl;
        return;
    }
    for (const auto& dir : currentDirectory->children) {// ������ǰĿ¼����Ŀ¼
        if (dir->fileControlBlock->fileName == dirname) {
            currentDirectory = dir;
            return;
        }
    }
    cout << "Ŀ¼�����ڡ�" << endl;
}

// ��ʾ��ǰĿ¼����
void showDirectory() {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }

    if (currentDirectory == currentUser->rootDirectory) {
        // ��ǰ���û���Ŀ¼�£����������Ŀ¼�������������ļ���
        cout << "�û� " << currentUser->username << " ��Ŀ¼�µ�Ŀ¼: ";
        for (const auto& dir : currentDirectory->children) {
            cout << dir->fileControlBlock->fileName << " ";
        }
        cout << endl << "�ļ�: ";
        for (const auto& file : currentDirectory->files) {
            cout << file->fileName << " ";
        }
        cout << endl;
    }
    else {
        // ��ǰ����Ŀ¼�£������ǰĿ¼�������������ļ���
        cout << "��ǰĿ¼: " << currentDirectory->fileControlBlock->fileName << endl;
        cout << "Ŀ¼: ";
        for (const auto& dir : currentDirectory->children) {
            cout << dir->fileControlBlock->fileName << " ";
        }
        cout << endl << "�ļ�: ";
        for (const auto& file : currentDirectory->files) {
            cout << file->fileName << " ";
        }
        cout << endl;
    }
}


// �����ļ�
void createFile(const string& filename) {

    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (!isValidName(filename)) {
        cout << "��Ч���ļ�����" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == filename) {
            cout << "�ļ��Ѵ��ڡ�" << endl;
            return;
        }
    }
    auto fcb = make_shared<FileControlBlock>();
    fcb->fileName = filename;
    fcb->isDirectory = false;
    fcb->readWritePointer = 0; // ��ʼ����дָ��
    fcb->isLocked = false; // ��ʼ������״̬
    currentDirectory->files.push_back(fcb);
    cout << "�ļ������ɹ���" << endl;

}




// ɾ���ļ�
void deleteFile(const string& filename) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (!isValidName(filename)) {
        cout << "��Ч���ļ�����" << endl;
        return;
    }
    for (auto it = currentDirectory->files.begin(); it != currentDirectory->files.end(); ++it) {
        if ((*it)->fileName == filename) {
            currentDirectory->files.erase(it);
            cout << "�ļ�ɾ���ɹ���" << endl;
            return;
        }
    }
    cout << "�ļ������ڡ�" << endl;
    saveDisk(SAVE_PATH);
}


// д���ļ�
void writeFile() {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "��ǰû�д򿪵��ļ���" << endl;
        return;
    }
    cout << "��ӵڶ��п�ʼ�������ݣ����� 'END' ��ʾ������:" << endl;
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
            file->readWritePointer += content.size(); // ���¶�дָ��λ��
            cout << "�ļ�д��ɹ���" << endl;
            return;
        }
    }
    cout << "�ļ������ڡ�" << endl;
}




// ��ȡ�ļ�
void readFile() {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "��ǰû�д򿪵��ļ���" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == openFileName) {
            if (file->readWritePointer >= file->content.size()) {
                cout << "�ļ����ݶ�ȡ��ϡ�" << endl;
            }
            else {
                cout << "�ļ�����:\n" << file->content.substr(file->readWritePointer) << endl;
                file->readWritePointer = file->content.size(); // ���¶�дָ��λ�õ��ļ�β��
            }
            return;
        }
    }
    cout << "�ļ������ڡ�" << endl;
}

// ��ʾ�����û�
void listUsers() {
    if (!diskData) {
        cout << "����δ��ʼ����" << endl;
        return;
    }
    cout << "ϵͳ�û��б�:" << endl;
    for (const auto& user : diskData->users) {
        cout << "  �û���: " << user.first << " ����: " << user.second->password << endl;
    }
}

// �ж��ļ����Ƿ�Ϸ�
bool isValidName(const string& name) {
    // ��������Ƿ�Ϊ�ջ������������ַ�
    if (name.empty() || name.find_first_of("\\/:*?\"<>|") != string::npos) {
        return false;
    }
    return true;
}

// �����ļ�
void copyFile(const string& filename) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == filename) {
            copiedFile = make_shared<FileControlBlock>(*file);
            cout << "�ļ� " << filename << " �Ѹ��ơ�" << endl;
            return;
        }
    }
    cout << "�ļ� " << filename << " �����ڡ�" << endl;
}

// ճ���ļ�
void pasteFile() {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (!copiedFile) {
        cout << "��ǰû�б��������ļ���" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == copiedFile->fileName) {
            cout << "�ļ� " << copiedFile->fileName << " �Ѵ��ڡ���ѡ��: (1) ���� (2) ȡ��ճ��: "<<endl;
            string  choice;
            getline(cin, choice); // ʹ�� getline ��ȡ����
            if (choice == "1") {
                *file = *copiedFile;
                cout << "�ļ� " << copiedFile->fileName << " �Ѹ��ǡ�" << endl;
            }
            else if(choice == "2") {
                cout << "ճ��ȡ����" << endl;
            }
            return;
        }
    }
    // ���û�������ļ�����ֱ��ճ��
    currentDirectory->files.push_back(make_shared<FileControlBlock>(*copiedFile));
    cout << "�ļ� " << copiedFile->fileName << " ��ճ����" << endl;
    copiedFile = nullptr; // �ͷŸ����ļ���Ϣ
}

// ��ȡ��ǰ·��
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

// ɾ��Ŀ¼����������Ŀ¼���ļ�
void removeDirectory(const string& dirname) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (!isValidName(dirname)) {
        cout << "��Ч��Ŀ¼����" << endl;
        return;
    }
    auto it = std::find_if(currentDirectory->children.begin(), currentDirectory->children.end(),
        [&dirname](const shared_ptr<Directory>& dir) {
            return dir->fileControlBlock->fileName == dirname;
        });
    if (it == currentDirectory->children.end()) {
        cout << "Ŀ¼�����ڡ�" << endl;
        return;
    }
    currentDirectory->children.erase(it);
    cout << "Ŀ¼ɾ���ɹ���" << endl;
    saveDisk(SAVE_PATH); // ɾ��Ŀ¼�������������״̬
}


//�ƶ��ļ�
void moveFile(const string& filename, const string& destDir) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }

    shared_ptr<FileControlBlock> fileToMove = nullptr;
    auto fileIt = currentDirectory->files.end();

    // �����ļ����Ƴ�
    for (auto it = currentDirectory->files.begin(); it != currentDirectory->files.end(); ++it) {
        if ((*it)->fileName == filename) {
            fileToMove = *it;
            fileIt = it;
            break;
        }
    }

    if (!fileToMove) {
        cout << "�ļ������ڡ�" << endl;
        return;
    }

    shared_ptr<Directory> destDirectory = nullptr;

    // �����ƶ�����һ��Ŀ¼�����
    if (destDir == "..") {
        destDirectory = currentDirectory->parentDirectory.lock();
        if (!destDirectory) {
            cout << "��ǰĿ¼���Ǹ�Ŀ¼���޷�������һ����" << endl;
            return;
        }
    }
    else {
        // ����Ŀ��Ŀ¼
        for (const auto& dir : currentDirectory->children) {
            if (dir->fileControlBlock->fileName == destDir) {
                destDirectory = dir;
                break;
            }
        }

        if (!destDirectory) {
            cout << "Ŀ��Ŀ¼�����ڡ�" << endl;
            return;
        }
    }

    // ���Ŀ��Ŀ¼�Ƿ����������ļ�
    for (const auto& file : destDirectory->files) {
        if (file->fileName == filename) {
            cout << "Ŀ��Ŀ¼���Ѵ����ļ� " << filename << "����ѡ��: (1) ���� (2) ȡ���ƶ�: "<<endl;
            int choice;
           
            cin >> choice;
            if (choice == 1) {
                // �����ļ�
                *file = *fileToMove;
                currentDirectory->files.erase(fileIt);
                cout << "�ļ��ƶ������ǳɹ���" << endl;
                saveDisk(SAVE_PATH);
                return;
            }
            else if (choice == 2) {
                cout << "�ƶ�ȡ����" << endl;
                return;
            }
        }
    }

    // Ŀ��Ŀ¼û�������ļ���ֱ���ƶ�
    destDirectory->files.push_back(fileToMove);
    currentDirectory->files.erase(fileIt);

    cout << "�ļ��ƶ��ɹ���" << endl;
    saveDisk(SAVE_PATH);
}

//���ļ�
void openFile(const string& filename) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == filename) {
            if (file->isLocked) {
                cout << "�ļ� " << filename << " �ѱ��������޷��򿪡�" << endl;
                return;
            }
            openFiles.insert(filename);
            openFileName = filename;
            file->readWritePointer = 0; // ��ָ��ָ���ļ��׶�
            cout << "�ļ� " << filename << " �Ѵ򿪡�" << endl;
            return;
        }
    }
    cout << "�ļ������ڡ�" << endl;
}

//�ر��ļ�
void closeFile() {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "��ǰû�д򿪵��ļ���" << endl;
        return;
    }
    openFiles.erase(openFileName);
    openFileName = "";
    cout << "�ļ��ѹرա�" << endl;
    saveDisk(SAVE_PATH);
}

// �ƶ��ļ���дָ��
void lseekFile(int offset) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "��ǰû�д򿪵��ļ���" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == openFileName) {
            if ((int)file->readWritePointer + offset < 0 || file->readWritePointer + offset > file->content.size()) {
                cout << "��Ч���ƶ�����" << endl;
            }
            else {
                file->readWritePointer += offset;
                cout << "�ļ���дָ�����ƶ���λ�� " << file->readWritePointer << endl;
            }
            return;
        }
    }
    cout << "�ļ������ڡ�" << endl;
}

//�ļ�����������
void flockFile(const string& filename) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == filename) {
            if (file->isLocked) {
                file->isLocked = false;
                cout << "�ļ� " << filename << " �ѽ�����" << endl;
            }
            else {
                file->isLocked = true;
                cout << "�ļ� " << filename << " �Ѽ�����" << endl;
            }
            return;
        }
    }
    cout << "�ļ������ڡ�" << endl;
}

// ��ͷ��ʾ�ļ�����
void headFile(int num) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "��ǰû�д򿪵��ļ���" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {// ������ǰĿ¼�е��ļ������Ҵ򿪵��ļ�
        if (file->fileName == openFileName) {
            istringstream stream(file->content);
            string line;
            int lineCount = 0;
            while (lineCount < num && getline(stream, line)) {// ��ȡ�����ǰ num ������
                cout << line << endl;
                lineCount++;
            }
            return;
        }
    }
    cout << "�ļ������ڡ�" << endl;
}

//��β��ʾ�ļ�����
void tailFile(int num) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    if (openFileName.empty()) {
        cout << "��ǰû�д򿪵��ļ���" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == openFileName) {
            istringstream stream(file->content);
            vector<string> lines;
            string line;
            while (getline(stream, line)) {// �������ж�������
                lines.push_back(line);
            }
            int totalLines = lines.size();
            int startLine = max(0, totalLines - num);// ���㿪ʼ��ʾ���к�
            for (int i = startLine; i < totalLines; i++) {      // ������ num ������
                cout << lines[i] << endl;
            }
            return;
        }
    }
    cout << "�ļ������ڡ�" << endl;
}

//�����ļ�
void importFile(const string& localPath, const string& virtualName) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }

    // ������Ϣ����ʾ��ͼ�򿪵��ļ�·��
    cout << "��ͼ�򿪱����ļ�: " << localPath << endl;

    ifstream inFile(localPath, ios::binary);
    if (!inFile) {
        cout << "�޷��򿪱����ļ� " << localPath << endl;
        return;
    }
    // ��ȡ�ļ����ݵ��ַ�����
    stringstream buffer;
    buffer << inFile.rdbuf();
    string content = buffer.str();
    inFile.close();

    if (!isValidName(virtualName)) {
        cout << "��Ч���ļ�����" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == virtualName) {
            cout << "�ļ��Ѵ��ڡ�" << endl;
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
    cout << "�ļ� " << virtualName << " �ѳɹ�����������̵ĵ�ǰĿ¼��" << endl;
}


//�����ļ�
void exportFile(const string& virtualName, const string& localPath) {
    if (!currentUser) {
        cout << "���ȵ�¼��" << endl;
        return;
    }
    for (const auto& file : currentDirectory->files) {
        if (file->fileName == virtualName) {
            ofstream outFile(localPath + "\\" + virtualName, ios::binary);
            if (!outFile) {
                cout << "�޷����������ļ� " << localPath << "\\" << virtualName << endl;
                return;
            }
            outFile << file->content;
            outFile.close();
            cout << "�ļ� " << virtualName << " �ѳɹ������� " << localPath << endl;
            return;
        }
    }
    cout << "�ļ� " << virtualName << " ��������������̡�" << endl;
}

const string GREEN_TEXT = "\033[1;32m";
const string RESET_TEXT = "\033[0m";

// ��ʾ��ʾ��
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

// �û������߳�
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
            std::lock_guard<std::mutex> lock(diskMutex);// ������ȷ���̰߳�ȫ
            if (input == "exit") {
                exitFlag = true;
                break;
            }
            commandQueue.push(input);// ������������
        }

        cv.notify_one();// ֪ͨ���̲����߳���������

    }
}

void diskOperation() {
    while (!exitFlag) {
        string command;
        {
            std::unique_lock<std::mutex> lock(diskMutex);// ������ȷ���̰߳�ȫ
            cv.wait(lock, [] { return !commandQueue.empty() || exitFlag; });// �ȴ���������˳���־
            if (exitFlag && commandQueue.empty()) break; // ����˳���־�������������Ϊ�գ����˳�
            command = commandQueue.front();// ��ȡ����
            commandQueue.pop();// �Ƴ�����
            lock.unlock();
        }
        {
            std::lock_guard<std::mutex> disklock(diskMutex);// ������ȷ���̰߳�ȫ
            // ���浱ǰ���ļ�����Ϣ
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
            // ���¼��ش���
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
                
                if (commandTokens[0] == "read") readFile();
                else if (commandTokens[0] == "write") writeFile();
                else if (commandTokens[0] == "close") closeFile();
                else if (commandTokens[0] == "lseek") {
                    if (commandTokens.size() < 2) {
                        cout << "�÷�: lseek <�ƶ���>\n";
                        continue;
                    }
                    int offset = stoi(commandTokens[1]);
                    lseekFile(offset);
                }
                else if (commandTokens[0] == "head") {
                    if (commandTokens.size() < 2) {
                        cout << "�÷�: head <����>\n";
                        continue;
                    }
                    int num = stoi(commandTokens[1]);
                    headFile(num);
                }
                else if (commandTokens[0] == "tail") {
                    if (commandTokens.size() < 2) {
                        cout << "�÷�: tail <����>\n";
                        continue;
                    }
                    int num = stoi(commandTokens[1]);
                    tailFile(num);
                }
                else {
                    cout << "��ǰ���ļ��򿪣�ֻ��ʹ�� read��write��close��lseek��head �� tail ���\n";
                }
                saveDisk(SAVE_PATH);
                showPrompt(); // ��ʾ��ʾ��
                continue;
            }

            if (commandTokens[0] == "register") {
                if (commandTokens.size() < 3) {
                    cout << "�÷�: register <�û���> <����>\n";
                    continue;
                }
                registerUser(commandTokens[1], commandTokens[2]);
            }
            else if (commandTokens[0] == "login") {
                if (commandTokens.size() < 3) {
                    cout << "�÷�: login <�û���> <����>\n";
                    continue;
                }
                loginUser(commandTokens[1], commandTokens[2]);
            }
            else if (commandTokens[0] == "logout") {
                logoutUser();
            }
            else if (commandTokens[0] == "mkdir") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: mkdir <Ŀ¼��>\n";
                    continue;
                }
                makeDirectory(commandTokens[1]);
            }
            else if (commandTokens[0] == "cd") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: cd <Ŀ¼��>\n";
                    continue;
                }
                changeDirectory(commandTokens[1]);
            }
            else if (commandTokens[0] == "dir") showDirectory();
            else if (commandTokens[0] == "create") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: create <�ļ���>\n";
                    continue;
                }
                createFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "delete") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: delete <�ļ���>\n";
                    continue;
                }
                deleteFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "help") printHelp();
            else if (commandTokens[0] == "listUsers") listUsers();
            else if (commandTokens[0] == "copy") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: copy <�ļ���>\n";
                    continue;
                }
                copyFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "paste") pasteFile();
            else if (commandTokens[0] == "rmdir") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: rmdir <Ŀ¼��>\n";
                    continue;
                }
                removeDirectory(commandTokens[1]);
            }
            else if (commandTokens[0] == "move") {
                if (commandTokens.size() < 3) {
                    cout << "�÷�: move <�ļ���> <Ŀ��Ŀ¼>\n";
                    continue;
                }
                moveFile(commandTokens[1], commandTokens[2]);
            }
            else if (commandTokens[0] == "open") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: open <�ļ���>\n";
                    continue;
                }
                openFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "close") closeFile();
            else if (commandTokens[0] == "lseek") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: lseek <�ƶ���>\n";
                    continue;
                }
                int offset = stoi(commandTokens[1]);
                lseekFile(offset);
            }
            else if (commandTokens[0] == "flock") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: flock <�ļ���>\n";
                    continue;
                }
                flockFile(commandTokens[1]);
            }
            else if (commandTokens[0] == "head") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: head <����>\n";
                    continue;
                }
                int num = stoi(commandTokens[1]);
                headFile(num);
            }
            else if (commandTokens[0] == "tail") {
                if (commandTokens.size() < 2) {
                    cout << "�÷�: tail <����>\n";
                    continue;
                }
                int num = stoi(commandTokens[1]);
                tailFile(num);
            }
            else if (commandTokens[0] == "import") {
                if (commandTokens.size() < 3) {
                    cout << "�÷�: import <�����ļ�·��> <��������ļ���>\n";
                    continue;
                }
                importFile(commandTokens[1], commandTokens[2]);
            }
            else if (commandTokens[0] == "export") {
                if (commandTokens.size() < 3) {
                    cout << "�÷�: export <��������ļ���> <����Ŀ¼·��>\n";
                    continue;
                }
                exportFile(commandTokens[1], commandTokens[2]);
            }
            else {
                cout << "�Ƿ����������\"help\"�鿴�������\n";
            }

            // ÿ�β����󱣴����
            saveDisk(SAVE_PATH);
            showPrompt(); // ��ʾ��ʾ��
        }
    }
}

