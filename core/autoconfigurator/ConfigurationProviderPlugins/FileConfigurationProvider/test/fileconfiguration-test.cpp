#include <assert.h>
#include <iostream>

#include <core/autoconfigurator/AutoConfigurator.hpp>

#include "test-serialize-modules.hpp"

using namespace core;

int main()
{
	ComponentRegistrar * registrar = new ComponentRegistrar();
	// make test runable using waf
	if( chdir( "../../../../../../monitoring" ) == 0 ) {
		chdir( "../" );
	}

	AutoConfigurator * a = new AutoConfigurator( registrar, "siox-core-autoconfigurator-FileConfigurationProvider", "", "core/autoconfigurator/ConfigurationProviderPlugins/FileConfigurationProvider/test/test.config" );

	MyChildModule * child = new MyChildModule();
	MyParentModule * parent = new MyParentModule();

	cout << "Parent Empty Configuration" << endl;
	cout << a->DumpConfiguration( & parent->getOptions() ) << endl;

	cout << "Child Empty Configuration" << endl;
	cout << a->DumpConfiguration( & child->getOptions() ) << endl;

	cout << "Loading configuration from file" << endl;
	vector<Component *> components = a->LoadConfiguration( "daemon", "hostname=\"node1\" mode=\"debug\"" );

	parent = dynamic_cast<MyParentModule *>( components[1] );
	assert( parent );
	child = dynamic_cast<MyChildModule *>( components[0] );
	assert( child );

	cout << parent->getOptions<MyParentModuleOptions>().pname << endl;
	cout << child->getOptions<MyChildModuleOptions>().name << endl;
	cout << parent->getOptions<MyParentModuleOptions>().childInterface.componentID << endl;
	assert( parent->getOptions<MyParentModuleOptions>().childInterface.componentID );

	ComponentReference chi = parent->getOptions<MyParentModuleOptions>().childInterface;
	void * instance = GET_INSTANCE( MyChildModule, chi );

	assert( instance == child );

	delete( registrar );

	return 0;
}
