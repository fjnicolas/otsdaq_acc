basic_plugin(FEACCInterface "interface"
    LIBRARIES REG 
    otsdaq_components::FEOtsUDPTemplateInterface
    ACC
    ${CETLIB}
    ${CETLIB_EXCEPT}
)
