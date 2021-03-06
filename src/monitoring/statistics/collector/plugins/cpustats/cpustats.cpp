#include "../ProcSingleFilePlugin.hpp"

#include <map>
#include <stdint.h>
#include <unistd.h>

using namespace std;

/*
Reads from /proc/stats and parses the fields.

cpu  1001327 1535 211910 10043870 10999 10 11650 0 0 0
cpu0 367697 622 75475 2366969 3437 2 5201 0 0 0
cpu1 340882 532 77640 2383862 1543 6 6175 0 0 0
cpu2 142832 236 26710 2653559 4581 0 189 0 0 0
cpu3 149915 144 32083 2639479 1436 1 84 0 0 0
intr 23060279 43 1607 0 0 0 0 0 0 1 701 0 0 40894 0 0 0 436195 161 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 14198 4736161 340745 1180523 531 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
ctxt 124918753
btime 1374908428
processes 21508
procs_running 1
procs_blocked 0
softirq 10934790 0 3455549 2149 25126 326929 0 1222230 3122564 25616 2754627

See http://www.linuxhowtos.org/System/procstat.htm for a description.

*/

class CPUstats: public ProcSingleFilePlugin<12> {
	private:
		typedef struct Cpu {
			int sourceLine;	//the number of the line that contains the information for this cpu
			int id;	//the aggregated field has the id -1
			StatisticsValue user, nice, system, idle, iowait, irq, softirq, virtualSystems, virtualOs, consumed;
			Cpu( int sourceLine, int id ) : sourceLine(sourceLine), id(id), user( ( uint64_t )0 ), nice( ( uint64_t )0 ), system( ( uint64_t )0 ), idle( ( uint64_t )0 ), iowait( ( uint64_t )0 ), irq( ( uint64_t )0 ), softirq( ( uint64_t )0 ), virtualSystems( ( uint64_t )0 ), virtualOs( ( uint64_t )0 ), consumed((uint64_t)(0)) {}
		} Cpu;

		long tickLen;
	protected:
		vector<Cpu> cpuStatistics;
		StatisticsValue interrupts = ( uint64_t )0;
		StatisticsValue sw_interrupts = ( uint64_t )0;
		StatisticsValue contextSwitches = ( uint64_t )0;
		StatisticsValue threadsCreated = ( uint64_t )0;
		StatisticsValue curProcsRunning = ( uint64_t )0;
		StatisticsValue curProcsBlocked = ( uint64_t )0;

		void timestepLine( int lineNr, vector<string> & entries ) {
			string name = entries[0];

			if( name.compare( 0, 3, "cpu" ) == 0 ) {
				Cpu* statistics = 0;
				for( size_t i = cpuStatistics.size(); i--; ) {
					if( cpuStatistics[i].sourceLine == lineNr ) {
						statistics = &cpuStatistics[i];
						break;
					}
				}
				assert( statistics );
				assert( entries.size() >= 10 );
				statistics->user = ( uint64_t )atoll( entries[1].c_str() ) * tickLen;
				statistics->nice = ( uint64_t )atoll( entries[2].c_str() ) * tickLen;
				statistics->system = ( uint64_t )atoll( entries[3].c_str() ) * tickLen;
				statistics->idle = ( uint64_t )atoll( entries[4].c_str() ) * tickLen;
				statistics->iowait = ( uint64_t )atoll( entries[5].c_str() ) * tickLen;
				statistics->irq = ( uint64_t )atoll( entries[6].c_str() ) * tickLen;
				statistics->softirq = ( uint64_t )atoll( entries[7].c_str() ) * tickLen;
				statistics->virtualSystems = ( uint64_t )atoll( entries[8].c_str() ) * tickLen;
				statistics->virtualOs = ( uint64_t )atoll( entries[9].c_str() ) * tickLen;

				statistics->consumed = statistics->user.uint64() + statistics->nice.uint64() + statistics->system.uint64() + statistics->irq.uint64() + statistics->softirq.uint64() + statistics->virtualSystems.uint64() + statistics->virtualOs.uint64();
			} else if( name == "intr" ) {
				interrupts = ( uint64_t ) atoll( entries[1].c_str() );
			} else if( name == "ctxt" ) {
				contextSwitches = ( uint64_t ) atoll( entries[1].c_str() );
			} else if( name == "processes" ) {
				threadsCreated = ( uint64_t ) atoll( entries[1].c_str() );
			} else if( name == "procs_running" ) {
				curProcsRunning = ( uint64_t ) atoll( entries[1].c_str() );
			} else if( name == "procs_blocked" ) {
				curProcsBlocked = ( uint64_t ) atoll( entries[1].c_str() );
			} else if( name == "softirq" ) {
				sw_interrupts = ( uint64_t ) atoll( entries[1].c_str() );
			}
		}

		void initLine( int lineNr, vector<string> & entries ) {
			string name = entries[0];
			if( name.compare( 0, 3, "cpu" ) == 0 ) {
				int cpuId = ( name.size() == 3 ) ? -1 : atoi( name.c_str() + 3 );
				cpuStatistics.push_back( Cpu( lineNr, cpuId ) );
			}
		}

		const string filename() {
			return "/proc/stat";
		}

		virtual vector<StatisticsProviderDatatypes> availableMetrics() throw() {
			vector<StatisticsProviderDatatypes> result;

			uint64_t overflow_value = ( uint64_t ) 1 << 63; //TODO CHECK ME, we expect 64 Bit...

			// compute the duration of a tick in ms.
			tickLen = 1000 / sysconf( _SC_CLK_TCK );

			for( unsigned i = 0; i < cpuStatistics.size(); i++ ) {
				Cpu& curCpu = cpuStatistics[i];
				ostringstream topologyPathStream;
				topologyPathStream << "@localhost";
				if( curCpu.id >= 0 ) {
					topologyPathStream << "/cpu:" << curCpu.id;
				}
				string topologyPathString = topologyPathStream.str();	//we need to keep the string around as long as we use the char*
				const char* topologyPath = topologyPathString.c_str();

				result.push_back( {"time/cpu", topologyPath, curCpu.consumed , INCREMENTAL, "ms", "Consumed CPU time", overflow_value, 0} );
				result.push_back( {"time/user", topologyPath, curCpu.user , INCREMENTAL, "ms", "Time spend in user mode", overflow_value, 0} );
				result.push_back( {"time/nice", topologyPath, curCpu.nice , INCREMENTAL, "ms", "Time spend for niced processes", overflow_value, 0} );
				result.push_back( {"time/system", topologyPath, curCpu.system , INCREMENTAL, "ms", "Time spend in system mode", overflow_value, 0} );
				result.push_back( {"time/idle", topologyPath, curCpu.idle , INCREMENTAL, "ms", "Idle time", overflow_value, 0} );
				result.push_back( {"time/iowait", topologyPath, curCpu.iowait , INCREMENTAL, "ms", "Time spend waiting for IO", overflow_value, 0} );
				result.push_back( {"time/interrupts", topologyPath, curCpu.irq , INCREMENTAL, "ms", "Time spend for IRQ", overflow_value, 0} );
				result.push_back( {"time/softirq", topologyPath, curCpu.softirq , INCREMENTAL, "ms", "Time spend for softIRQ ", overflow_value, 0} );
				result.push_back( {"time/vms", topologyPath, curCpu.virtualSystems , INCREMENTAL, "ms", "Time spend executing virtual hosts", overflow_value, 0} );
				result.push_back( {"time/vmsOS", topologyPath, curCpu.virtualOs , INCREMENTAL, "ms", "Time spend for guest operating systems", overflow_value, 0} );
			}

			result.push_back( {"quantity/hwInterrupts", "@localhost", interrupts, INCREMENTAL, "", "Serviced interrupts", overflow_value, 0} );
			result.push_back( {"quantity/swInterrupts", "@localhost", sw_interrupts , INCREMENTAL, "", "Serviced software interrupts", overflow_value, 0} );
			result.push_back( {"quantity/contextSwitches", "@localhost", contextSwitches , INCREMENTAL, "", "The total number of context switches across all CPUs.", overflow_value, 0} );
			result.push_back( {"quantity/threadsCreated", "@localhost", threadsCreated , INCREMENTAL, "", "The number of processes and threads created", overflow_value, 0} );
			result.push_back( {"quantity/procs/running", "@localhost", curProcsRunning , SAMPLED, "", "The number of processes currently running on all CPUs.", 0, 0} );
			result.push_back( {"quantity/procs/blocked", "@localhost", curProcsBlocked , SAMPLED, "", "The the number of processes currently blocked.", 0, 0} );

			return result;
		}
};

extern "C" {
	void * MONITORING_STATISTICS_PLUGIN_INSTANCIATOR_NAME()
	{
		return new CPUstats();
	}
}
