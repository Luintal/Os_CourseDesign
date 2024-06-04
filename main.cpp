#include "File_System.h"

int main() {
    cout << "�Ƿ��ʼ��һ���µĴ���? (yes/no): ";
    string response;
    cin >> response;
    if (response == "yes") {
        initDisk();
    }
    else {
        if (!loadDisk(SAVE_PATH)) {
            cout << "�������ݼ���ʧ�ܡ�\n";
            return 1;
        }
        else {
            cout << "�������ݼ��سɹ���" << endl;
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