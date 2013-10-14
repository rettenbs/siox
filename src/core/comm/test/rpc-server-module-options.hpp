#include <core/component/component-options.hpp>
#include <core/component/ComponentReference.hpp>

using namespace std;
using namespace core;

//@serializable
class RPCServerModuleOptions: public core::ComponentOptions {
	public:
		// the communication module we want to use
		ComponentReference comm;
		string commAddress;
		ComponentReference rpcTarget;
};

