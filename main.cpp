#include "File_System.h"

int main() {
    cout << "是否初始化一个新的磁盘? (yes/no): ";
    string response;
    cin >> response;
    if (response == "yes") {
        initDisk();
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