/** @file ActivityMultiplexerAsync.cpp
 *
 * Implementation of the Activity Multiplexer Interface
 *
 *
 *
 *
 * Possible Improvements
 * TODO global flags to discard activities
 * TODO measure response time of plugins (e.g. how fast does l->Notify() return)
 * TODO a custom shared lock that suits the use case best
 *
 *
 */

#include <assert.h>

#include <list>
#include <deque>
#include <mutex>
#include <condition_variable>

#include <thread>

#include <boost/thread/shared_mutex.hpp>

#include <monitoring/datatypes/Activity.hpp>
#include <monitoring/activity_multiplexer/ActivityMultiplexerImplementation.hpp>
#include <monitoring/activity_multiplexer/ActivityMultiplexerListener.hpp>

#include "ActivityMultiplexerAsyncOptions.hpp"





using namespace core;
using namespace monitoring;

namespace monitoring {

	// forward declarations
	class ActivityMultiplexerNotifier;
	class ActivityMultiplexerAsync;

	class Dispatcher
	{
	public:
		virtual void Dispatch(int lost, void * work) {	 printf("Dispatch(Dummy): %p", work); }
	};


	/**
	 * A threadsafe queue implementation for the multiplexer to use
	 * The Queue is also responsible for counting discarded activities.
	 */
	class ActivityMultiplexerQueue
	{
		friend class ActivityMultiplexerNotifier;

		std::mutex lock;
		std::condition_variable not_full;
		std::condition_variable not_empty;

		std::deque<Activity *> queue;
		unsigned int capacity = 1000; // TODO specify by options

		bool overloaded = false;
		int lost = 1;

		// old mutex
		std::mutex mut;

		bool terminate = false;


	public:
			ActivityMultiplexerQueue () {};
			virtual ~ActivityMultiplexerQueue () {};

			/**
			 * Check whether or not the queue has still capacity
			 *
			 * @return  bool	true = queue is full, false = not full
			 */
			virtual bool Full() {
				bool result = ( queue.size() > capacity );
				return result;
			};



			/* Check whether of not the queue is empty
			 *
			 * @return bool     true = queue is empty, false = not empty
			 */
			virtual bool Empty() {
				bool result = ( queue.size() == 0 );
				return result;
			};


			/**
			 * Check if queue is in overload mode
			 *
			 */
			virtual bool Overloaded() {
				return overloaded;
			};

			/**
			 * Add an activity to the queue if there is capacity, set overload flag
			 * otherwhise.
			 *
			 * @param   activity     an activity that need to be dispatched in the future
			 */
			virtual void Push(Activity * activity) {
				//std::lock_guard<std::mutex> lock( mut );

				std::unique_lock<std::mutex> l(lock);
				not_full.wait(l, [=](){ return this->Full() == 0; });

				printf("push %p\n", activity);

				if (Overloaded() && Empty()) {
				    // TODO notifier.Reset(lost);
				    lost = 0;
				    overloaded = false;
				}


				if( Overloaded() ) {
					lost++;
				} else {

					if( Full() ) {
						overloaded = true;
						lost = 1;
					} else {
						queue.push_back( activity );
					}
				}


				not_empty.notify_one();
			};


			/**
			 * Get an activity from queue, returned element is popped!
			 *
			 * @return	Activity	an activity that needs to be dispatched to async listeners
			 */
			virtual Activity * Pop() {

				//std::lock_guard<std::mutex> lock( mut );

				std::unique_lock<std::mutex> l(lock);

				not_empty.wait(l, [=](){ return (this->Empty() == 0 || terminate); });

				Activity * activity = nullptr;
				if (queue.empty() || terminate) {
						return nullptr;
				}

				auto itr = queue.begin();
				queue.erase(itr);
				printf("pop %p\n", *itr);


				not_full.notify_one();

				return *itr;
			};

			virtual void finalize() {
				printf("Queue:  Finalizing\n");
				terminate = true;

				// this is important to wake up waiting pop/notifier
				not_empty.notify_one();
			}
	};



	class ActivityMultiplexerNotifier
	{
		friend class ActivityMultiplexerQueue;

	public:
			ActivityMultiplexerNotifier (Dispatcher * dispatcher, ActivityMultiplexerQueue * queue) {
				this->dispatcher = dispatcher;
				this->queue = queue;

				for( int i = 0; i < num_dispatcher; ++i ) {
					t.push_back( std::thread( &ActivityMultiplexerNotifier::Run, this ) );
				}
			};

			virtual ~ActivityMultiplexerNotifier () {
				for( auto it = t.begin(); it != t.end(); ++it ) {
					( *it ).join();
				}
			};

			virtual void Run() {
				assert(queue);
				// call dispatch of Dispatcher


				while( !terminate ) {

					Activity * activity = queue->Pop();

					if ( activity )
					{
						//TODO get overloaded with pop
						dispatcher->Dispatch(-1, (void *)activity);
					}

				}
			}

			virtual void finalize() {
				printf("Notifier:  Finalizing\n");
				terminate = true;
				queue->finalize();
			}

	private:
			Dispatcher * dispatcher = nullptr;
			ActivityMultiplexerQueue * queue = nullptr;

			// notifier currently maintains own thread pool
			int num_dispatcher = 1;
			list<std::thread> t;

			bool terminate = false;
	};



	/**
	 * ActivityMultiplexer
	 * Forwards logged activities to registered listeners (e.g. Plugins) either
	 * in an syncronised or asyncronous manner.
	 */
	class ActivityMultiplexerAsync : public ActivityMultiplexer, public Dispatcher {

	private:
			list<ActivityMultiplexerListener *> 		listeners;
			list<ActivityMultiplexerListenerAsync *> 	listeners_async;

			ActivityMultiplexerQueue * queue = nullptr;
			ActivityMultiplexerNotifier * notifier = nullptr;

			boost::shared_mutex  listener_change_mutex;
			boost::shared_mutex  listener_change_mutex_async;
	public:

			~ActivityMultiplexerAsync() {
				if ( notifier ) {
					notifier->finalize();
					delete notifier;
				}

				if ( queue )
					delete queue;

			}

			/**
			 * hand over activity to registered listeners
			 *
			 * @param	activity	logged activity
			 */
			virtual void Log( Activity * activity ){
				assert( activity != nullptr );
				// quick sync dispatch
				{
					boost::shared_lock<boost::shared_mutex> lock( listener_change_mutex );
					for(auto l = listeners.begin(); l != listeners.end() ; l++){
						(*l)->Notify(activity);
					}
				}
				// add to async patch
				if ( queue ) {
					queue->Push(activity);
				}
			}

			/**
			 * Notify async listeners of activity
			 *
			 * @param	lost	lost activtiy count
			 * @param	work	activtiy as void pointer to support abstract notifier
			 */
			virtual void Dispatch(int lost, void * work) {
				printf("dispatch: %p\n", work);
				Activity * activity = (Activity *) work;
				assert( activity != nullptr );
				boost::shared_lock<boost::shared_mutex> lock( listener_change_mutex_async );
				for(auto l = listeners_async.begin(); l != listeners_async.end() ; l++){
					(*l)->NotifyAsync(lost, activity);
				}
			}

			/**
			 * Register Listener to sync path
			 *
			 * @param	listener	listener to be registered
			 */
			virtual void registerListener( ActivityMultiplexerListener * listener ){
				boost::unique_lock<boost::shared_mutex> lock( listener_change_mutex );
				listeners.push_back(listener);

				// snipped conserved for later use
				//boost::upgrade_lock<boost::shared_mutex> lock( listener_change_mutex );
				// if () {
				// 		boost::upgrade_to_unique_lock<boost::shared_mutex> lock( listener_change_mutex );
				// }
			}


			/**
			 * Unegister Listener from sync path
			 *
			 * @param	listener	listener to be unregistered
			 */
			virtual void unregisterListener( ActivityMultiplexerListener * listener ){
				boost::unique_lock<boost::shared_mutex> lock( listener_change_mutex );
				listeners.remove(listener);
			}

			/**
			 * Register Listener to async path
			 *
			 * @param	listener	listener to be registered
			 */
			virtual void registerAsyncListener( ActivityMultiplexerListenerAsync * listener ){
				boost::unique_lock<boost::shared_mutex> lock( listener_change_mutex_async );
				listeners_async.push_back(listener);
			}


			/**
			 * Unegister Listener from async path
			 *
			 * @param	listener	listener to be unregistered
			 */
			virtual void unregisterAsyncListener( ActivityMultiplexerListenerAsync * listener ){
				boost::unique_lock<boost::shared_mutex> lock( listener_change_mutex_async );
				listeners_async.remove(listener);
			}


			/* Satisfy Component Requirements
			 */
			ComponentOptions * AvailableOptions() {
				return new ActivityMultiplexerAsyncOptions();
			}

			void init() {
				ActivityMultiplexerAsyncOptions & options = getOptions<ActivityMultiplexerAsyncOptions>();

				queue = new ActivityMultiplexerQueue();
				notifier = new ActivityMultiplexerNotifier(dynamic_cast<Dispatcher *>(this), queue);
			}
	};

}



extern "C" {
	void * MONITORING_ACTIVITY_MULTIPLEXER_INSTANCIATOR_NAME()
	{
		return new ActivityMultiplexerAsync();
	}
}