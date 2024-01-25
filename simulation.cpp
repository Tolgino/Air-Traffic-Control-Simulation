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

//float frameLength=1.0f; //each second cars will appear, then police will run its routine
int s=15, k=121, t=0;
float p=0.5;
std::mutex officerNotifyMutex,threadEchoMutex,rngMutex;
std::condition_variable honkNoise;// threadEchoVar; //well air is shared, i might make a class for that lol
int maxCarAtALane=5;
int maxWaitBeforePriorityDuration = 20;
std::string policeLogPath= "police.log", carLogPath="car.log", tabChar="\t";
volatile int idIndex = 1;
std::mutex arrivalMutex, officerDataMutex, carDataMutex; //i thought of creating a new std::thread for each arrival to wait instead of cars, but like the officer one, operation is quite quick(just pop,push), once in a second and 4 atmost, 
volatile bool finalizing = false;
std::deque<std::string> *carLogStrings= new std::deque<std::string>(), *policeLogStrings= new std::deque<std::string>();
bool debugging = false; 
std::mt19937 *mt=NULL;
std::uniform_real_distribution<float> rngRange(0.0, 1.0);

void policeEventPush(std::string str) {
	officerDataMutex.lock();
	policeLogStrings->push_back(str);
	officerDataMutex.unlock();
}

std::string iStr(int i) {
	return std::to_string(i);
}
void policeEvent(std::string honkOrCellPhone) { //yeah can use enums here
	if (!finalizing) {
		std::stringstream ss;
		time_t now = std::time(NULL);
		struct tm timeinfo=*localtime(&now);
		ss << iStr(timeinfo.tm_hour) + ":" + iStr(timeinfo.tm_min) + ":" + iStr(timeinfo.tm_sec) << tabChar;
		ss << honkOrCellPhone;

		policeEventPush(ss.str());
	}
}

void carEventPush(std::string str) {
	carDataMutex.lock();
	carLogStrings->push_back(str);
	carDataMutex.unlock();
}

void carEvent(int carID, std::string direction, time_t arrival, time_t begone) {
	if (!finalizing) {
		std::stringstream ss;
		struct tm arrivalInfo=*localtime(&arrival), begoneInfo=*localtime(&begone);
		ss << carID << tabChar;
		ss << direction << tabChar;
		ss << iStr(arrivalInfo.tm_hour) +":"+ iStr(arrivalInfo.tm_min) +":"+ iStr(arrivalInfo.tm_sec) << tabChar;
		ss << iStr(begoneInfo.tm_hour) + ":" + iStr(begoneInfo.tm_min) + ":" + iStr(begoneInfo.tm_sec) << tabChar;
		ss << ((int)(begone - arrival));

		carEventPush(ss.str());
	}
}


int assignCarIndex() { //yeah, can use enums here to, but we will print it, so (shrug emoji)
	arrivalMutex.lock();
	int toReturn = idIndex++; //like java i++ first evaluates then increments right?
	arrivalMutex.unlock();
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

void theCodeThatWasProvidedOnBlackboardToMakeThreadsSleep(int sleepMilliSeconds) {
	std::this_thread::sleep_for(std::chrono::seconds(sleepMilliSeconds / 1000)); //improve to work with milliseconds
}

void jigglypuff(int sleepMilliSeconds) {
	theCodeThatWasProvidedOnBlackboardToMakeThreadsSleep(sleepMilliSeconds);
}

void parseArgs(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		std::string examined = std::string(argv[i]);
		std::string searched = examined.substr(0, 2);
		if (searched.compare("s=")==0) {
			s = std::stoi(examined.substr(2, examined.length()));
			dn("set s to "+iStr(s));
		}
		if (searched.compare("k=")==0) {
			k = std::stoi(examined.substr(2, examined.length()));
			dn("set k to "+iStr(k));
		}
		if (searched.compare("p=")==0) {
			p = std::stod(examined.substr(2, examined.length()));
			dn("set p to "+std::to_string(p));
		}
		if (searched.compare("t=")==0) {
			t = std::stoi(examined.substr(2, examined.length()));
			dn("set t to "+iStr(t));
		}
	}
	
}

class Car
{
	// Access specifier 
public:

	// Data Members 
	std::string geekname;
	int carID=0;
	std::string direction;
	time_t arrivalTime;
	//int crossTime;

	Car(int id, time_t arrival, std::string dir) {
		arrivalTime = arrival;
		carID = id;
		direction = dir;
		honkNoise.notify_one();
	}
	void turnAndBegone() {
		carEvent(carID, direction, arrivalTime, std::time(NULL));
		delete this; // delet dis
	}
};

class Lane
{
	// Access specifier 
public:

	Lane() {}

	// Data Members 
	std::string laneName;
	std::deque<Car*> carQueue;
	float spawnProbability = p;
	std::mutex carQueueMutex;

	// Member Functions()
	Lane(std::string str) {
		laneName = str;
	}

	void run() {
		while (true) {
			spawnCar();
			jigglypuff(1000);
		}
	}

	bool spawnCar(){
		float ft = safeRandom();
		if (ft <= p) {
			int id=assignCarIndex();
			time_t arrival = std::time(NULL);
			carPush(new Car(id, arrival, laneName));
			return true;
		}
		else {
			return false;
		}		
	}
	Car* carPop(){
		carQueueMutex.lock();
		Car* returnCar = carQueue.front();
		carQueue.pop_front();
		carQueueMutex.unlock();
		return returnCar;
	}
	void carPush(Car* car) {
		carQueueMutex.lock();
		carQueue.push_back(car);
		carQueueMutex.unlock();
	}
	int getCarCount() {
		return carQueue.size(); //this is pretty much atomic, dont alter the value, so no race, ok.
	}
	std::string getDirection() {
		return laneName;
	}
	int getMaxWaitTime() {
		int max = 0, inter = 0;
		carQueueMutex.lock();
		for (Car *car : carQueue) {
			time_t now = std::time(NULL);
			inter = now - car->arrivalTime; //maybe i should set a getter
			if (inter > max)
				max = inter;
		}
		carQueueMutex.unlock();
		return max;
	}
};

class NorthLane : public Lane
{
	// Access specifier 
public:

	// Data Members 

	// Member Functions() 
	NorthLane(){
		laneName = "N";
		spawnProbability = 1.0f - p;
	}

	//ne dion ak?: From North, once no car comes, there is a 20 second delay (sleep) before __any__ new car
		//will definitely arrive. Then cars will start arriving again with 1-p.
		//benim anladığım, demeye çalıştığı araç kalmadığı durumda 20 saniye sonraya kadar gelmezse 20 saniye sonra garanti gelsein, ama o zaman 1-p'ye dönme kısmı yalan.
		//ama dediği o saniye araç gelmezse 20 saniye gelmeyecek sonra bir araç kesin gelecek, geldikten sonra 1-p'den devam edecek. O ZAMAN ANY DEME A DE. yikes.
		//idk nasıl yorumlasam, bozuk para atayım

	void runNorth() {
		while (true) {
			spawnCarNorth();
			jigglypuff(1000);
		}		
	}
	bool spawnCarNorth() {
		if (safeRandom() <= p) {
			int id = assignCarIndex();
			time_t arrival = std::time(NULL);
			carPush(new Car(id, arrival,laneName));
			return true;
		}
		else {
			jigglypuff(20000);
			int id = assignCarIndex();
			time_t arrival = std::time(NULL);
			carPush(new Car(id, arrival,laneName));
			return true;
		}
	}
};

std::vector<Lane*> lanes;
pthread_t * nThread, * eThread, * sThread, * wThread, * oThread, * dgThread;
Lane *eastLane = new Lane("E"), *southLane = new Lane("S"), *westLane = new Lane("W");
NorthLane *northLane = new NorthLane();

class Officer
{
	// Access specifier 
public:

	// Data Members 
	bool hasHearingProblems = false;
	int numberOfEyes = 4;
	int officerHonkReactionTime = 3;
	std::string *lastLane= new std::string("");
	//int Wait-Time; created in the debug fun

	// Member Functions() 
	void run()
	{
		dn("Officer Billy, reporting for duty!\n");
		while (true) {
			switchLanes();
			jigglypuff(1000);
		}
	}

	void switchLanes() { //first to those waited the waitLimit, then to the one with most cars, if same then N > E > S > W priority
		std::unordered_map<std::string, int> vals;
		std::string priority[] = {"N","E","S","W"};

		//check wait times, use getMaxCarWaitTime, 0 zero if no car
		//collect in map<letter,waitTime> and store max
		//if max<20 contuniue, else iterate with elif nesw order go with the first lane stored==max, repeat
		int max = 0, inter = 0;
		for (Lane *lane : lanes) {
			inter = lane->getMaxWaitTime();
			if (inter > max)
				max = inter;
			vals[lane->getDirection()] = inter;
		}
		if (debugging)dn("max wait time"+ std::to_string(max));
		if (max >= maxWaitBeforePriorityDuration) {
			for (int i = 0; i < 4; i++) {
				if (vals[priority[i]] == max) {
					switchToThisLane(priority[i]);
					return;
				}
			}
		}

		//collect car count, store max , getCarCount
		//if max<5 continue, else iterate with elif news order, go with first stored==max ,if max==0, play games
		max = 0, inter = 0;
		for (Lane *lane : lanes) {
			inter = lane->getCarCount();
			if (inter > max)
				max = inter;
			vals[lane->getDirection()]=inter;
		}
		if (max >= maxCarAtALane) {
			for (int i = 0; i < 4; i++) {
				if (vals[priority[i]] == max) {
					switchToThisLane(priority[i]);
					return;
				}
			}
		}
				
		//if no cars, play games
		if (max == 0) {
			if (debugging)dn("started playing games");
			startPlayingCandyCrush();
			return;
		}

		//if lastLane letter exist and lane itself actually is not empty, then go with the last
		if ((!lastLane->empty()) && (vals[*lastLane] != 0) ) {
			if (debugging)dn("keep going the same lane");
			switchToThisLane(*lastLane);
			return;
		}else { //if lastLineLetter does not exist, or the lane is emptied, grab a new lane from <5 counts for all are so 
			for (int i = 0; i < 4; i++) {
				if (vals[priority[i]] == max) {
					if (debugging)dn("grabbed a new lane:"+priority[i]);
					switchToThisLane(priority[i]);
					return;
				}
			}
		}		
	}
	
	void switchToThisLane(std::string laneLetter) {
		//definitely would benefit from enum here with a switch
		//lets go with extra ram, a map
		//lol i couldnt even make a global access map, i dont know anything about pointers lol
		//this is not the day to lear cpp, and this is not the day lol
		//i learn by textbooks, i should read one about cpp sometime
		Car *car;
		if (laneLetter.compare("N")==0) {
			car = northLane->carPop();
			car->turnAndBegone();
		}else if (laneLetter.compare("E")==0) {
			car = eastLane->carPop();
			car->turnAndBegone();
		}else if (laneLetter.compare("S")==0) {
			car = southLane->carPop();
			car->turnAndBegone();
		}else if (laneLetter.compare("W")==0) {
			car = westLane->carPop();
			car->turnAndBegone();
		}
	}
	void startPlayingCandyCrush() {
		policeEvent("Cell Phone");
		std::unique_lock<std::mutex> lck(officerNotifyMutex);
		if (debugging)dn("started sleeping!zzzzz");
		honkNoise.wait(lck);
		if (debugging)dn("honked");
		honkedByCar();
	}
	void honkedByCar(){
		policeEvent("Honk");
		jigglypuff(3000);
		*lastLane = "";
		switchLanes();//TODO 
	}
};

Officer *officer;

class DebugLogger
{
	// Access specifier 
public:

	// Data Members 	
	std::ofstream policeLog, carLog;
	

	DebugLogger() {
		policeLog.open(policeLogPath);
		carLog.open(carLogPath);
		writePoliceLog("Time" + tabChar + "Event\n");
		writeCarLog("CarID" + tabChar + "Direction" + tabChar + "Arrival-Time" + tabChar + "Cross-Time" + tabChar + "WaitTime\n");
	}

	void run() {
		while (!finalizing) {
			showState();
			writeLogs();
			jigglypuff(1000);
		}
		writeLogs();
		closeFiles();
	}

	void writeLogs() {
		carDataMutex.lock();		
		std::deque<std::string> writeDeque = *carLogStrings;
		delete carLogStrings;
		carLogStrings = new std::deque<std::string>();
		carDataMutex.unlock();

		
		std::stringstream ss; //lets buffer it a bit
		for (std::string str : writeDeque) {
			ss << str << std::endl;
		}
		writeCarLog(ss.str());

		officerDataMutex.lock();
		writeDeque = *policeLogStrings;
		delete policeLogStrings;
		policeLogStrings = new std::deque<std::string>();
		officerDataMutex.unlock();
		std::stringstream ss2;
		for (std::string str : writeDeque) {
			ss2 << str << std::endl;
		}
		writePoliceLog(ss2.str());
	}

	void showState() {
		if(t--<=0){
		std::stringstream ss;
		time_t now = std::time(NULL);
		struct tm timeinfo=*localtime(&now);
		ss << "At:" << tabChar;
		ss << iStr(timeinfo.tm_hour) + ":" + iStr(timeinfo.tm_min) + ":" + iStr(timeinfo.tm_sec) << std::endl;
		ss << std::endl;
		ss << tabChar << northLane->getCarCount() << std::endl;
		ss << westLane->getCarCount() << tabChar + tabChar << eastLane->getCarCount() << std::endl;
		ss << tabChar << southLane->getCarCount() << std::endl;
		std::cout << ss.str();
		}
	}

	void writePoliceLog(std::string str) {
		policeLog << str;
	}

	void writeCarLog(std::string str) {
		carLog << str;
	}

	// Member Functions() 
	
	void finalize() {
		finalizing = true;
	}
	void closeFiles() {
		carLog.close();
		policeLog.close();
	}
	
};

DebugLogger *dg = new DebugLogger();

void *laneRunner(void* lane) {
	((Lane*)lane)->run();
}
void *northLaneRunner(void* lane) {
	((NorthLane*)lane)->runNorth();
}
void *officerRunner(void* officer) {
	((Officer*)officer)->run();
}
void *debugRunner(void* debugLogger) {
	((DebugLogger*)debugLogger)->run();
}

int main(int argc, char** argv) //expecting args in order, s=simulation time, p=probability of car arrival at any lane but north, north-> 1-p , k=seed
{
	s = 20;
	p = 0.4f;
	k = 123; //121 with no honk
	//parse these and overwrite
	parseArgs(argc, argv);

	mt = new std::mt19937(k);

	//@SupressWarnings
	//startTime = std::time(NULL);
	//int endTime = s;

	lanes.push_back(northLane);
	lanes.push_back(eastLane);
	lanes.push_back(southLane);
	lanes.push_back(westLane);	
	
	
	nThread = new pthread_t();
	eThread = new pthread_t();
	sThread = new pthread_t();
	wThread = new pthread_t();
	oThread = new pthread_t();
	dgThread = new pthread_t();

	pthread_create(nThread,NULL ,northLaneRunner, northLane);
	pthread_create(eThread,NULL ,laneRunner, eastLane);
	pthread_create(sThread,NULL ,laneRunner, southLane);
	pthread_create(wThread,NULL ,laneRunner, westLane);

	officer = new Officer();
	pthread_create(oThread,NULL ,officerRunner, officer);
	pthread_create(dgThread,NULL ,debugRunner, dg);


	while(s--> 0){ // while s goes to zero, ahhahahahaha
		//std::this_thread::sleep_for(std::chrono::seconds(1)); //improve to work with milliseconds
		jigglypuff(1000);
		if (debugging)dn("cooldown:" + iStr(s));
	}
	dg->finalize();
	pthread_join(*dgThread,NULL);
	return 0;
}

/*
class FrameHandler
{ // action order?-> spawn cars, make police do its frame routine
	// Access specifier
	public:

	// Data Members
	std::string geekname;

	// Member Functions()
	void printname()
	{
	   std::cout << "Geekname is: " << geekname;
	}
};
*/