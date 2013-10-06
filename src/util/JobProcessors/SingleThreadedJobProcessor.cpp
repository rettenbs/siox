#include "SingleThreadedJobProcessor.hpp"

using namespace std;

namespace util{

void SingleThreadedJobProcessor::startProcessing(){
	unique_lock<mutex> lk(m);
	enabledProcessing = true;
	cv.notify_one();
}

void SingleThreadedJobProcessor::iStartJob(void * job){
	unique_lock<mutex> lk(m);
	
	if( queue->mayEnqueue() ){
		queue->enqueueJob(job);
		// wakeup a pending threads
		if(enabledProcessing){
			cv.notify_one();
		}
	}else{
		jobAborted(job);
	}
}

void SingleThreadedJobProcessor::iCancelJob(void * job){
	unique_lock<mutex> lk(m);
	queue->removeJob(job);
}

void SingleThreadedJobProcessor::shutdown(){
	{
		unique_lock<mutex> lk(m);
		status = OperationalStatus::SHUTTING_DOWN;
		cv.notify_one();
	}
	myThread->join();
}

void SingleThreadedJobProcessor::terminate(){
	unique_lock<mutex> lk(m);
	status = OperationalStatus::SHUTTING_DOWN;
	abortPendingJobs();
	cv.notify_one();		
}

SingleThreadedJobProcessor::SingleThreadedJobProcessor(){
	myThread = new thread( & SingleThreadedJobProcessor::process, this );
}

void SingleThreadedJobProcessor::process(){
	while(true){
		void * job;
		{
			unique_lock<mutex> lk(m);
			while( queue->empty() || ! enabledProcessing ){
				if ( status == OperationalStatus::SHUTTING_DOWN ){
					return;
				}
				cv.wait(lk);
			}
			job = queue->pollNextJob();
		}
		processJob(job);
	}
}

}