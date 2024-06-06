#include "File_System.h"

int main() {
    cout << "欢迎使用虚拟文件系统！\n";
    cout << "当前时间: " << getCurrentTime() << endl;
    cout << "是否初始化一个新的磁盘? (yes/no): ";
    string response;
    cin >> response;
    if (response == "yes") {
        initDisk();
        //saveDisk(SAVE_PATH); // 初始化后立即保存磁盘状态
    }
    else {
        if (!loadDisk(SAVE_PATH)) {
            cout << "磁盘数据加载失败。\n";
            return 1;
        }
        else {
            cout << "磁盘数据加载成功！" << endl;
        }
    }

    printHelp();
    cin.ignore();

    thread userThread(userInteraction);
    thread diskThread(diskOperation);

    userThread.join();
    diskThread.join();

    return 0;
}
