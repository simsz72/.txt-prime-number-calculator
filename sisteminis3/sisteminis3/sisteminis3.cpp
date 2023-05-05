#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <Windows.h>

using namespace std;

int minPrime = INT_MAX;
int maxPrime = INT_MIN;
DWORD waitResult;
queue<pair<string, int>> numQueue;
HANDLE mutexHandle;
string folderPath = "C:/Users/simon/Downloads/rand_files/file";
string currentFile;
string temp = "";

//tikrinama ar skaicius pirminis
bool isPrime(int n) {
    if (n <= 1) {
        return false;
    }
    for (int i = 2; i <= sqrt(n); i++) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

//kas 0,1 sekunde isvedamas apdorojamo failo pavadinimas
void printFilesProcessed() {
    while (true) {
        if (temp != currentFile)
        {
            cout << "At least one consumer is currently working on " << currentFile << endl;
            temp = currentFile;
        }
        if (currentFile == "file1000")
            break;
        Sleep(100);
    }
}

//produceryje atidaromi failai ir sudedami skaiciai i eile (su failo pavadinimu)
DWORD WINAPI producer(LPVOID lpParam) {
    for (int i = 1; i <= 1000; i++) { //bus perziurima 1000 failu
        string filePath = folderPath + to_string(i) + ".txt"; //nurodomas path ir failo vardas
        string txtName = "file" + to_string(i);
        ifstream file(filePath);
        if (!file.is_open()) { //jei failo atidaryti neina (arba jo nera), metamas klaidos pranesimas
            cerr << "Error opening file " << filePath << endl;
            continue;
        }
        else
        {
            int num;
            waitResult = WaitForSingleObject(mutexHandle, INFINITE); //uzrakinamas masyvas, kad butu galima saugiai sudeti skaicius i eile
            while (file >> num) {
                numQueue.push({ txtName, num });
            }
            file.close();
            ReleaseMutex(mutexHandle); //atrakinamas masyvas
        }
    }
    return 0;
}

//consumeris ima skaicius is skaiciu eiles, pasalina skaiciu is eiles ir tikrina ar jis pirminis... jei skaiciu eile yra tuscia ir mutexas gautas nelaukiant, consumeris baigia darba (nebebus skaiciu eileje)
DWORD WINAPI consumer(LPVOID lpParam) {
    pair<string, int> fileNum;
    bool done = false;
    while (!done) {
        waitResult = WaitForSingleObject(mutexHandle, INFINITE); //uzrakinamas masyvas
        if (numQueue.empty() && waitResult == WAIT_OBJECT_0) {
            if (done) {
                ReleaseMutex(mutexHandle);
                break;
            }
            ReleaseMutex(mutexHandle);
            Sleep(10); //jei eile yra tuscia ir nieks nelaukia mutexo, reiskiasi kad consumeriai dar dirba, del to duodamas laikas jiems pasivyti
        }
        else if (!numQueue.empty()) { //kol eile nera tuscia, imamas pirmas idetas elementas ir su juo atliekami veiksmai prime nustatymui
            fileNum = numQueue.front();
            numQueue.pop();
            ReleaseMutex(mutexHandle);
            if (isPrime(fileNum.second)) {
                WaitForSingleObject(mutexHandle, INFINITE);
                if (fileNum.second < minPrime) {
                    minPrime = fileNum.second;
                }
                if (fileNum.second > maxPrime) {
                    maxPrime = fileNum.second;
                }
                ReleaseMutex(mutexHandle);
            }
        }
        else {
            ReleaseMutex(mutexHandle);
        }
        waitResult = WaitForSingleObject(mutexHandle, INFINITE);
        if (numQueue.empty() && waitResult == WAIT_OBJECT_0) {
            done = true; //consumeris baigia darba
        }
        ReleaseMutex(mutexHandle);
        currentFile = fileNum.first; //parodo, kurioje vietoje yra consumeris. Naudojamas isvesti pereitu failu skaiciui.
    }
    return 0;
}


int main() {
    int numConsumers = 0;
    mutexHandle = CreateMutex(NULL, FALSE, NULL); //sukuriamas mutex handle, skirtas duomenu uzrakinimui ir sinchronizavimui (jei masyvas uzrakintas, kiti consumeriai negali juo naudotis ir laukia kol jis bus atrakintas).
    if (mutexHandle == NULL) {
        cerr << "Error creating mutex handle." << endl;
        return 0;
    }
    cout << "How much starting threads would you like?: ";
    cin >> numConsumers;
    HANDLE hProducer = CreateThread(NULL, 0, producer, NULL, 0, NULL); //sukuriamas producer handle

    vector<HANDLE> hConsumers;
    for (int i = 0; i < numConsumers; i++) { //sukuriama tiek consumer handles, kiek nurode vartotojas
        HANDLE hConsumer = CreateThread(NULL, 0, consumer, NULL, 0, NULL);
        hConsumers.push_back(hConsumer); //idedami i vektoriu
    }
    thread counterThread(printFilesProcessed); //sukuriama procesuotu failu gija
    WaitForSingleObject(hProducer, INFINITE);//palaukiama kol hproducer baigs darba
    while (!numQueue.empty()) //tol kol skaiciu eile nebus tuscia (consumeriai tures darbo), veiks ciklas, kuriame vartotojas gales atimti ir prideti consumeriu
    {
        int input = 0;
        cout << "Current number of consumer threads: " << numConsumers << endl;
        cout << "Enter number of threads to add (positive integer) or remove (negative integer): " << endl;
        cin >> input;
        if (input < 0 && numConsumers + input <= 0) { //jei norimas istrinti skaicius padarys consumerius skaicius mazesni arba lygu 0
            cout << "Error: cannot remove more threads than currently running." << endl;
        }
        else {
            for (int i = 0; i < abs(input); i++) {
                if (input > 0) {
                    HANDLE hConsumer = CreateThread(NULL, 0, consumer, NULL, 0, NULL); //prikuriami consumeriai
                    if (hConsumer == NULL) {
                        cerr << "Error creating new consumer thread " << i + 1 << endl;
                        continue;
                    }
                    hConsumers.push_back(hConsumer);
                    numConsumers++;
                }
                else {
                    try {
                        TerminateThread(hConsumers.back(), 0); //pasalinami consumeriai. svarbu pamineti, kad TerminateThread yra nepatartinas naudoti, kadangi jis iskart pasalina threada,
                        //ir nelaukia kol jis baigs darba. Taip gali but prarasti resursai ar skaiciavimai, duomenys ir gauti rezultatai.
                        CloseHandle(hConsumers.back()); //consumeris uzdaromas
                        hConsumers.pop_back(); //ir pasalinamas is vektoriaus
                        numConsumers--;
                    }
                    catch (...) {
                        cerr << "Error terminating thread." << endl;
                    }
                }
            }
        }
    };
    counterThread.join(); //uzdaroma gija
    WaitForMultipleObjects(hConsumers.size(), hConsumers.data(), TRUE, INFINITE); //teoriskai laukiama kol bus grazinami visi consumeriai
 
    if (WaitForSingleObject(mutexHandle, INFINITE) != WAIT_OBJECT_0) { //hei mutexhandle buvo grazintas nesekmingai
        cerr << "Error waiting for mutex handle." << endl;
    }
    CloseHandle(mutexHandle); //uzdaromi handles
    CloseHandle(hProducer);
    for (auto hConsumer : hConsumers) {
        CloseHandle(hConsumer);
    }


    if (minPrime == INT_MAX && maxPrime == INT_MIN) { //isspausidami atitinkami skaiciai min ir max arba jei nerasta, pranesama kad nerasta
        cout << "No prime numbers found." << endl;
    }
    else {
        cout << "Lowest prime number: " << minPrime << endl;
        cout << "Highest prime number: " << maxPrime << endl;
    }
    return 0;
}
