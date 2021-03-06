#ifndef SIOX_DATATYPE_HELPER_HDF5_
#define SIOX_DATATYPE_HELPER_HDF5_

// Generic flags for file open modes
enum HDF5FileFlags {
	SIOX_HDF5_ACCESS_RDWR = 1,
	SIOX_HDF5_ACCESS_RDONLY = 2,
	SIOX_HDF5_ACCESS_TRUNCATE = 4,
	SIOX_HDF5_ACCESS_EXCLUSIVE = 8,
	SIOX_HDF5_ACCESS_DEBUG = 16,
	SIOX_HDF5_ACCESS_CREATE = 32,
};

#endif
