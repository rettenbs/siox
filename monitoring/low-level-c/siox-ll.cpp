//////////////////////////////////////////////////////////
/// @file siox-ll.cpp
//////////////////////////////////////////////////////////
/// Implementation of the SIOX Low-Level API,
/// realised as a light-weight wrapper of C++ classes,
//////////////////////////////////////////////////////////

#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <sstream>
#include <fstream>
#include <pwd.h>
#include <typeinfo>

#include <core/module/ModuleLoader.hpp>
#include <core/datatypes/VariableDatatype.hpp>

#include "siox-ll-internal.hpp"

extern "C" {
#define SIOX_INTERNAL_USAGE
#include <C/siox.h>
}

using namespace std;
using namespace core;
using namespace monitoring;

// Are we currently working in the siox namespace?
// Needed for the instrumentation with DL_SYM to avoid circular dependencies.
extern "C" {
	__thread int siox_namespace = 0;
}

#define FUNCTION_BEGIN siox_namespace++;
#define FUNCTION_END siox_namespace--;

#define U32_TO_P(i) ((void*)((size_t)(i)))
#define P_TO_U32(p) ((uint32_t)((size_t)(p)))


//############################################################################
///////////////// GLOBAL VARIABLES ///////////////////////
//############################################################################

/// Flag indiciating whether SIOX is finalized and needing initalization.
bool finalized = true;

/// Struct to hold references to global objects needed.
struct process_info process_data;



//////////////////////////////////////////////////////////

// Functions used for testing the interfaces.
/*ProcessID build_process_id(NodeID hw, uint32_t pid, uint32_t time){
    return {.hw = hw, .pid = pid, .time = time};
}
*/

//////////////////////////////////////////////////////////

NodeID lookup_node_id( const string & hostname )
{
	FUNCTION_BEGIN
	auto ret = process_data.system_information_manager->register_nodeID( hostname );
	FUNCTION_END
	return ret;
}


//////////////////////////////////////////////////////////////////////////////
/// Create a process id object .
//////////////////////////////////////////////////////////////////////////////
/// @param nid [in] The node id of the hardware node the process runs on
//////////////////////////////////////////////////////////////////////////////
/// @return A process id, which is system-wide unique
//////////////////////////////////////////////////////////////////////////////
// Each process can create a runtime ID locally KNOWING the NodeID from the daemon
ProcessID create_process_id( NodeID nid )
{
	pid_t pid = getpid();
	struct timespec tv;

#ifndef __APPLE__
	clock_gettime( CLOCK_MONOTONIC, & tv );
#endif

	// casting will strip off the most significant bits on X86 and hence preserve the current seconds and PID
	ProcessID result = ProcessID();
	result.nid = nid;
	result.pid = ( uint32_t ) pid;
	result.time = ( uint32_t ) tv.tv_sec;
	return result;
}

//////////////////////////////////////////////////////////////////////////////
/// Read a character from a file.
//////////////////////////////////////////////////////////////////////////////
/// @param filename [in]
//////////////////////////////////////////////////////////////////////////////
/// @todo Document this function!
//////////////////////////////////////////////////////////////////////////////
static string readfile( const string & filename )
{
	ifstream file( filename, ios_base::in | ios_base::ate );
	if( ! file.good() ) {
		// @todo add error value.
		return string( "(error in " ) + filename + ")";
	}

	//return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

	// Some files from /proc contain 0.
	stringstream s;

	while( ! file.eof() ) {
		char c = 0;
		file >> c;
		if( c == 0 ) {
			c = ' ';
		}
		s << c;
	}

	return s.str();
}

//////////////////////////////////////////////////////////////////////////////
/// Gather relevant process information and attach it as attributes to the
/// current process.
//////////////////////////////////////////////////////////////////////////////
static void add_program_information()
{
	uint64_t pid = getpid();
	uint64_t u64;
	string read;

	OntologyAttribute description;
	description = process_data.ontology->register_attribute( "program", "description/commandLine", VariableDatatype::Type::STRING );

	{
		stringstream s;
		s << "/proc/" << pid << "/cmdline";
		read = readfile( s.str() );
	}
	process_data.association_mapper->set_process_attribute( process_data.pid, description, read );


	description = process_data.ontology->register_attribute( "program", "description/user-id", VariableDatatype::Type::UINT64 );
	u64 = ( uint64_t ) getuid();
	process_data.association_mapper->set_process_attribute( process_data.pid, description, u64 );


	description = process_data.ontology->register_attribute( "program", "description/group-id", VariableDatatype::Type::UINT64 );
	u64 = ( uint64_t ) getgid();
	process_data.association_mapper->set_process_attribute( process_data.pid, description, u64 );

	struct passwd * pwd = getpwuid( getuid() );

	if( pwd != nullptr ) {
		description = process_data.ontology->register_attribute( "program", "description/user-name", VariableDatatype::Type::STRING );
		process_data.association_mapper->set_process_attribute( process_data.pid, description, pwd->pw_name );
	}


	description = process_data.ontology->register_attribute( "program", "description/environment", VariableDatatype::Type::STRING );
	{
		stringstream s;
		s << "/proc/" << pid << "/environ";
		read = readfile( s.str() );
	}
	process_data.association_mapper->set_process_attribute( process_data.pid, description, read );
}

static void add_program_statistics()
{
}

extern "C" {

// Constructor for the shared library
	__attribute__( ( constructor ) ) void siox_ll_ctor()
	{
		FUNCTION_BEGIN
		// Retrieve hostname; NodeID and PID will follow once process_data is set up
		// TODO: Adapt this to C++?
		char local_hostname[1024];
		local_hostname[1023] = '\0';
		gethostname( local_hostname, 1023 );
		string hostname( local_hostname );
		// If necessary, do actual initialisation
		if( finalized ) {
			try {
				printf( "SIOX INITIALIZE\n" );

				// Load required modules and pull the interfaces into global datastructures
				// Use an environment variable and/or configuration files in <DIR> or /etc/siox.conf
				process_data.registrar = new ComponentRegistrar();

				// Check relevant environment modules:
				{
					const char * provider = getenv( "SIOX_CONFIGURATION_PROVIDER_MODULE" );
					const char * path = getenv( "SIOX_CONFIGURATION_PROVIDER_PATH" );
					const char * configuration = getenv( "SIOX_CONFIGURATION_PROVIDER_ENTRY_POINT" );

					provider = ( provider != nullptr ) ? provider : "siox-core-autoconfigurator-FileConfigurationProvider" ;
					path = ( path != nullptr ) ? path : "";
					configuration = ( configuration != nullptr ) ? configuration :  "siox.conf:/etc/siox.conf:monitoring/low-level-c/test/siox.conf:monitoring/low-level-c/test/siox.conf" ;

					process_data.configurator = new AutoConfigurator( process_data.registrar, provider, path, configuration );

					const char * configurationMode = getenv( "SIOX_CONFIGURATION_PROVIDER_MODE" );
					const char * configurationOverride = getenv( "SIOX_CONFIGURATION_SECTION_TO_USE" );

					string configName;
					if( configurationOverride != nullptr ) {
						configName = configurationOverride;
					} else {
						// hostname configurationMode (is optional)

						{
							stringstream configName_s;
							configName_s << hostname;
							if( configurationMode != nullptr ) {
								configName_s << " " << configurationMode;
							}
							configName = configName_s.str();
						}
					}

					cout << provider << " path " << path << " " << configuration << " ConfigName: \"" << configName << "\"" << endl;

					vector<Component *> loadedComponents;
					try {
						loadedComponents = process_data.configurator->LoadConfiguration( "Process", configName );
					} catch( InvalidConfiguration & e ) {
						// fallback to global configuration
						loadedComponents = process_data.configurator->LoadConfiguration( "Process", "" );
					}


					// if(loadedComponents == nullptr){ // MZ: Error, "==" not defined
					if( loadedComponents.empty() ) {
						cerr << "FATAL Invalid configuration set: " << endl;
						cerr << "SIOX_CONFIGURATION_PROVIDER_MODULE=" << provider << endl;
						cerr << "SIOX_CONFIGURATION_PROVIDER_PATH=" << path << endl;
						cerr << "SIOX_CONFIGURATION_PROVIDER_ENTRY_POINT=" << configuration << endl;
						cerr << "SIOX_CONFIGURATION_PROVIDER_MODE=" << configurationMode << endl;
						// TODO use FATAL function somehow?
						exit( 1 );
					}

					// check loaded components and assign them to the right struct elements.
					process_data.ontology = process_data.configurator->searchFor<Ontology>( loadedComponents );
					process_data.system_information_manager = process_data.configurator->searchFor<SystemInformationGlobalIDManager>( loadedComponents );
					process_data.association_mapper =  process_data.configurator->searchFor<AssociationMapper>( loadedComponents );
					process_data.amux = process_data.configurator->searchFor<ActivityMultiplexer>( loadedComponents );

					assert( process_data.ontology );
					assert( process_data.system_information_manager );
					assert( process_data.association_mapper );
					assert( process_data.amux );
				}
				// Retrieve NodeID and PID now that we have a valid SystemInformationManager
				process_data.nid = lookup_node_id( hostname );
				process_data.pid = create_process_id( process_data.nid );
			} catch( exception & e ) {
				cerr << "Received exception of type " << typeid( e ).name() << " message: " << e.what() << endl;
				exit( 1 );
			}

			add_program_information();
		}

		finalized = false;
		printf( "SIOX INITIALIZE END\n" );
		FUNCTION_END
	}

// Destructor for the shared library
	__attribute__( ( destructor ) ) void siox_ll_dtor()
	{
		if( finalized ) {
			return;
		}
		// Never enter any siox function after it has been stopped.
		FUNCTION_BEGIN

		add_program_statistics();

		// cleanup datastructures by using the component registrar:
		process_data.registrar->shutdown();

		delete( process_data.registrar );
		delete( process_data.configurator );

		finalized = true;
	}

//############################################################################
///////////////////// Implementation of SIOX-LL /////////////
//############################################################################

/// Variant datatype to uniformly represent the multitude of different attibutes
	typedef VariableDatatype AttributeValue;


//////////////////////////////////////////////////////////////////////////////
/// Convert an attribute's value to the generic datatype used in the ontology.
//////////////////////////////////////////////////////////////////////////////
/// @param attribute [in]
/// @param value [in]
//////////////////////////////////////////////////////////////////////////////
/// @return
//////////////////////////////////////////////////////////////////////////////
	static VariableDatatype convert_attribute( siox_attribute * attribute, const void * value )
	{
		AttributeValue v;
		switch( attribute->storage_type ) {
			case( VariableDatatype::Type::UINT32 ):
				return *( ( uint32_t * ) value );
			case( VariableDatatype::Type::INT32 ): {
				return *( ( int32_t * ) value );
			}
			case( VariableDatatype::Type::UINT64 ):
				return *( ( uint64_t * ) value );
			case( VariableDatatype::Type::INT64 ): {
				return *( ( int64_t * ) value );
			}
			case( VariableDatatype::Type::FLOAT ): {
				return  *( ( float * ) value );
			}
			case( VariableDatatype::Type::DOUBLE ): {
				return  *( ( double * ) value );
			}
			case( VariableDatatype::Type::STRING ): {
				return ( char * ) value;
			}
			case( VariableDatatype::Type::INVALID ): {
				assert( 0 );
			}
		}
		return "";
	}


	void siox_process_set_attribute( siox_attribute * attribute, const void * value )
	{
		assert( attribute != nullptr );
		assert( value != nullptr );
		FUNCTION_BEGIN
		AttributeValue val = convert_attribute( attribute, value );
		process_data.association_mapper->set_process_attribute( process_data.pid, *attribute, val );
		FUNCTION_END
	}


	siox_associate * siox_associate_instance( const char * instance_information )
	{
		FUNCTION_BEGIN
		auto ret = U32_TO_P( process_data.association_mapper->create_instance_mapping( instance_information ) );
		FUNCTION_END
		return ret;
	}

//############################################################################
/////////////// MONITORING /////////////////////////////
//############################################################################

	siox_node * siox_lookup_node_id( const char * hostname )
	{
		FUNCTION_BEGIN
		if( hostname == NULL ) {
			// we take the localhost's ID.
			return U32_TO_P( process_data.nid );
		}

		auto ret = U32_TO_P( lookup_node_id( hostname ) );
		FUNCTION_END
		return ret;
	}


	siox_component * siox_component_register( siox_unique_interface * uiid, const char * instance_name )
	{
		assert( uiid != SIOX_INVALID_ID );
		assert( instance_name != nullptr );

		FUNCTION_BEGIN

		boost::upgrade_lock<boost::shared_mutex> lock( process_data.critical_mutex );

		UniqueInterfaceID uid = P_TO_U32( uiid );
		const string & interface_implementation = process_data.system_information_manager->lookup_interface_implementation( uid );
		const string & interface_name = process_data.system_information_manager->lookup_interface_name( uid );

		char configName[1001];
		if( interface_implementation != "" ) {
			snprintf( configName, 1000, "%s %s", interface_name.c_str(), interface_implementation.c_str() );
		} else {
			snprintf( configName, 1000, "%s", interface_name.c_str() );
		}

		cout << "siox_component_register: " << configName << endl;

		vector<Component *> loadedComponents;
		try {
			loadedComponents = process_data.configurator->LoadConfiguration( "Interface", configName );
			if( loadedComponents.empty() ) {
				cerr << "WARNING Invalid configuration set for component" << endl;
				/// @todo Use FATAL function somehow?
				//exit(1);
			}
		} catch( InvalidConfiguration & e ) {
			cerr << "WARNING Invalid configuration: \"" << configName << "\"" << endl;
			cerr << e.what() << endl;
			//return nullptr;
		}

		boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock( lock );

		// check loaded components and assign them to the right struct elements.
		siox_component * result = new siox_component();
		result->cid.id = ++process_data.last_componentID;
		result->cid.pid = process_data.pid;
		result->uid = uid;

		string instance_str( instance_name );

		if( instance_str.size() > 0 ) {
			result->instance_associate = process_data.association_mapper->create_instance_mapping( instance_str );
		} else {
			result->instance_associate = SIOX_INVALID_ID;
		}

		result->amux = process_data.configurator->searchFor<ActivityMultiplexer>( loadedComponents );
		if( result->amux == nullptr ) {
			// link the global AMUX:
			result->amux = process_data.amux;
			assert( result->amux != nullptr );
		}

		FUNCTION_END

		assert( result != nullptr );

		return result;
	}


	void siox_component_set_attribute( siox_component * component, siox_attribute * attribute, const void * value )
	{
		FUNCTION_BEGIN
		assert( attribute != nullptr );
		assert( value != nullptr );
		assert( component != nullptr );

		OntologyValue val = convert_attribute( attribute, value );
		process_data.association_mapper->set_component_attribute( ( component->cid ), *attribute, val );
		FUNCTION_END
	}


	siox_component_activity * siox_component_register_activity( siox_unique_interface * uiid, const char * activity_name )
	{
		assert( uiid != SIOX_INVALID_ID );
		assert( activity_name != nullptr );

		FUNCTION_BEGIN
		auto ret = U32_TO_P( process_data.system_information_manager->register_activityID( P_TO_U32( uiid ), activity_name ) );
		FUNCTION_END
		return ret;
	}


	void siox_component_unregister( siox_component * component )
	{
		FUNCTION_BEGIN
		assert( component != nullptr );
		// Simple implementation: rely on ComponentRegistrar for shutdown.
		delete( component );
		FUNCTION_END
	}


	void siox_report_node_statistics( siox_node * node, siox_attribute * statistic, siox_timestamp start_of_interval, siox_timestamp end_of_interval, const void * value )
	{
		FUNCTION_BEGIN

		// MZ: Das reicht eigentlich bloß an den SMux weiter, oder?
		// Vorher: Statistik zusammenbauen!
		// TODO
		FUNCTION_END
	}

//############################################################################
/////////////// ACTIVITIES /////////////////////////////
//############################################################################

// MZ: How do we get our finished activity object back?!
//     In ..._activity_stop()?
// HM: After calling siox_activity_end(), the Activity object (siox_activity * == Activity *) is complete and may be handed over elsewhere.


	siox_activity * siox_activity_start( siox_component * component, siox_component_activity * activity )
	{
		assert( component != nullptr );
		assert( activity != nullptr );

		FUNCTION_BEGIN
		Activity * a;
		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();
		//cout << "START: " << ab << endl;

		a = ab->startActivity( component->cid, P_TO_U32( activity ), nullptr );
		FUNCTION_END

		return new siox_activity( a, component );
	}


	void siox_activity_stop( siox_activity * activity )
	{
		assert( activity != nullptr );

		FUNCTION_BEGIN
		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();
		ab->stopActivity( activity->activity, nullptr );
		FUNCTION_END
	}


	void siox_activity_set_attribute( siox_activity * activity, siox_attribute * attribute, const void * value )
	{
		assert( activity != nullptr );
		assert( attribute != nullptr );

		if( value == nullptr ) {
			return;
		}

		FUNCTION_BEGIN

		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();
		Attribute attr( attribute->aID, convert_attribute( attribute, value ) );

		ab->setActivityAttribute( activity->activity, attr );

		FUNCTION_END
	}


	void siox_activity_report_error( siox_activity * activity, siox_activity_error error )
	{
		assert( activity != nullptr );

		FUNCTION_BEGIN

		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();

		ab->reportActivityError( activity->activity, error );

		FUNCTION_END
	}


	void siox_activity_end( siox_activity * activity )
	{
		assert( activity != nullptr );
		assert( activity->activity != nullptr );

		FUNCTION_BEGIN
		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();

		//cout << "END: " << ab << endl;

		ab->endActivity( activity->activity );

		// Find component's amux
		ComponentID cid = activity->activity->aid().cid;
		siox_component * component = activity->component;
		assert( component != nullptr );
		// Send activity to it

		component-> amux->Log( activity->activity );

		delete( activity->activity );

		// prevent double free
		activity->activity = nullptr;
		delete( activity );

		FUNCTION_END
	}

	siox_activity_ID * siox_activity_get_ID( const siox_activity * activity )
	{
		assert( activity != nullptr );

		assert( sizeof( siox_activity_ID ) == sizeof( ActivityID ) );
		siox_activity_ID * id = ( siox_activity_ID * ) malloc( sizeof( ActivityID ) );
		ActivityID aid = activity->activity->aid();
		memcpy( id, & aid, sizeof( ActivityID ) );
		return id;
	}

	void siox_activity_link_to_parent( siox_activity * activity_child, siox_activity_ID * aid )
	{
		if( aid == nullptr )
			return;

		assert( activity_child != nullptr );

		FUNCTION_BEGIN
		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();

		ab->linkActivities( activity_child->activity, *( ( ActivityID * ) aid ) );

		FUNCTION_END
	}

//############################################################################
//////////////// REMOTE CALL ///////////////////////////////////
//############################################################################


	siox_remote_call * siox_remote_call_setup( siox_activity * activity, siox_node * target_node, siox_unique_interface * target_unique_interface, siox_associate * target_associate )
	{
		FUNCTION_BEGIN
		assert( activity != nullptr );

		siox_remote_call * rc;
		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();

		rc = ab->setupRemoteCall( activity->activity, P_TO_U32( target_node ), P_TO_U32( target_unique_interface ), P_TO_U32( target_associate ) );
		FUNCTION_END

		return rc;
	}


	void siox_remote_call_set_attribute( siox_remote_call * remote_call, siox_attribute * attribute, const void * value )
	{
		assert( remote_call != nullptr );
		assert( attribute != nullptr );
		assert( value != nullptr );
		FUNCTION_BEGIN
		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();
		Attribute attr( attribute->aID, convert_attribute( attribute, value ) );

		ab->setRemoteCallAttribute( remote_call, attr );

		FUNCTION_END
	}


	void siox_remote_call_start( siox_remote_call * remote_call )
	{
		assert( remote_call != nullptr );
		FUNCTION_BEGIN
		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();

		ab->startRemoteCall( remote_call, nullptr );
		FUNCTION_END
	}


	siox_activity * siox_activity_start_from_remote_call( siox_component * component, siox_component_activity * activity, siox_node * caller_node, siox_unique_interface * caller_unique_interface, siox_associate * caller_associate )
	{
		assert( component != nullptr );
		assert( activity != nullptr );

		FUNCTION_BEGIN
		Activity * a;
		ActivityBuilder * ab = ActivityBuilder::getThreadInstance();

		a = ab->startActivity( component->cid, P_TO_U32( activity ), P_TO_U32( caller_node ), P_TO_U32( caller_unique_interface ), P_TO_U32( caller_associate ), nullptr );

		FUNCTION_END
		return new siox_activity( a, component );
	}


//############################################################################
//////////////// ONTOLOGY  ///////////////////////////////////
//############################################################################

	siox_attribute * siox_ontology_register_attribute( const char * domain, const char * name, enum siox_ont_storage_type storage_type )
	{
		assert( domain != nullptr );
		assert( name != nullptr );

		try {
			FUNCTION_BEGIN
			auto ret = & process_data.ontology->register_attribute( domain, name, ( VariableDatatype::Type ) storage_type );
			FUNCTION_END
			return ret;
		} catch( IllegalStateError & e ) {
			FUNCTION_END
			return nullptr;
		}
	}


/// @todo change return value to bool, unless this proves C++-incompatible
	int siox_ontology_set_meta_attribute( siox_attribute * parent_attribute, siox_attribute * meta_attribute, const void * value )
	{
		assert( parent_attribute != nullptr );
		assert( meta_attribute != nullptr );
		assert( value != nullptr );

		AttributeValue val = convert_attribute( meta_attribute, value );
		try {
			FUNCTION_BEGIN
			process_data.ontology->attribute_set_meta_attribute( *parent_attribute, *meta_attribute, val );
			FUNCTION_END
		} catch( IllegalStateError & e ) {
			FUNCTION_END
			return 0;
		}
		return 1;
	}


	siox_attribute * siox_ontology_register_attribute_with_unit( const char * domain, const char * name, const char * unit, enum siox_ont_storage_type  storage_type )
	{
		assert( domain != nullptr );
		assert( name != nullptr );
		assert( unit != nullptr );

		// MZ: Hier besser statt einer eigenen Domain für Units d verwenden?!
		try {
			FUNCTION_BEGIN
			const OntologyAttribute & meta = process_data.ontology->register_attribute( "Meta", "Unit", VariableDatatype::Type::STRING );
			// OntologyAttribute * attribute = process_data.ontology->register_attribute(d, n, convert_attribute_type(storage_type));
			const OntologyAttribute & attribute = process_data.ontology->register_attribute( domain, name, ( VariableDatatype::Type ) storage_type );
			process_data.ontology->attribute_set_meta_attribute( attribute, meta, unit );
			FUNCTION_END
			return & attribute;
		} catch( IllegalStateError & e ) {
			FUNCTION_END
			return nullptr;
		}
	}


	siox_attribute * siox_ontology_lookup_attribute_by_name( const char * domain, const char * name )
	{
		assert( name != nullptr );
		try {
			FUNCTION_BEGIN
			auto ret = & process_data.ontology->lookup_attribute_by_name( domain, name );
			FUNCTION_END
			return ret;
		} catch( NotFoundError & e ) {
			FUNCTION_END
			return nullptr;
		}
	}


	siox_unique_interface * siox_system_information_lookup_interface_id( const char * interface_name, const char * implementation_identifier )
	{
		assert( interface_name != nullptr );
		assert( implementation_identifier != nullptr );

		try {
			FUNCTION_BEGIN
			auto ret = U32_TO_P( process_data.system_information_manager->register_interfaceID( interface_name, implementation_identifier ) );
			FUNCTION_END
			return ret;
		} catch( IllegalStateError & e ) {
			FUNCTION_END
			return 0;
		}
	}

} // extern "C"
