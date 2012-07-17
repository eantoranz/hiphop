#--------------------------------------------------------
# Find postgresql
##########################################################################


#-------------- FIND POSTGRESQL_INCLUDE_DIR ------------------
FIND_PATH(POSTGRESQL_INCLUDE_DIR libpq-fe.h
		$ENV{POSTGRESQL_INCLUDE_DIR}
		$ENV{POSTGRESQL_DIR}/include
		/usr/include/postgresql
		/usr/local/include/postgresql
		/opt/postgresql/postgresql/include
		/opt/postgresql/postgresql/include/postgresql
		/opt/postgresql/include
		/usr/local/postgresql/include
		/usr/local/postgresql/include/postgresql
)

#----------------- FIND POSTGRESQL_LIB_DIR -------------------
FIND_LIBRARY(POSTGRESQL_LIB NAMES pq
				 PATHS
				 $ENV{POSTGRESQL_DIR}/.libs
				 $ENV{POSTGRESQL_DIR}/lib
				 /usr/lib
				 /usr/local/lib
				 /usr/local/postgresql/lib
)

IF(POSTGRESQL_LIB)
	GET_FILENAME_COMPONENT(POSTGRESQL_LIB_DIR ${POSTGRESQL_LIB} PATH)
ENDIF(POSTGRESQL_LIB)

IF (POSTGRESQL_INCLUDE_DIR AND POSTGRESQL_LIB_DIR)
	SET(POSTGRESQL_FOUND TRUE)

	SET(POSTGRESQL_CLIENT_LIBS pq)

	INCLUDE_DIRECTORIES(${POSTGRESQL_INCLUDE_DIR})
	LINK_DIRECTORIES(${POSTGRESQL_LIB_DIR})

	MESSAGE(STATUS "Postgresql Include dir: ${POSTGRESQL_INCLUDE_DIR}  library dir: ${POSTGRESQL_LIB_DIR}")
	MESSAGE(STATUS "Postgresql client libraries: ${POSTGRESQL_CLIENT_LIBS}")
ELSE (POSTGRESQL_INCLUDE_DIR AND POSTGRESQL_LIB_DIR)
	MESSAGE(FATAL_ERROR "Cannot find Postgresql. Include dir: ${POSTGRESQL_INCLUDE_DIR}  library dir: ${POSTGRESQL_LIB_DIR}")
ENDIF (POSTGRESQL_INCLUDE_DIR AND POSTGRESQL_LIB_DIR)

