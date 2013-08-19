#ifndef SIOX_MODULE_LOADER_H
#define SIOX_MODULE_LOADER_H

#include <exception>
#include <string>


using namespace std;


namespace core{

/** 
* The module loader interface provides a simple abstraction to "plugin"  component modules at runtime.
* Load a single module and extracts any numbers of components with different interfaces from it.
* 

Definitions:
A COMPONENT is a module which usually is loaded just once per SIOX process.
A PLUGIN is a module of which multiple instances of different plugins are loaded, and controlled by another COMPONENT.

To write your own module, the following steps are needed.
A) If you create a new public interface (in the example the interface will be called [INTERFACE]) which is implementable by modules you have to do:

0) Define the appropriate namespace of your component (core, monitoring, knowledge).
1) Create [INTERFACE].hpp in include
	Create an abstract class which is derived from Component (core/component/Component.hpp)

	Also define the name of the interface:
	@code
	#define [INTERFACE][_PLUGIN]_INTERFACE "[INTERFACE][_plugin]"
	@endcode
	_plugin is used to suffix Plugin interfaces.

2) Create [INTERFACE]Implementation.hpp in include, this file is intented for developers implementing your interface.
	@code
	#include <[INTERFACE].hpp>
	@endcode
	This file should contain a macro called COMPONENT (or PLUGIN) which creates the interface, e.g.
	@code
	#define COMPONENT(x) \
	extern "C"{\
	void * get_instance_[INTERFACE][_plugin]() { return new x(); }\
	}
	@endcode


B) To implement a module (do not put any implementation file under include):
1) Create valid options for your module, for example, a filename your module works on.
	@code
	#include <core/component/component-macros.hpp>

	using namespace std;
	using namespace monitoring;

	class FileOntologyOptions: public core::ComponentOptions{
	public:
		string filename; // You can add any options, containers etc. here.

		SERIALIZE_CONTAINER(MEMBER(filename)) // add the name of the variable here as well.
	};
	@endcode

12 Create some CPP files, in one add "#include [INTERFACE]Implementation.hpp"
	and implement your interface according to the specification by deriving from your abstract class.
3) Add CREATE_SERIALIZEABLE_CLS(FileOntologyOptions) in one of your implementation CPP files to make sure the serialization code for your options is created.
4) Use the macro from A.2) to create the module interface in one of your implementation CPP files, e.g.:
	@code
	COMPONENT(FileOntology)
	@endcode
5) To create a valid module you have to implement the interfaces from a Component:
	@code
	protected:
		// This function returns the available options of this component.
		virtual ComponentOptions * AvailableOptions() = 0;
	public:
		// This function is invoked after the module has been loaded.
		// Options are then available using getOptions<[OPTIONSNAME]>()
		virtual void init() = 0;
	@endcode
For example, one could implement it using:
	@code
	ComponentOptions * AvailableOptions(){
		return new FileOntologyOptions();
	}

	void init(){
		FileOntologyOptions & o = getOptions<FileOntologyOptions>();
		cout << o.filename << endl;
	}
	@endcode
You may also use a destructor to cleanup your module.

*/ 
 
/**
 * Exception thrown upon an error of the module instantiation.
 */
class ModuleError: public std::exception{
	private:
		string interface;
		string module;
		string msg;
	public:
		ModuleError(string module_path, string module_name, string interface, string msg);
		const char* what() const throw();
};

namespace module_internal{
	// this definition reduces the definition required in the header file significantly.
	void * module_create_instance(string module_path, string module_name, string interface_name) throw (ModuleError);
}

/** 
 * This function loads the given module, localizes the interface as specified in the name.
 */
template <class COMPONENTTYPE> 
COMPONENTTYPE * module_create_instance(string module_path, string module_name, string interface_name) throw (ModuleError){
	void * instance =  module_internal::module_create_instance(module_path, module_name, interface_name);
	COMPONENTTYPE * instance_casted = static_cast<COMPONENTTYPE*>(instance);
	if (instance_casted == nullptr){
		throw ModuleError(module_path, module_name, interface_name, "Instance has an invalid type. Fatal module error!");
		// WARNING module instance may now leak memory, but this is an implementation error of the module which must be fixed there.
	}
	return instance_casted;
}

// COMPONENTTYPE is not restricted to component but it could be
// #include <core/component/component.hpp>
//#include <boost/type_traits.hpp>
//#include <boost/static_assert.hpp>	
//BOOST_STATIC_ASSERT(boost::is_base_of<core::component, COMPONENTTYPE>::value); 

} // end namespace

#endif 
