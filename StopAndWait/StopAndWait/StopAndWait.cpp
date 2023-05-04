#include <iostream>
#include <queue>
#include <mutex>
#include <chrono>
#include <random>
#include <thread>
#include <condition_variable>

struct Package
{
    int id;
};

void Sender(int& nrOfPackages, std::queue<Package>& packagesQueue, std::mutex& packagesQueueMutex, std::condition_variable& packageSent, std::condition_variable& ackReceived, bool& allPackagesReceived)
{
    for (int i = 1; i <= nrOfPackages; i++)
    {
        bool ackReceivedCurrentPackage = false;

        while (!ackReceivedCurrentPackage)
        {
            Package package = { i };
            {
                std::lock_guard<std::mutex> lock(packagesQueueMutex);

                packagesQueue.push(package);

                std::cout << "Sender: Sent package " << package.id << "." << std::endl;
            }

            packageSent.notify_one();

            std::unique_lock<std::mutex> lock(packagesQueueMutex);

            ackReceivedCurrentPackage = ackReceived.wait_for(lock, std::chrono::milliseconds(2000), [&]() { return !packagesQueue.empty() && packagesQueue.front().id == i; });

            if (!ackReceivedCurrentPackage)
            {
                std::cout << "Sender: Haven't received ACK for package " << package.id << ". Resending..." << std::endl;
            }
        }
    }

    allPackagesReceived = true;
}

void Receiver(int& lostPackageId, std::queue<Package>& packagesQueue, std::mutex& packagesQueueMutex, std::condition_variable& packageSent, std::condition_variable& ackReceived, bool& allPackagesReceived)
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(packagesQueueMutex);

        packageSent.wait(lock, [&]() { return !packagesQueue.empty(); });

        Package receivedPackage = packagesQueue.front();

        packagesQueue.pop();

        lock.unlock();

        if (receivedPackage.id == lostPackageId)
        {
            std::cout << "Receiver: Package " << receivedPackage.id << " was lost." << std::endl;

            lostPackageId = -1;

            continue;
        }

        std::cout << "Receiver: Received package " << receivedPackage.id << " and sent ACK." << std::endl;

        ackReceived.notify_one();

        if (receivedPackage.id == lostPackageId && lostPackageId == -1)
        {
            break;
        }

        if (allPackagesReceived == true)
        {
            break;
        }
    }
}

int main()
{
    int nrOfPackages;

    std::cout << "Number of packages that need to be sent: ";

    std::cin >> nrOfPackages;

    std::cout << std::endl;

    std::queue<Package> packagesQueue;
    std::mutex packagesQueueMutex;
    std::condition_variable packageSent;
    std::condition_variable ackReceived;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, nrOfPackages);

    int lostPackageId = dis(gen);

    bool allPackagesReceived = false;

    std::thread senderThread(Sender, std::ref(nrOfPackages), std::ref(packagesQueue), std::ref(packagesQueueMutex), std::ref(packageSent), std::ref(ackReceived), std::ref(allPackagesReceived));
    std::thread receiverThread(Receiver, std::ref(lostPackageId), std::ref(packagesQueue), std::ref(packagesQueueMutex), std::ref(packageSent), std::ref(ackReceived), std::ref(allPackagesReceived));

    senderThread.join();
    receiverThread.join();

    return 0;
}