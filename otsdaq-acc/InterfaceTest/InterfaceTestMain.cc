/*
#include "otsdaq-demo/FEInterfaces/FEWInterfacesManager.h"
#include "otsdaq-core/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq-core/ConfigurationDataFormats/ConfigurationKey.h"

#include "otsdaq-demo/FEInterfaces/FEWOtsGenericInterface.h"
#include "otsdaq-demo/FEInterfaces/FEWPurdueFSSRInterface.h"
#include "otsdaq-core/FECore/FEVInterface.h"

#include "otsdaq-core/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq-core/ConfigurationPluginDataFormats/Configurations.h"
//#include "otsdaq-core/ConfigurationDataFormats/FrontEndConfiguration.h"
#include "otsdaq-core/ConfigurationDataFormats/FEWOtsUDPHardwareConfiguration.h"

#include <iostream>

#include "../FEInterfaces/FEWOtsUDPFSSRInterface.h"
#include "../FEInterfaces/FEWOtsUDPHCALInterface.h"

using namespace ots;
*/
int main()
{
	return 1;
}
/*
	//Variables
	const int supervisorInstance_    = 1;
	const int configurationKeyValue_ = 0;
	const ConfigurationKey* theConfigurationKey_ = new ConfigurationKey(configurationKeyValue_);

	////////////////////////////////////////////////////////////////
	//INSERTED GLOBALLY IN THE CODE
//	ConfigurationManager*   theConfigurationManager_ = new ConfigurationManager;
//	FEWInterfacesManager    theFEWInterfacesManager_(theConfigurationManager_, supervisorInstance_);
//
//	theConfigurationManager_->setupFEWSupervisorConfiguration(theConfigurationKey_,supervisorInstance_);
//	theFEWInterfacesManager_.configure();
	////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	//Getting just the informations about the FEWInterface

//	const int fecInterfaceNumber_ = 0;
//	ConfigurationInterface* theInterface_;
//	theInterface_ = ConfigurationInterface::getInstance(true);//FIXME This will be variable because if false it takes it from the database
//	Configurations* configurations = 0;
//	theInterface_->get(configurations);
//	FEConfiguration* frontEndConfiguration = 0;
//	theInterface_->get(frontEndConfiguration, theConfigurationKey_, configurations);
//	FEWOtsUDPHardwareConfiguration* interfaceConfiguration_ = 0;
//
//	if(configurations->findKOC(theConfigurationKey_->key(),"FEWOtsUDPHardwareConfiguration"))
//		theInterface_->get(interfaceConfiguration_, theConfigurationKey_, configurations);
////	FEVInterface* theFEWInterface = new FEWOtsUDPFSSRInterface(fecInterfaceNumber_, "OTS_UDP", interfaceConfiguration_);
////	FEVInterface* theFEWInterface = new FEWZEDRyanInterface(fecInterfaceNumber_, "OTS_UDP", interfaceConfiguration_);
//	FEVInterface* theFEWInterface = new FEWOtsGenericInterface(fecInterfaceNumber_, "OTS_UDP", "FSSR", interfaceConfiguration_);
////	FEVInterface* theFEWInterface = new FEWZEDHCALInterface(fecInterfaceNumber_, interfaceConfiguration_);
//
//	theFEWInterface->configureFEW();
//	// theFEWInterface->start();
//	//theFEWInterface->configureDetector();
	////////////////////////////////////////////////////////////////

	return 0;
}
*/
