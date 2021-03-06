#ifndef SIOX_ONTOLOGY_DATATYPE_H
#define SIOX_ONTOLOGY_DATATYPE_H

#include <string>
#include <vector>
#include <string.h>
#include <assert.h>

#include <monitoring/datatypes/ids.hpp>
#include <core/datatypes/VariableDatatype.hpp>

using namespace std;

namespace monitoring {

	typedef VariableDatatype OntologyValue;

	struct OntologyAttribute {
			OntologyAttributeID aID;

			enum VariableDatatype::Type storage_type;
			//OntologyAttribute(OntologyAttributeID aid, string & name, enum siox_ont_storage_type storage_type) : aID(aid), name(name), storage_type(storage_type) {}

			inline bool operator==( OntologyAttribute const & a ) const {
				return ( a.aID == this->aID );
			}

			inline bool operator!=( OntologyAttribute const & a ) const {
				return !( a.aID == this->aID );
			}

			OntologyAttribute() : aID(0), storage_type(VariableDatatype::Type::INVALID) {}
			OntologyAttribute(OntologyAttributeID aID, enum VariableDatatype::Type storage_type) : aID(aID), storage_type(storage_type){
				
			}
	};


	struct OntologyAttributeFull : public OntologyAttribute {
			/**
			 * Reserved domains:
			 * - unit
			 *  for units of attribute values
			 *  (attach the unit as meta attribute of type SIOX_STORAGE_STRING to base attribute)
			 */
			string domain;
			string name;

			OntologyAttributeFull & operator=( OntologyAttributeFull const & a ) {
				this->aID = a.aID;
				this->domain = a.domain;
				this->name = a.name;
				this->storage_type = a.storage_type;
				return *this;
			}

			OntologyAttributeFull() : OntologyAttribute(), domain(), name() {}
			OntologyAttributeFull(OntologyAttributeID aID, const string & domain, const string & name, enum VariableDatatype::Type storage_type): OntologyAttribute(aID, storage_type), domain(domain), name(name){
				
			}
	};	
}

#endif
