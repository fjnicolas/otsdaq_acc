include(otsdaq::dataProcessor)

cet_build_plugin(ACCBurstDataSaverConsumer 
    otsdaq::dataProcessor
    ACC
)

install_headers()
install_source()
