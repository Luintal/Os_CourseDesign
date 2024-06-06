#include "File_System.h"

int main() {
    cout << "��ӭʹ�������ļ�ϵͳ��\n";
    cout << "��ǰʱ��: " << getCurrentTime() << endl;
    cout << "�Ƿ��ʼ��һ���µĴ���? (yes/no): ";
    string response;
    cin >> response;
    if (response == "yes") {
        initDisk();
        saveDisk(SAVE_PATH); // ��ʼ���������������״̬
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

    thread userThread(userInteraction);//�û��߳�
    thread diskThread(diskOperation);//�����߳�

    userThread.join();//�ȴ��û��߳̽���
    diskThread.join();//�ȴ������߳̽���

    return 0;
}
