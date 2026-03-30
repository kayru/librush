file(GLOB _metallibs "${SRC_DIR}/*.metallib")
if(_metallibs)
	file(COPY ${_metallibs} DESTINATION "${DST_DIR}")
endif()
