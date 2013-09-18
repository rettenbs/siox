macro(SIOX_GENERATE_BUFFERS)
	PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${SIOX_INCLUDE}/core/comm/messages/siox.proto)
	add_library(core-comm-buffers ${PROTO_SRCS} ${PROTO_HDRS})
	target_link_libraries(core-comm-buffers ${PROTOBUF_LIBRARIES})
endmacro(SIOX_GENERATE_BUFFERS)

macro(SIOX_RUN_SERIALIZER IN OUT)
    add_custom_command(
	OUTPUT ${OUT}
	COMMAND ${CMAKE_SOURCE_DIR}/tools/serialization-code-generator.py
	ARGS -I ${CMAKE_SOURCE_DIR}/include -i ${CMAKE_CURRENT_SOURCE_DIR}/${IN} -o ${OUT} 
    )
endmacro(SIOX_RUN_SERIALIZER)
