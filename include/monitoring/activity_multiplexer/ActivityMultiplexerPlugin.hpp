#ifndef ACTIVITY_MULTIPLEXER_PLUGIN_H
#define ACTIVITY_MULTIPLEXER_PLUGIN_H

#include <assert.h>

#include <core/component/Component.hpp>
#include <monitoring/activity_multiplexer/ActivityMultiplexer.hpp>
#include <monitoring/activity_multiplexer/ActivityMultiplexerPluginOptions.hpp>
#include <knowledge/activity_plugin/ActivityPluginDereferencing.hpp>

using namespace core;

namespace monitoring {


	/**
	 *
	 */
	class ActivityMultiplexerPlugin: public Component {
		protected:
			ActivityMultiplexer * parent_multiplexer;

			// needed for some boost magic?
			ActivityPluginDereferencing * dereferenceFacade;

			virtual void init( ActivityMultiplexer & multiplexer ) = 0;

		public:
			void init( ActivityMultiplexer * activity_multiplexer, ActivityPluginDereferencing * dereferenceFacade ) {
				parent_multiplexer = activity_multiplexer;
				// may be 0.
				this->dereferenceFacade = dereferenceFacade;

				init( *parent_multiplexer );
			}

			void init() {
				ActivityMultiplexerPluginOptions & o = getOptions<ActivityMultiplexerPluginOptions>();
				assert( o.multiplexer.componentID != 0 );

				init( GET_INSTANCE(ActivityMultiplexer, o.multiplexer), GET_INSTANCE(ActivityPluginDereferencing, o.dereferenceFacade) );
			}
	};

}

#define ACTIVITY_MULTIPLEXER_PLUGIN_INTERFACE "monitoring_activitymultiplexer_plugin"

#endif

// BUILD_TEST_INTERFACE monitoring/activity_multiplexer/plugins/ActivityMultiplexerPlugin
