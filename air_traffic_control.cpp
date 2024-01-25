#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <fstream>
#include <sstream>
#include <deque>
#include <random>

int s;
int t;
float p;
int a;
int n;
std::mutex randomMutex;
std::string planeLogPath = "planeLogs.log";
std::string tab = "\t";
volatile int index = 1;
volatile int counter = 5;
int maxWaitTime; // to avoid air starvation
std::mutex requestMutex;
std::mutex planeDataMutex;
volatile bool finishing = false;
std::deque<std::string> *planeLogStrings= new std::deque<std::string>();
bool debugging = false;
std::mt19937 *mt=NULL;
std::uniform_real_distribution<float> prob(0.0, 1.0);

std::string intToStr(int i) {
    return std::to_string(i);
}

int pthread_sleep (int seconds)
{
   pthread_mutex_t mutex;
   pthread_cond_t conditionvar;
   struct timespec timetoexpire;
   if(pthread_mutex_init(&mutex,NULL))
    {
      return -1;
    }
   if(pthread_cond_init(&conditionvar,NULL))
    {
      return -1;
    }
   struct timeval tp;
   //When to expire is an absolute time, so get the current time and add //it to our delay time
   gettimeofday(&tp, NULL);
   timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;

   pthread_mutex_lock (&mutex);
   int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
   pthread_mutex_unlock (&mutex);
   pthread_mutex_destroy(&mutex);
   pthread_cond_destroy(&conditionvar);

   //Upon successful completion, a value of zero shall be returned
   return res;

}

void planeData(int planeID, std::string status, time_t request, time_t runway) {
    if (!finishing) {
        std::stringstream planeData;
        struct tm requestInfo=*localtime(&request);
        struct tm runwayInfo=*localtime(&runway);
        planeData << tab << planeID << tab <<tab;
        planeData << status << tab << tab;
        planeData << intToStr(requestInfo.tm_hour) + ":" + intToStr(requestInfo.tm_min) + ":" + intToStr(requestInfo.tm_sec) << tab << tab << tab << tab;
        planeData << intToStr(runwayInfo.tm_hour) + ":" + intToStr(runwayInfo.tm_min) + ":" + intToStr(runwayInfo.tm_sec) << tab << tab << tab;
        planeData << ((int)(runway - request));

        planeDataMutex.lock();
        planeLogStrings->push_back(planeData.str());
        planeDataMutex.unlock();
    }
}

void dn(std::string str) {
    std::cout << str+"\n";
}

void parseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string examined = std::string(argv[i]);
        std::string searched = examined.substr(0, 2);
        if (searched.compare("s=")==0) {
            s = std::stoi(examined.substr(2, examined.length()));
            dn("set s to " + intToStr(s));
        }
        if (searched.compare("p=")==0) {
            p = std::stod(examined.substr(2, examined.length()));
            dn("set p to " + intToStr(p));
        }
        if (searched.compare("t=")==0) {
            t = std::stoi(examined.substr(2, examined.length()));
            dn("set t to " + intToStr(t));
        }
        if (searched.compare("n=")==0) {
            t = std::stoi(examined.substr(2, examined.length()));
            dn("set n to " + intToStr(t));
        }
    }
    
}

class Plane
{
public:
    int planeID=0;
    std::string status;
    time_t requestTime;
    time_t runwayTime;
    time_t turnaroundTime;

    Plane(int ID, std::string stat, time_t request, time_t runway) {
        planeID = ID;
        status = stat;
        requestTime = request;
        runwayTime = runway;
    }
    void turnAndGo() {
        planeData(planeID, status, requestTime, std::time(NULL));
        delete this;
    }
};

class Runway
{
public:
    std::deque<Plane*> departingPlaneQueue;
    std::deque<Plane*> landingPlaneQueue;
    std::deque<Plane*> planeQueue;
    std::deque<Plane*> emergencyPlaneQueue;
    float addProbability = 0.5;
    std::mutex planeQueueMutex;
    std::mutex departingPlaneQueueMutex;
    std::mutex landingPlaneQueueMutex;
    std::mutex emergencyPlaneQueueMutex;

    Runway() {
    }

    void run() {
        while (true) {
            addPlane();
            pthread_sleep(t);
            a = a + 1;
        }
    }

    bool addPlane(){
        randomMutex.lock();
        float rgen=prob(*mt);
        randomMutex.unlock();
        
        if(a %40 == 0 && a > 0){//emergency
            requestMutex.lock();
            int ID = index++;
            requestMutex.unlock();
            time_t arrival = std::time(NULL);
            addEmergencyPlane(new Plane(ID,"E", arrival, std::time(NULL)));
            return true;
        }else{
        if (rgen <= addProbability) {//landing
            requestMutex.lock();
            int ID = index++;
            requestMutex.unlock();
            time_t arrival = std::time(NULL);
            addLandingPlane(new Plane(ID,"L", arrival, std::time(NULL)));
            return true;
        }
        else if(rgen > addProbability){//departing
            requestMutex.lock();
            int ID = index++;
            requestMutex.unlock();
            time_t arrival = std::time(NULL);
            addDepartingPlane(new Plane(ID,"D", arrival, std::time(NULL)));
            return true;
        }
        }
    }
    
    Plane* deleteDepartingPlane(){
        planeQueueMutex.lock();
        Plane* nextPlane = departingPlaneQueue.front();
        departingPlaneQueue.pop_front();
        planeQueueMutex.unlock();
        return nextPlane;
    }
    Plane* deleteLandingPlane(){
        planeQueueMutex.lock();
        Plane* nextPlane = landingPlaneQueue.front();
        landingPlaneQueue.pop_front();
        planeQueueMutex.unlock();
        return nextPlane;
    }
    Plane* deleteEmergencyPlane(){
        planeQueueMutex.lock();
        Plane* nextPlane = emergencyPlaneQueue.front();
        emergencyPlaneQueue.pop_front();
        planeQueueMutex.unlock();
        return nextPlane;
    }
    void addDepartingPlane(Plane* plane) {
        departingPlaneQueueMutex.lock();
        departingPlaneQueue.push_back(plane);
        departingPlaneQueueMutex.unlock();
    }
    
    void addEmergencyPlane(Plane* plane) {
        emergencyPlaneQueueMutex.lock();
        emergencyPlaneQueue.push_back(plane);
        emergencyPlaneQueueMutex.unlock();
    }
    
    void addLandingPlane(Plane* plane) {
           landingPlaneQueueMutex.lock();
           landingPlaneQueue.push_back(plane);
           landingPlaneQueueMutex.unlock();
       }
    int getLandingPlaneCount() {
        return landingPlaneQueue.size();
    }
    
    int getDepartingPlaneCount() {
           return departingPlaneQueue.size();
       }
    
    int getEmergencyPlaneCount() {
              return emergencyPlaneQueue.size();
          }
};

pthread_t * runwayThread;
pthread_t * loggerThread;
pthread_t * towerThread;
Runway *runway = new Runway();

class Tower
{
public:

    void run()
    {
        while (true) {
            towerControl();
            pthread_sleep(2*t);
        }
    }
    
    Plane* plane;
    
    
    
    void towerControl() {
        
        if(runway->getEmergencyPlaneCount() == 1){
            plane = runway->deleteEmergencyPlane();
            printf("Emergency, ID:%d\n",plane->planeID);
            plane->turnAndGo();
        }
        else if(runway->getLandingPlaneCount() > 0){
            struct tm aaaaa=*localtime(&runway->landingPlaneQueue.front()->requestTime);
            int cccc = aaaaa.tm_sec + aaaaa.tm_min*60;
            time_t now = std::time(NULL);
            struct tm bbbb=*localtime(&now);
            int dddd = bbbb.tm_sec + bbbb.tm_min*60;
            
            if(runway->getDepartingPlaneCount() < counter){ // landing is prior to departing
                            plane = runway->deleteLandingPlane();
                            printf("Landing, ID:%d\n",plane->planeID);
                            plane->turnAndGo();
            } else {
                
                
            if((dddd - cccc) >= maxWaitTime){
            
            plane = runway->deleteLandingPlane();
            printf("Landing, ID:%d\n",plane->planeID);
            plane->turnAndGo();
            }else{
                plane = runway->deleteDepartingPlane();
                printf("Departing, ID:%d\n",plane->planeID);
                   plane->turnAndGo();
            }
            }
            printf("Waiting Time of the Landing Plane: %d \n",(dddd - cccc));
        }else{
            if(runway->getDepartingPlaneCount() > 0){
                plane = runway->deleteDepartingPlane();
             printf("Departing, ID:%d\n",plane->planeID);
                plane->turnAndGo();
            }
        }
        
    }
};

Tower *tower;

class Logger
{
public:
   
    std::ofstream planeLog;
    
    Logger() {
        planeLog.open(planeLogPath);
        planeLog << "PlaneID" << tab << "Status" << tab << tab << "Request Time" << tab << tab << "Runway Time" << tab << tab << "Turnaround Time\n";
    }

    void run() {
        while (!finishing) {
            groundOrAir();
            addLogs();
            pthread_sleep(t);
        }
        addLogs();
        planeLog.close();
    }
    
    void groundOrAir() {
        if (a > n){
            printf("At %d sec ground: ", a);
            for(Plane* plane : runway->departingPlaneQueue){
                printf("%d ",plane->planeID);
            }
            printf("\nAt %d sec air: ", a);
            for(Plane* plane : runway->landingPlaneQueue){
                printf("%d ",plane->planeID);
            }
            printf("\n");
        }
    }
    
    void addLogs() {
        planeDataMutex.lock();
        std::deque<std::string> writeDeque = *planeLogStrings;
        delete planeLogStrings;
        planeLogStrings = new std::deque<std::string>();
        planeDataMutex.unlock();

        std::stringstream str1;
        for (std::string str : writeDeque) {
            str1 << str << std::endl;
        }
        
        planeLog << str1.str();
    }

    void finish() {
        finishing = true;
    }
    
};

Logger *logger = new Logger();

void *createRunway(void* runway) {
    ((Runway*)runway)->run();
}
void *createTower(void* tower) {
    ((Tower*)tower)->run();
}
void *createLogger(void* logger) {
    ((Logger*)logger)->run();
}

int main(int argc, char** argv)
{
    s = 60;
    p = 0.5;
    t = 1;
    a = 0;
    n= 0;
    mt = new std::mt19937(120); // seed for debugging
    maxWaitTime = s/3;

    parseArgs(argc, argv);
    
    runwayThread = new pthread_t();
    loggerThread = new pthread_t();
    towerThread = new pthread_t();

    tower = new Tower();
    
    pthread_create(towerThread, NULL, createTower, tower);
    pthread_create(runwayThread, NULL, createRunway, runway);
    pthread_create(loggerThread, NULL, createLogger, logger);

    while(s--> 0){
        pthread_sleep(t);
    }
    
    logger->finish();
    pthread_join(*loggerThread, NULL);
    
    return 0;
}
