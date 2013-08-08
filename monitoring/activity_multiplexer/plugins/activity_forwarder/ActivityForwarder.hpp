#ifndef ACTIVITYFORWARDER_H
#define ACTIVITYFORWARDER_H

#include <monitoring/datatypes/Activity.hpp>
#include <monitoring/activity_multiplexer/ActivityMultiplexer.hpp>
#include <monitoring/activity_multiplexer/ActivityMultiplexerListener.hpp>

class ActivityForwarder : ActivityMultiplexerListener
{
public:
	ActivityForwarder(const ActivityMultiplexer & in, const ActivityMultiplexer & out) =0;


	void Notify(Activity * element) =0;

private:
	/* data */
};

#endif /* ACTIVITYFORWARDER_H */
