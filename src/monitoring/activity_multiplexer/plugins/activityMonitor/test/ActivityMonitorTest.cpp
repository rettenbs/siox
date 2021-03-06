#include <unistd.h>

#include <core/module/ModuleLoader.hpp>

#include <monitoring/activity_multiplexer/ActivityMultiplexerPlugin.hpp>
#include <core/autoconfigurator/AutoConfigurator.hpp>
#include <monitoring/system_information/SystemInformationGlobalIDManager.hpp>
#include <monitoring/ontology/Ontology.hpp>

using namespace std;

using namespace core;
using namespace monitoring;
using namespace knowledge;

int main( int argc, char const * argv[] )
{
	ComponentRegistrar registrar = ComponentRegistrar();
	// make test runable using waf which is a different directory.
	if( chdir( "../../../../../../monitoring" ) == 0 ) {
		if( chdir( "../" ) ) assert(0 && "error while changing directory"), abort();
	}

	AutoConfigurator * a = new AutoConfigurator( & registrar, "siox-core-autoconfigurator-FileConfigurationProvider", "", "monitoring/activity_multiplexer/plugins/anomaly-skeleton/test/siox.conf:siox.conf" );

	vector<Component *> components = a->LoadConfiguration( "skeletonTest", "" );
	cout << "Loaded: " << components.size() << " Components/Plugins" << endl;


	registrar.start();

	// the activity multiplexer is expected to be first:
	ActivityMultiplexer * m1 = a->searchFor<ActivityMultiplexer>( components );
	assert( m1 != nullptr );

	Ontology * o = a->searchFor<Ontology>( components );
	assert( o != nullptr );

	SystemInformationGlobalIDManager * sys = a->searchFor<SystemInformationGlobalIDManager>( components );
	assert( sys != nullptr );

	UniqueInterfaceID uid = sys->register_interfaceID( "test", "impl1" );
	UniqueComponentActivityID aid = sys->register_activityID( uid, "open" );
	OntologyAttribute o1 = o->register_attribute( "test", "filesize", VariableDatatype::Type::UINT64 );
	OntologyAttribute unit = o->register_attribute( "Meta", "Unit", VariableDatatype::Type::STRING );
	o->attribute_set_meta_attribute( o1, unit, "Byte" );

	OntologyAttribute o2 = o->register_attribute( "test", "filename", VariableDatatype::Type::STRING );
	OntologyAttribute o3 = o->register_attribute( "test", "filesystem", VariableDatatype::Type::UINT32 );

	ActivityID aaid = {.cid = {.pid = {2, 3, 4}, .id = 1}, .id = 2};
	ActivityID parent =  {.cid = {.pid = {2, 3, 4}, .id = 1}, .id = 1};
	// Cast the real activity to the serializable object class wrapper
	shared_ptr<Activity> activity ( new Activity( aid, 1000, 2000, aaid, {parent}, { {.id = o1.aID, .value = ( uint64_t ) 4711}, 	{.id = o2.aID, .value = VariableDatatype( "julian/data.nc4" )},
	{.id = o3.aID, .value = ( uint32_t ) 1}}, {}, NULL, 0 ) );

	m1->Log( activity );

	sleep(1);

	registrar.stop();
	registrar.shutdown();

	cout << "OK" << endl;

	delete( a );

	return 0;
}


