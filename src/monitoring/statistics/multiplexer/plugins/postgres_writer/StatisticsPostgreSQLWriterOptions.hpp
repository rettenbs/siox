#ifndef INCLUDE_MONITORING_STATISTICS_MULTIPLEXER_FILEWRITER_OPTIONS_H
#define INCLUDE_MONITORING_STATISTICS_MULTIPLEXER_FILEWRITER_OPTIONS_H

#include <string>

#include <monitoring/statistics/multiplexer/StatisticsMultiplexerPluginOptions.hpp>
#include <core/component/ComponentReference.hpp>

using namespace std;
using namespace monitoring;

//@serializable
class StatisticsPostgreSQLOptions : public monitoring::StatisticsMultiplexerPluginOptions {
public:
	string dbinfo;
};

#endif
