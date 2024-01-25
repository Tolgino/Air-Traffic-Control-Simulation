//#include <chrono>
#include <string>
#include <iostream>           // std::std::cout
#include <thread>             // std::std::thread
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable
#include <ctime>
#include <deque>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <random>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// log kismini degistir


//float frameLength=1.0f; //each second cars will appear, then police will run its routine
int s;
int t;
float p;
int a;
int n;
std::mutex threadEchoMutex,rngMutex;
int maxPlanesAtALane=5;
int maxWaitBeforeEmergency = 40*t;
std::string planeLogPath="plane.log", tabChar="\t";
volatile int idIndex = 1;
std::mutex requestMutex, planeDataMutex; //i thought of creating a new std::thread for each arrival to wait instead of cars, but like the officer one, operation is quite quick(just pop,push), once in a second and 4 atmost,
volatile bool finalizing = false;
std::deque<std::string> *planeLogStrings= new std::deque<std::string>(), *policeLogStrings= new std::deque<std::string>();
bool debugging = false;
std::mt19937 *mt=NULL;
std::uniform_real_distribution<float> rngRange(0.0, 1.0);

std::string iStr(int i) {
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



void addPlaneEvent(std::string str) {
    planeDataMutex.lock();
    planeLogStrings->push_back(str);
    planeDataMutex.unlock();
}

void planeEvent(int planeID, std::string status, time_t request, time_t runway) {
    if (!finalizing) {
        std::stringstream ss;
        struct tm requestInfo=*localtime(&request), runwayInfo=*localtime(&runway);
        ss << tabChar << planeID << tabChar <<tabChar;
        ss << status << tabChar << tabChar;
        ss << iStr(requestInfo.tm_hour) +":"+ iStr(requestInfo.tm_min) +":"+ iStr(requestInfo.tm_sec) << tabChar <<tabChar<<tabChar;
        ss << iStr(runwayInfo.tm_hour) + ":" + iStr(runwayInfo.tm_min) + ":" + iStr(runwayInfo.tm_sec) << tabChar <<tabChar<<tabChar;
        ss << ((int)(runway - request));

        addPlaneEvent(ss.str());
    }
}


int assignPlaneIndex() { //yeah, can use enums here to, but we will print it, so (shrug emoji)
    requestMutex.lock();
    int toReturn = idIndex++; //like java i++ first evaluates then increments right?
    requestMutex.unlock();
    return toReturn;
}

float safeRandom() {
    
    rngMutex.lock();
    float rtnVal=rngRange(*mt);
    rngMutex.unlock();
    return rtnVal;
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
            dn("set s to "+iStr(s));
        }
        if (searched.compare("p=")==0) {
            p = std::stod(examined.substr(2, examined.length()));
            dn("set p to "+std::to_string(p));
        }
        if (searched.compare("t=")==0) {
            t = std::stoi(examined.substr(2, examined.length()));
            dn("set t to "+iStr(t));
        }
        if (searched.compare("n=")==0) {
            t = std::stoi(examined.substr(2, examined.length()));
            dn("set n to "+iStr(t));
        }
    }
    
}

class Plane
{
    // Access specifier
public:

    // Data Members
    std::string geekname;
    int planeID=0;
    std::string status;
    time_t requestTime;
    time_t runwayTime;
    time_t turnaroundTime;

    Plane(int ID, std::string stat, time_t request, time_t runway) {
        requestTime = request;
        runwayTime = runway;
        planeID = ID;
        status = stat;
        //honkNoise.notify_one();
    }
    void turnAndBegone() {
        planeEvent(planeID, status, requestTime, std::time(NULL));
        delete this; // delet dis
    }
    
    std::string getStatus() {
        return status;
    }
};

class Runway
{
    // Access specifier
public:

    // Data Members
    std::deque<Plane*> departingPlaneQueue;
    std::deque<Plane*> landingPlaneQueue;
    std::deque<Plane*> planeQueue;
    std::deque<Plane*> emergencyPlaneQueue;
    float spawnProbability = 0.5;
    std::mutex planeQueueMutex;
    std::mutex departingPlaneQueueMutex;
    std::mutex landingPlaneQueueMutex;
    std::mutex emergencyPlaneQueueMutex;

    // Member Functions()
    Runway() {

    }

    void run() {
        while (true) {
            spawnPlane();
            //std::this_thread::sleep_for(std::chrono::seconds(1000 / 1000));
            pthread_sleep(1);
            a = a + 1;
        }
    }

    bool spawnPlane(){
        float ft = safeRandom();
        if(a %40 == 0 && a > 0){
            int ID=assignPlaneIndex();
            time_t arrival = std::time(NULL);
            addPlanetoEmergency(new Plane(ID,"E", arrival, std::time(NULL)));
            return true;
        }else{
            
        if (ft <= spawnProbability) {//departing
            int ID=assignPlaneIndex();
            time_t arrival = std::time(NULL);
            addPlanetoDeparting(new Plane(ID,"D", arrival, std::time(NULL)));
            return true;
        }
        else if(ft > spawnProbability){// landing
            int ID=assignPlaneIndex();
            time_t arrival = std::time(NULL);
            addPlanetoLanding(new Plane(ID,"L", arrival, std::time(NULL)));
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
        Plane* nextPlane = landingPlaneQueue.front();
        emergencyPlaneQueue.pop_front();
        planeQueueMutex.unlock();
        return nextPlane;
    }
    void addPlanetoDeparting(Plane* plane) {
        departingPlaneQueueMutex.lock();
        departingPlaneQueue.push_back(plane);
        departingPlaneQueueMutex.unlock();
    }
    
    void addPlanetoEmergency(Plane* plane) {
        emergencyPlaneQueueMutex.lock();
        emergencyPlaneQueue.push_back(plane);
        emergencyPlaneQueueMutex.unlock();
    }
    
    void addPlanetoLanding(Plane* plane) {
           landingPlaneQueueMutex.lock();
           landingPlaneQueue.push_back(plane);
           landingPlaneQueueMutex.unlock();
       }
    int getLandingPlaneCount() {
        return landingPlaneQueue.size(); //this is pretty much atomic, dont alter the value, so no race, ok.
    }
    
    int getDepartingPlaneCount() {
           return departingPlaneQueue.size(); //this is pretty much atomic, dont alter the value, so no race, ok.
       }
    
    int getEmergencyPlaneCount() {
              return emergencyPlaneQueue.size(); //this is pretty much atomic, dont alter the value, so no race, ok.
          }
//    std::deque* getDepartingPlaneQueue(){
//        return departingPlaneQueue;
//    }
//    std::deque* getLandingPlaneQueue(){
//        return landingPlaneQueue;
//    }
//    std::deque* getEmergencyPlaneQueue(){
//        return emergencyPlaneQueue;
//    }
    
};

pthread_t * runwayThread, * dgThread, * tThread;
Runway *runway = new Runway();

class Tower
{
    // Access specifier
public:

    // Data Members
   
    
    // Member Functions()
    void run()
    {
        while (true) {
            switchLanes();
            //std::this_thread::sleep_for(std::chrono::seconds(2000 / 1000));
            pthread_sleep(2);
        }
    }
    Plane* plane;
    void switchLanes() { //first to those waited the waitLimit, then to the one with most cars, if same then N > E > S > W priority

        //check wait times, use getMaxCarWaitTime, 0 zero if no car
        //collect in map<letter,waitTime> and store max
        //if max<20 contuniue, else iterate with elif nesw order go with the first lane stored==max, repeat
        if(runway->getEmergencyPlaneCount() == 1){
            plane = runway->deleteEmergencyPlane();
            printf("Emergency, ID:%d\n",plane->planeID);
            plane->turnAndBegone();
        }
        else if(runway->getDepartingPlaneCount() >= 5 ){
            plane = runway->deleteDepartingPlane();
            printf("Departing, ID:%d\n",plane->planeID);
            plane->turnAndBegone();
        }
        else if(runway->getLandingPlaneCount() > 0){
            plane = runway->deleteLandingPlane();
            printf("Landing, ID:%d\n",plane->planeID);
            plane->turnAndBegone();
        }else{
            if(runway->getDepartingPlaneCount() > 0){
                plane = runway->deleteDepartingPlane();
             printf("Departing, ID:%d\n",plane->planeID);
                plane->turnAndBegone();
            }
        }
    }
};

Tower *tower;

class DebugLogger
{
    // Access specifier
public:

    // Data Members
    std::ofstream planeLog;
    

    DebugLogger() {
        planeLog.open(planeLogPath);
        writePlaneLog("PlaneID" + tabChar + "Status" + tabChar + tabChar + "Request Time" + tabChar + tabChar + "Runway Time" + tabChar + tabChar + "Turnaround Time\n");
    }

    void run() {
        while (!finalizing) {
            showState();
            writeLogs();// her seyi icinde topla
            //std::this_thread::sleep_for(std::chrono::seconds(1000 / 1000));
            pthread_sleep(1);
        }
        writeLogs();
        closeFiles();
    }

    void writeLogs() {
        planeDataMutex.lock();
        std::deque<std::string> writeDeque = *planeLogStrings;
        delete planeLogStrings;
        planeLogStrings = new std::deque<std::string>();
        planeDataMutex.unlock();

        
        std::stringstream ss; //lets buffer it a bit
        for (std::string str : writeDeque) {
            ss << str << std::endl;
        }
        writePlaneLog(ss.str());

    }
    
    void showState() {
//        printf("Departing planes: %d\n", runway->getDepartingPlaneCount());
//        printf("Landing planes: %d\n", runway->getLandingPlaneCount());
        
        if (a > n){
            printf("At %d sec ground: ", a);
            for(Plane* plane : runway->departingPlaneQueue){
                printf("%d ",plane->planeID);
            }
            printf("\nAt %d sec air:", a);
            for(Plane* plane : runway->landingPlaneQueue){
                printf("%d ",plane->planeID);
            }
            printf("\n");
        }
        
    }
    


    void writePlaneLog(std::string str) {
        planeLog << str;
    }

    // Member Functions()
    
    void finalize() {
        finalizing = true;
    }
    void closeFiles() {
        planeLog.close();
    }
    
};

DebugLogger *dg = new DebugLogger();

void *runwayRunner(void* runway) {// createRunway
    ((Runway*)runway)->run();
}
void *towerRunner(void* tower) {
    ((Tower*)tower)->run();
}
void *debugRunner(void* debugLogger) {
    ((DebugLogger*)debugLogger)->run();
}

int main(int argc, char** argv) //expecting args in order, s=simulation time, p=probability of car arrival at any lane but north, north-> 1-p , k=seed
{
    s = 20;
    p = 0.5;
    t = 1000;
    mt = new std::mt19937(120);
    a = 0;
    n= 10;
    
    //parse these and overwrite
    parseArgs(argc, argv);
    
    runwayThread = new pthread_t();
    dgThread = new pthread_t();
    tThread = new pthread_t();

    tower = new Tower();
    pthread_create(tThread,NULL ,towerRunner, tower);

    pthread_create(runwayThread,NULL ,runwayRunner, runway);

    pthread_create(dgThread,NULL ,debugRunner, dg);


    while(s--> 0){ // while s goes to zero, ahhahahahahahahahahhahahahahahahahhah
        //std::this_thread::sleep_for(std::chrono::seconds(1)); //improve to work with milliseconds
        //std::this_thread::sleep_for(std::chrono::seconds(1000 / 1000));
        pthread_sleep(1);
        if (debugging)dn("cooldown:" + iStr(s));
    }
    dg->finalize();
    pthread_join(*dgThread,NULL);
    return 0;
}
