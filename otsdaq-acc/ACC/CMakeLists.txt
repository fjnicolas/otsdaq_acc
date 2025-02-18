cet_make_library(LIBRARY_NAME ACC
SOURCE ACDC.cc Metadata.cc
    LIBRARIES
    PUBLIC
    otsdaq::MessageFacility
    otsdaq::ConfigurationInterface
)

#cet_make(LIBRARY_NAME ACC
#	LIBRARIES
#	ConfigurationInterface
#	${CETLIB}
#	${CETLIB_EXCEPT}   
#)

install_headers()
install_source()
