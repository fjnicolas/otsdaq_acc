cet_make_library(LIBRARY_NAME ACC
#  SOURCE DAQHardwareBase.cc OtsUDPFirmwareCore.cc OtsUDPFirmwareDataGen.cc OtsUDPHardware.cc
 ￼LIBRARIES
  PUBLIC
  otsdaq::MessageFacility
)

#cet_make(LIBRARY_NAME ACC
#	LIBRARIES
#	ConfigurationInterface
#	${CETLIB}
#	${CETLIB_EXCEPT}   
#)

install_headers()
install_source()
