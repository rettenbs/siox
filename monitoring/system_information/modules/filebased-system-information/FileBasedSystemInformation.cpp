#include <string>
#include <map>
#include <vector>

#include <fstream>
#include <iostream>
#include <sstream>
#include <mutex>
#include <utility> // for pair.

#include <monitoring/system_information/SystemInformationGlobalIDManagerImplementation.hpp>

#include <boost/archive/xml_oarchive.hpp> 
#include <boost/archive/xml_iarchive.hpp> 
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>



#include "FileBasedSystemInformationOptions.hpp"


using namespace std;

using namespace monitoring;
using namespace core;
using namespace boost;


CREATE_SERIALIZEABLE_CLS(FileBasedSystemInformationOptions)


namespace monitoring{

class FileBasedSystemInformation: public SystemInformationGlobalIDManager{

	void load(string filename){
		//domain_name_map.clear();
		//attribute_map.clear();

		ifstream file(filename);
		if(! file.good())
			return;
		try{
			boost::archive::xml_iarchive archive(file, boost::archive::no_header | boost::archive::no_codecvt);
			//archive >> boost::serialization::make_nvp("MAX_VALUE", nextID);
			//archive >> boost::serialization::make_nvp("map", attribute_map);

			// recreate domain_name_map
			// for(auto itr = attribute_map.begin(); itr != attribute_map.end(); itr++){
			// 	auto pair = *itr;
			// 	AttributeWithValues * av = pair.second;

			// 	stringstream unique(av->attribute.domain);
			// 	unique << "|" << av->attribute.name;
			// 	string fqn(unique.str());

			// 	domain_name_map[fqn] = av; 
			// }
		}catch(boost::archive::archive_exception e){
			cerr << "Input file " << filename << " is damaged, recreating ontology!" << endl;
			//domain_name_map.clear();
			//attribute_map.clear();
			nextID = 1;
		}

		file.close();
	}

	void save(string filename){
		ofstream file(filename);
		boost::archive::xml_oarchive archive(file, boost::archive::no_header | boost::archive::no_codecvt);
		//archive << boost::serialization::make_nvp("MAX_VALUE", nextID);
		//archive << boost::serialization::make_nvp("map", attribute_map);

		file.close();
	}

	void init(){
		FileBasedSystemInformationOptions & o = getOptions<FileBasedSystemInformationOptions>();
		string filename = o.filename;
		if (filename.length() == 0){
			filename = "system_info.dat";
		}
		cout << "Initializing ID-mapper from file using " << filename << endl;

		load(filename);
	}

	ComponentOptions * AvailableOptions() {
		return new FileBasedSystemInformationOptions();
	}

	~FileBasedSystemInformation(){
		FileBasedSystemInformationOptions & o = getOptions<FileBasedSystemInformationOptions>();
		string filename = o.filename;

		save(filename);

		for(auto itr = valueStringMap.begin(); itr != valueStringMap.end(); itr++){
			auto pair = *itr;
			//AttributeWithValues * av = pair.second;
			//delete(av);
		}
	}

	///////////////////////////////////////////////////
#define CHECK(map, srch) \
		auto res = map.find(srch); \
		if (res != map.end()){ \
			return res->second;\
		}else{\
			throw NotFoundError();\
		}

	NodeID						register_nodeID(const string & hostname){
		NodeID id  = nodeMap[hostname];
		if(id != 0){
			return id;
		}

		m.lock();
		id = nextID++;
		valueStringMap[id] = hostname;
		nodeMap[hostname] = id;
		m.unlock();

		return id;
	}

	NodeID						lookup_nodeID(const string & hostname) const throw(NotFoundError) {
		CHECK(nodeMap, hostname)
	}

    const string &              lookup_node_hostname(NodeID id) const throw(NotFoundError) {
  		CHECK(valueStringMap, id)
    }


	DeviceID					register_deviceID(NodeID id, const  string & local_unique_identifier){
		// check if device exists already.		
		auto pair_str = pair<uint32_t,string>(id, local_unique_identifier);
		uint32_t device_id = deviceMap[pair_str];

		if( device_id != 0){
			return device_id;
		}

		m.lock();
		device_id = nextID++;
		deviceMap[pair_str] = device_id;
		valueStringMap[device_id] = local_unique_identifier;
		deviceNodeMap[device_id] = id;
		m.unlock();

		return device_id;
	}

	DeviceID					lookup_deviceID(NodeID id, const  string & local_unique_identifier) const throw(NotFoundError) {
		// check if device exists already.		
		auto pair_str = pair<uint32_t,string>(id, local_unique_identifier);
		CHECK(deviceMap, pair_str)
	}


	FilesystemID				register_filesystemID(const string & global_unique_identifier) {
		uint32_t fs_id = filesystemMap[global_unique_identifier];
		if (fs_id != 0){
			return fs_id;
		}

		m.lock();
		fs_id = nextID++;
		filesystemMap[global_unique_identifier] = fs_id;
		valueStringMap[fs_id] = global_unique_identifier;
		m.unlock();

		return fs_id;
	}

	FilesystemID				lookup_filesystemID(const string & global_unique_identifier) const throw(NotFoundError) {
		CHECK(filesystemMap, global_unique_identifier)
	}


    UniqueComponentActivityID   register_activityID(UniqueInterfaceID id, const string & name) {
    	int32_t idNum =  (((uint32_t) id.interface) << 16) + id.implementation;

    	auto pair_str = pair<uint32_t, string>(idNum, name);

    	UniqueComponentActivityID aid = activityMap[pair_str];

    	if(aid != 0){
    		return aid;
    	}

		m.lock();
		aid = nextID++;
		activityMap[pair_str] = aid;
		activityInterfaceIDMap[aid] = id;
		valueStringMap[aid] = name;
		m.unlock();
    	return aid;
    }

    UniqueComponentActivityID   lookup_activityID(UniqueInterfaceID id, const string & name) const throw(NotFoundError)  {
    	int32_t idNum =  (((uint32_t) id.interface) << 16) + id.implementation;

    	auto pair_str = pair<uint32_t, string>(idNum, name);

    	CHECK(activityMap, pair_str)
    }

    UniqueInterfaceID           lookup_interface_of_activity(UniqueComponentActivityID id) const throw(NotFoundError) {
    	CHECK(activityInterfaceIDMap, id)
    }


    NodeID                      lookup_node_of_device(DeviceID id) const throw(NotFoundError) {
    	CHECK(deviceNodeMap, id)
    }


    const string &              lookup_device_local_name(DeviceID id) const throw(NotFoundError) {
    	CHECK(valueStringMap, id)
    }

    const string &              lookup_filesystem_name(FilesystemID id) const throw(NotFoundError) {
    	CHECK(valueStringMap, id)
    }

    const string & 				lookup_activity_name(UniqueComponentActivityID id) const throw(NotFoundError) {
    	CHECK(valueStringMap, id)
    }


	UniqueInterfaceID           register_interfaceID(const string & interface, const string & implementation) {
		// check if the interface and implementation combination exists
		auto pair_str = pair<string,string>(interface, implementation);
		uint32_t cur = interfaceImplMap[pair_str];

		if( cur != 0){
			// interface and impl exists
			return {(uint16_t) (cur >> 16), (uint16_t) cur};
		}

		// check if interface exists
		m.lock();

		uint16_t i = interfaceMapStr[interface];
		if (i == 0){
			// create interface
			i = (uint16_t) nextID++;
			interfaceMap[i] = interface;
		}
		cur = nextID++;

		uint32_t final_int = ((((uint32_t) i)<<16) + cur);
		valueStringMap[final_int] = implementation;
		interfaceImplMap[pair_str] = final_int;

		m.unlock();
		return {i, (uint16_t)  cur};
	}


	UniqueInterfaceID           lookup_interfaceID(const string & interface, const string & implementation)  const throw(NotFoundError) {
		// check if the interface and implementation combination exists
		auto pair_str = pair<string,string>(interface, implementation);
		
		auto res = interfaceImplMap.find(pair_str); 
		if (res != interfaceImplMap.end()){ 
			uint32_t cur = res->second;
			return {(uint16_t) (cur >> 16), (uint16_t) cur};
		}else{
			throw NotFoundError();
		}
	}

    const string & 				lookup_interface_name(UniqueInterfaceID id) const throw(NotFoundError)  {
    	CHECK(interfaceMap, id.interface)
    }

    const string & 				lookup_interface_implementation(UniqueInterfaceID id) const throw(NotFoundError) {
    	uint32_t val = (((uint32_t) id.interface) << 16) + id.implementation;
    	CHECK(valueStringMap, val)
    }


private:
	mutex m;

	uint32_t nextID = 1;

	map<uint32_t, string> valueStringMap;

	map<DeviceID, NodeID> deviceNodeMap;

	map<uint16_t, string> interfaceMap;
	map<string, uint16_t> interfaceMapStr;
	map<pair<string,string>, uint32_t> interfaceImplMap;

	map<pair<NodeID,string>, uint32_t> deviceMap;

	map<string, FilesystemID> filesystemMap;

	map<string, NodeID> nodeMap;

	map<UniqueComponentActivityID, UniqueInterfaceID> activityInterfaceIDMap;

	map<pair<uint32_t, string>, uint32_t> activityMap;
};

}



COMPONENT(FileBasedSystemInformation)
