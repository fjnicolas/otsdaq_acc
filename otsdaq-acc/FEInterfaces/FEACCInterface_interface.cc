#include <stdint.h>
#include <algorithm>
#include <iostream>  // std::cout, std::dec, std::hex, std::oct
#include <set>
#include "otsdaq-acc/FEInterfaces/FEACCInterface.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/InterfacePluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq-acc/ACC/ACDC.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FE-FEACCInterface"

FEACCInterface::FEACCInterface(const std::string&       interfaceUID,
                                       const ConfigurationTree& theXDAQContextConfigTree,
                                       const std::string& interfaceConfigurationPath)
    : Socket(theXDAQContextConfigTree.getNode(interfaceConfigurationPath)
                 .getNode("HostIPAddress")
                .getValue<std::string>(),
             theXDAQContextConfigTree.getNode(interfaceConfigurationPath)
                 .getNode("HostPort")
                 .getValue<unsigned int>())
    , FEOtsUDPTemplateInterface(
          interfaceUID, theXDAQContextConfigTree, interfaceConfigurationPath)
{
}


FEACCInterface::ConfigParams::ConfigParams() :
    rawMode(true),
    eventNumber(100),
    triggerMode(1),
    boardMask(0xff),
    label("testData"),
    reset(false),
    accTrigPolarity(0),
    validationStart(0),
    validationWindow(0),
    coincidentTrigMask(0x0f)
{
    for(int i = 0; i < 8; ++i)
    {
        coincidentTrigDelay[i] = 0;
        coincidentTrigStretch[i] = 5;
    }
}


//==============================================================================
FEACCInterface::~FEACCInterface(void) {}

//==============================================================================
void FEACCInterface::configure(void)
{
	__CFG_COUT__ << "configure" << std::endl;

	ConfigurationTree optionalLink =
	    theXDAQContextConfigTree_.getNode(theConfigurationPath_)
	        .getNode("LinkToOptionalParameters");
	bool usingOptionalParameters = !optionalLink.isDisconnected();

	std::string writeBuffer;
	uint64_t    readQuadWord;
	//Jin: we are loading the configuration params into local variables for quick reference.

	//params_.rawMode = !optionalLink.getNode("HumanReadableData").getValue<bool>();Jin: we are not using this property anymore
	params_.eventNumber = optionalLink.getNode("NumberofEvents").getValue<int>();
	params_.triggerMode = optionalLink.getNode("TriggerMode").getValue<int>();

	params_.boardMask = theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("ACDCMask").getValue<unsigned int>();
        
	params_.reset = optionalLink.getNode("ResetACCOnStart").getValue<bool>();

	params_.accTrigPolarity = optionalLink.getNode("ACCTrigPolarity").getValue<int>();

	params_.validationStart = optionalLink.getNode("ValidationStart").getValue<int>();
	params_.validationWindow = optionalLink.getNode("ValidationWindow").getValue<int>();

	params_.coincidentTrigMask = optionalLink.getNode("CoincidentTrigMask").getValue<int>();

	for(int i = 0; i < 8; ++i)
	{
	  try
	  {
	    params_.coincidentTrigDelay[i]   = optionalLink.getNode("LinkToACDC"+std::to_string(i)+"Parameters").getNode("CoincidentTrigDelay").getValue<int>();
	    params_.coincidentTrigStretch[i]   = optionalLink.getNode("LinkToACDC"+std::to_string(i)+"Parameters").getNode("CoincidentTrigStretch").getValue<int>();
	  }
	  catch(...)
	  {
	    //do nothing
	  }
	}

	////////////////////////////////////////////////////////////////////////////////
	// if clock reset is enabled reset clock
	// TODO?: MUST BE FIXED ADDING SOFT RESET. Fix config table as necessary.
	{
		try
		{
			if((usingOptionalParameters &&
			    optionalLink.getNode("EnableClockResetDuringConfigure")
			        .getValue<bool>() &&
			    optionalLink.getNode("PrimaryBoardConfig").getValue<bool>()))
			{
				__CFG_COUT__ << "\"Soft\" Resetting ACC Ethernet!" << std::endl;

				OtsUDPFirmwareCore::softEthernetReset(writeBuffer);
				OtsUDPHardware::write(writeBuffer);
				OtsUDPFirmwareCore::clearEthernetReset(writeBuffer);
				OtsUDPHardware::write(writeBuffer);
				// sleep(1); //seconds
			}
		}
		catch(...)
		{
			__CFG_COUT__ << "Could not find reset clock flag, so not resetting... "
			             << std::endl;
		}
	}
 
	FEOtsUDPTemplateInterface::configure();  // sets up destination IP/port
	//check ACDC PLL Settings
        // REVIEW ERRORS AND MESSAGES
		// Creates ACDCs for readout
	int retval = createAcdcs();
	if(retval==0)
	{
	  __CFG_COUT__ << "ACDCs could not be created." << std::endl;
	}

        //clear slow RX buffers just in case they have leftover data.
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x002, 0xff);
	OtsUDPHardware::write(writeBuffer);
        unsigned int boardsForRead = 0;
	__CFG_COUT__ << "Number of ACDCs connected: " << acdcs.size() << std::endl;
        for(ACDC& acdc : acdcs)
	{
            
	    acdc.parseConfig(theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("LinkToACDC"+std::to_string(acdc.getBoardIndex())+"Parameters"));

            //reset if requested
            if(acdc.params_.reset)
	    {
		resetACDC(1 << acdc.getBoardIndex());
		usleep(5000);
	    }

            // read ACDC info frame 
	    std::vector<uint64_t> acdcInfo = readSlowControl(acdc.getBoardIndex());            
            if((acdcInfo[0] & 0xffff) != 0x1234) //check header bytes
            {
                __CFG_COUT__ << "ACDC" << acdc.getBoardIndex() << " has invalid info frame" << std::endl;
            }

	    //Check PLL bits 
            if(!(acdcInfo[6] & 0x4)) __CFG_COUT__ << "ACDC" << acdc.getBoardIndex() << " has unlocked ACC pll" << std::endl;
            if(!(acdcInfo[6] & 0x2)) __CFG_COUT__ << "ACDC" << acdc.getBoardIndex() << " has unlocked serial pll" << std::endl;
            if(!(acdcInfo[6] & 0x1)) __CFG_COUT__ << "ACDC" << acdc.getBoardIndex() << " has unlocked white rabbit pll" << std::endl;

            if(!(acdcInfo[6] & 0x8))
            {
                // external PLL must be unconfigured, attempt to configure them 
                configJCPLL();

                // reset the ACDC after configuring JCPLL
                resetACDC();
                usleep(5000);

                // check PLL bit again
		acdcInfo = readSlowControl(acdc.getBoardIndex());
		if((acdcInfo[0] & 0xffff) != 0x1234)
                {
                    __CFG_COUT__ << "ACDC" << acdc.getBoardIndex() << " has invalid info frame" << std::endl;
                }
                
                if(!(acdcInfo[6] & 0x8))
		{
		    __SS__ << "ACDC" + std::to_string(acdc.getBoardIndex()) + " has unlocked sys pll." << std::endl;
		    __CFG_COUT_ERR__ << ss.str();
		}
            }

            //set pedestal settings
            if(acdc.params_.pedestals.size() == 5)
            {
                setPedestals(1 << acdc.getBoardIndex(), acdc.params_.pedestals);
            }

            //set dll_vdd
            for(int iPSEC = 0; iPSEC < 5; ++iPSEC)
            {
		OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x100, /*data*/ 0x00A00000 | (1 << (acdc.getBoardIndex() + 24))| (iPSEC << 12) | acdc.params_.dll_vdd);
		OtsUDPHardware::write(writeBuffer);
            }

	    //Set ACDC backpressure on
	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x100, /*data*/ 0x00B70000 | (1 << (acdc.getBoardIndex() + 24))| (acdc.params_.acc_backpressure?1:0));

	    OtsUDPHardware::write(writeBuffer);

	    __CFG_COUT__ << "Done configuring ACDC board " << acdc.getBoardIndex() << "." << std::endl;
        }

        //usleep(100000);

	//disable all triggers
	//ACC trigger
        for(unsigned int i = 0; i < 8; ++i)
	{
	  OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x0030+i, /*data*/0);
	  OtsUDPHardware::write(writeBuffer);
	}
	//ACDC trigger
	u_int64_t command = 0xffB00000;
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x100, /*data*/command);
	OtsUDPHardware::write(writeBuffer);
        //disable data transmission
        enableTransfer(0); 
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x0023, /*data*/0);
	OtsUDPHardware::write(writeBuffer);
	//flush data FIFOs
	dumpData(params_.boardMask);
	//train manchester links
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x0060, /*data*/0);
	OtsUDPHardware::write(writeBuffer);
	usleep(250);

        //scan hs link phases and pick optimal phase
        scanLinkPhase(params_.boardMask, true);

        // Toggles the calibration mode on if requested
	for(ACDC& acdc : acdcs) 
        {
            unsigned int acdcMask = 1 << acdc.getBoardIndex();
            if(acdcMask & params_.boardMask) toggleCal(acdc.params_.calibMode, 0x7FFF, acdcMask);
        }

	// Set trigger conditions
	switch(params_.triggerMode)
	{ 	
	case 0: //OFF
	    {
	        __SS__ << "Trigger source turned off." << std::endl;
	        __CFG_COUT_ERR__ << ss.str();
	    }
	    break;
	case 1: //Software trigger
            setHardwareTrigSrc(params_.triggerMode,params_.boardMask);
	    break;
	case 2: //Self trigger
	    //setHardwareTrigSrc(params_.triggerMode,params_.boardMask);
	    goto selfsetup;
	case 5: //Self trigger with SMA validation on ACC
	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x0038, /*data*/params_.accTrigPolarity);
	    OtsUDPHardware::write(writeBuffer);

	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x0039, /*data*/params_.validationStart);
	    OtsUDPHardware::write(writeBuffer);
			
	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x003a, /*data*/params_.validationWindow);
	    OtsUDPHardware::write(writeBuffer);
	    __attribute__ ((fallthrough));
	case 4: // ACC coincident TODO 
	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x003f, /*data*/params_.coincidentTrigMask);
	    OtsUDPHardware::write(writeBuffer);

	    for(int i = 0; i < 8; ++i)
	    {
		OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x0040+i, /*data*/params_.coincidentTrigDelay[i]);
		OtsUDPHardware::write(writeBuffer);

		OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x0048+i, /*data*/params_.coincidentTrigStretch[i]);
		OtsUDPHardware::write(writeBuffer);                            
	    }
	    __attribute__ ((fallthrough));
	case 3: //Self trigger with validation 
	    setHardwareTrigSrc(params_.triggerMode,params_.boardMask);
	    //timeout after 1 us 
	    command = 0x00B20000;
	    command = (command | (params_.boardMask << 24)) | 40;
	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x100, /*data*/command);
	    OtsUDPHardware::write(writeBuffer);			
	    goto selfsetup;
	default: // ERROR case
	{
	    __SS__ << "Error: Trigger mode unrecognized." << std::endl;
	    __CFG_COUT_ERR__ << ss.str();	
	}
	break;
	selfsetup:
	    command = 0x00B10000;
			
	    for(ACDC& acdc : acdcs)
	    {
	        //skip ACDC if it is not included in boardMask
	        unsigned int acdcMask = 1 << acdc.getBoardIndex();
	        if(!(params_.boardMask & acdcMask)) continue;
	    		
	        std::vector<unsigned int> CHIPMASK = {0x00000000,0x00001000,0x00002000,0x00003000,0x00004000};
	        for(int i=0; i<5; i++)
	        {		
		    command = 0x00B10000;
		    command = (command | (acdcMask << 24)) | CHIPMASK[i] | ((acdc.params_.selfTrigMask>>i*6) & 0x3f);
		    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x100, /*data*/command);
		    OtsUDPHardware::write(writeBuffer);
	        }
	    		
	        command = 0x00B16000;
	        command = (command | (acdcMask << 24)) | acdc.params_.selfTrigPolarity;
	        OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x100, /*data*/command);
	        OtsUDPHardware::write(writeBuffer);			
	    
	        if(acdc.params_.triggerThresholds.size() == 30)
	        {
		    for(int iChip = 0; iChip < 5; ++iChip)
		    {
			for(int iChan = 0; iChan < 6; ++iChan)
			{
			    command = 0x00A60000;
			    command = ((command + (iChan << 16)) | (acdcMask << 24)) | (iChip << 12) | acdc.params_.triggerThresholds[6*iChip + iChan];
			    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x100, /*data*/command);
			    OtsUDPHardware::write(writeBuffer);
			}
		    }
	        }
	        else
	        {
		    __SS__ << "Incorrect number of trigger thresholds: "<< std::to_string(acdc.params_.triggerThresholds.size()) << std::endl;
		    __CFG_COUT_ERR__ << ss.str();
	        }
	    }
	}

	//set fifo backpressure depth to maximum
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/ 0x0057, /*data*/0xe1);
	OtsUDPHardware::write(writeBuffer);		
	__CFG_COUT__ << "Done with configuring." << std::endl;
}  // end configure()

//==============================================================================
void FEACCInterface::halt(void)
{
	__CFG_COUT__ << "\tHalt" << std::endl;
	stop();
}

//==============================================================================
void FEACCInterface::pause(void)
{
	__CFG_COUT__ << "\tPause" << std::endl;
	stop();
}

//==============================================================================
void FEACCInterface::resume(void)
{
	__CFG_COUT__ << "\tResume" << std::endl;
	start(runNumber_);
}

//==============================================================================
void FEACCInterface::start(std::string runNumber)
{
        auto t0 = std::chrono::high_resolution_clock::now();//start measuring time.
	runNumber_ = runNumber;
	__CFG_COUT__ << "\tStart " << runNumber_ << std::endl;
	std::string writeBuffer;

        //flush data FIFOs
	dumpData(params_.boardMask);

	usleep(1000);

	__CFG_COUT__ << "Enabling burst mode!" << __E__;
	OtsUDPFirmwareCore::startBurst(writeBuffer);
	OtsUDPHardware::write(writeBuffer);
	//Enables the transfer of data from ACDC to ACC
	enableTransfer(3, params_.boardMask);
	
	
        //enable "auto-transmit" mode for ACC data readout
        OtsUDPFirmwareCore::writeAdvanced(writeBuffer,0x0023,1);
       	OtsUDPHardware::write(writeBuffer);

	//Enable triggers
	setHardwareTrigSrc(params_.triggerMode, params_.boardMask);
	__CFG_COUT__ << "\tStart Done" << std::endl;
}

//==============================================================================
void FEACCInterface::stop(void)
{
	std::string writeBuffer;
        
	OtsUDPHardware::write(writeBuffer);
	setHardwareTrigSrc(0, 0xff);

	__CFG_COUT__ << "\tStop" << std::endl;

	// attempt to stop burst always
	OtsUDPHardware::write(writeBuffer);
	enableTransfer(0);
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0023, /*data*/0);
        OtsUDPHardware::write(writeBuffer);
	usleep(100);//TODO: same as other sleeps; is this a good practice?
	OtsUDPFirmwareCore::stopBurst(writeBuffer);
	__CFG_COUT__ << "Done Stopping." << std::endl;
	
}

//==============================================================================
bool FEACCInterface::running(void)
{
    //__CFG_COUT__ << "Running" << "\n";
    listenForAcdcData();
    return true;
}  // end running()

////==============================================================================
/*ID:9 Create ACDC class instances for each connected ACDC board*/
int FEACCInterface::createAcdcs()
{
	//Check for connected ACDC boards
	std::vector<int> connectedBoards = whichAcdcsConnected(); 
	if(connectedBoards.size() == 0)
	{
		__CFG_COUT__ << "Trying to reset ACDC boards" << std::endl;
		std::string writeBuffer;
		OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, 0xFFFF0000);
		OtsUDPHardware::write(writeBuffer);
		usleep(10000);
		connectedBoards = whichAcdcsConnected();
		if(connectedBoards.size() == 0)
		{
			__CFG_COUT__ << "After ACDC reset no changes, still no boards found" << std::endl;
		}
	}

	//if there are no ACDCs, return 0
	if(connectedBoards.size() == 0)
	{
		__SS__ << "No aligned ACDC indices." << std::endl;
		__CFG_COUT_ERR__ << ss.str();
		return 0;
	}
	
	//Clear the ACDC class map if one exists
	acdcs.clear();

	//Create ACDC objects with their board numbers
	for(int bi: connectedBoards)
	{
		acdcs.emplace_back(bi);
	}

	if(acdcs.size()==0)
	{
	    //What possible scenerio could lead to this???
		__SS__ << "Error: No ACDCs created even though there were boards found." << std::endl;
		__CFG_COUT_ERR__ << ss.str();
		return 0;
	}

	return 1;
}

/*ID:11 Queries the ACC for information about connected ACDC boards*/
std::vector<int> FEACCInterface::whichAcdcsConnected()
{
	unsigned int command;
	vector<int> connectedBoards;

	//New sequence to ask the ACC to reply with the number of boards connected 
	//Disables the PSEC4 frame data transfer for this sequence. Has to be set to HIGH later again
	enableTransfer(0); 
	usleep(1000);

	std::string writeBuffer;
	u_int64_t readQuadWord;
	//Resets the RX buffer on all 8 ACDC boards
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0020, 0xFF);
	OtsUDPHardware::write(writeBuffer);

	//Request and read the ACC info buffer and pass it the the corresponding vector
	OtsUDPFirmwareCore::readAdvanced(writeBuffer, 0x1011);
	OtsUDPHardware::read(writeBuffer, readQuadWord);
	uint64_t accInfo = readQuadWord;

	unsigned short alignment_packet = ~((unsigned short)accInfo);
	for(int i = 0; i < MAX_NUM_BOARDS; i++)
	{	
		//both (1<<i) and (1<<i+8) should be true if aligned & synced respectively
		if((alignment_packet & (1 << i)))
		{
			//the i'th board is connected
			connectedBoards.push_back(i);
		}
	}

	cout << "Connected Boards: " << connectedBoards.size() << endl;
	return connectedBoards;
}


/*ID 21: Set up the hardware trigger*/
void FEACCInterface::setHardwareTrigSrc(int src, unsigned int boardMask)
{
	if(src > 9){
		string err_msg = "Source: ";
		err_msg += to_string(src);
		err_msg += " will cause an error for setting Hardware triggers. Source has to be <9";
		__SS__ << err_msg << std::endl;
		__CFG_COUT_ERR__ << ss.str();
	}

        int ACCtrigMode = 0;
        int ACDCtrigMode = 0;
        switch(src)
        {
        case 0:
            ACCtrigMode = 0;
            ACDCtrigMode = 0;
            break;
        case 1:
            ACCtrigMode = 1;
            ACDCtrigMode = 1;
            break;
        case 2:
            ACCtrigMode = 0;
            ACDCtrigMode = 2;
            break;
        case 3:
            ACCtrigMode = 2;
            ACDCtrigMode = 1;
            break;
        case 4:
            ACCtrigMode = 5;
            ACDCtrigMode = 3;
            break;
        default:
            ACCtrigMode = 0;
            ACDCtrigMode = 0;
            break;
        
        }

	std::string writeBuffer;
	//ACC hardware trigger
	for(unsigned int i = 0; i < 8; ++i)
    {
	  if((boardMask >> i) & 1)
	  {
	      OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0030+i, ACCtrigMode);
	      OtsUDPHardware::write(writeBuffer);
	  }
	  else                  
	  {
	      OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0030+i, 0);
	      OtsUDPHardware::write(writeBuffer);
	  } 
    }
	//ACDC hardware trigger
	unsigned int command = 0x00B00000;
	command = (command | (boardMask << 24)) | (unsigned short)ACDCtrigMode;
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, command);
	OtsUDPHardware::write(writeBuffer);
}

/*ID 20: Switch for the calibration input on the ACC*/
void FEACCInterface::toggleCal(int onoff, unsigned int channelmask, unsigned int boardMask)
{
	unsigned int command = 0x00C00000;
	std::string writeBuffer;
	//the firmware just uses the channel mask to toggle
	//switch lines. So if the cal is off, all channel lines
	//are set to be off. Else, uses channel mask
	if(onoff == 1)
	{
		//channelmas is default 0x7FFF
		command = (command | (boardMask << 24)) | channelmask;
		OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0100, 0x00c10001|(boardMask<<24));
		OtsUDPHardware::write(writeBuffer);
	}
	else if(onoff == 0)
	{
		command = (command | (boardMask << 24));
		OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0100, 0x00c10000|(boardMask<<24));
		OtsUDPHardware::write(writeBuffer);
	}
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0100, command);
	OtsUDPHardware::write(writeBuffer);
          

}
/*------------------------------------------------------------------------------------*/
/*---------------------------Read functions listening for data------------------------*/

/*ID 15: Main listen fuction for data readout.*/
int FEACCInterface::listenForAcdcData()
{
  //TODO: This function is not used in otsdaq setup. -Jin
//    //setup a sigint capturer to safely
//    //reset the boards if a ctrl-c signal is found
//    struct sigaction sa;
//    memset( &sa, 0, sizeof(sa) );
//    sa.sa_handler = got_signal;
//    sigfillset(&sa.sa_mask);
//    sigaction(SIGINT,&sa,NULL);

    int eventCounter = 0;
    while(eventCounter<params_.eventNumber)
    {
        ++eventCounter;
        if(params_.triggerMode == 1)
        {
            softwareTrigger();

            //ensure we are past the 80 us PSEC read time
            usleep(300);

            //check if hardware buffers are filling up
            //and give time for readout to catch up.
	    //TODO: nEvtsMax counter does not work now. 
            //while(eventCounter - nEvtsMax >= 4)
	    //{
	    //usleep(100);
	    //}
        }
    }


    return 0;
}
/*------------------------------------------------------------------------------------*/
/*---------------------------Active functions for informations------------------------*/

/*ID 19: Pedestal setting procedure.*/
bool FEACCInterface::setPedestals(unsigned int boardmask, unsigned int chipmask, unsigned int adc)
{
    for(int iChip = 0; iChip < 5; ++iChip)
    {
	if(chipmask & (0x01 << iChip))
	{
	    unsigned int command = 0x00A20000;
	    command = (command | (boardmask << 24) ) | (iChip << 12) | adc;
		std::string writeBuffer;
	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0100, command);
	    OtsUDPHardware::write(writeBuffer);
          
	}
    }
    return true;
}


bool FEACCInterface::setPedestals(unsigned int boardmask, const std::vector<unsigned int>& pedestals)
{
    if(pedestals.size() != 5) 
    {
		__SS__ << "Error: Incorrect number of pedestal values ("<< std::to_string(pedestals.size()) << ") specified." << std::endl;
		__CFG_COUT_ERR__ << ss.str();
        return false;
    }

    for(int iChip = 0; iChip < 5; ++iChip)
    {
        unsigned int command = 0x00A20000;
        command = (command | (boardmask << 24) ) | (iChip << 12) | pedestals[iChip];
		std::string writeBuffer;
		OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0100, command);
		OtsUDPHardware::write(writeBuffer);
    }
    return true;
}

/*ID 24: Special function to check connected ACDCs for their firmware version*/ 
void FEACCInterface::versionCheck(bool debug)
{
    unsigned int command;
    std::string writeBuffer;
    std::string readBuffer;
    OtsUDPFirmwareCore::readAdvanced(writeBuffer,
				     0x1000, 32, 0, true);//flags=0, clear_buffer=true
    OtsUDPHardware::read(writeBuffer, readBuffer);
    std::vector<uint64_t> AccBuffer(readBuffer.size()/8);
    memcpy((void*)AccBuffer.data(), (void*)readBuffer.c_str(), readBuffer.size());
    if(AccBuffer.size()==32)
    {
        __CFG_COUT__ << "ACC has the firmware version: " << std::hex << AccBuffer.at(0) << std::dec;
        uint16_t year  = (AccBuffer[1] >> 16) & 0xffff;
        uint16_t month = (AccBuffer[1] >>  8) & 0xff;
        uint16_t day   = (AccBuffer[1] >>  0) & 0xff;
        __CFG_COUT__ << " from " << std::hex << month << "/" << std::hex << day << "/" << std::hex << year << std::endl;

	if(debug)
	{

	    OtsUDPFirmwareCore::readAdvanced(writeBuffer,
					     0x1100, 64+32, 0, true);//flags=0, clear_buffer=true
	    OtsUDPHardware::read(writeBuffer, readBuffer);
	    std::vector<uint64_t> eAccBuffer(readBuffer.size()/8);
	    memcpy((void*)eAccBuffer.data(), (void*)readBuffer.c_str(), readBuffer.size());

	    printf("  PLL lock status:\n    System PLL: %d\n    Serial PLL: %d\n    DPA PLL 1:  %d\n    DPA PLL 2:  %d\n", (AccBuffer[2] & 0x1)?1:0, (AccBuffer[2] & 0x2)?1:0, (AccBuffer[2] & 0x4)?1:0, (AccBuffer[2] & 0x8)?1:0);
	    printf("  %-30s %10s %10s %10s %10s %10s %10s %10s %10s\n", "", "ACDC0", "ACDC1", "ACDC2", "ACDC3", "ACDC4", "ACDC5", "ACDC6", "ACDC7");
	    printf("  %-30s %10d %10d %10d %10d %10d %10d %10d %10d\n", "40 MPBS link rx clk fail", (AccBuffer[16] & 0x1)?1:0, (AccBuffer[16] & 0x2)?1:0, (AccBuffer[16] & 0x4)?1:0, (AccBuffer[16] & 0x8)?1:0, (AccBuffer[16] & 0x10)?1:0, (AccBuffer[16] & 0x20)?1:0, (AccBuffer[16] & 0x40)?1:0, (AccBuffer[16] & 0x80)?1:0);
	    printf("  %-30s %10d %10d %10d %10d %10d %10d %10d %10d\n", "40 MPBS link align err", (AccBuffer[17] & 0x1)?1:0, (AccBuffer[17] & 0x2)?1:0, (AccBuffer[17] & 0x4)?1:0, (AccBuffer[17] & 0x8)?1:0, (AccBuffer[17] & 0x10)?1:0, (AccBuffer[17] & 0x20)?1:0, (AccBuffer[17] & 0x40)?1:0, (AccBuffer[17] & 0x80)?1:0);
	    printf("  %-30s %10d %10d %10d %10d %10d %10d %10d %10d\n", "40 MPBS link decode err", (AccBuffer[18] & 0x1)?1:0, (AccBuffer[18] & 0x2)?1:0, (AccBuffer[18] & 0x4)?1:0, (AccBuffer[18] & 0x8)?1:0, (AccBuffer[18] & 0x10)?1:0, (AccBuffer[18] & 0x20)?1:0, (AccBuffer[18] & 0x40)?1:0, (AccBuffer[18] & 0x80)?1:0);
	    printf("  %-30s %10d %10d %10d %10d %10d %10d %10d %10d\n", "40 MPBS link disparity err", (AccBuffer[19] & 0x1)?1:0, (AccBuffer[19] & 0x2)?1:0, (AccBuffer[19] & 0x4)?1:0, (AccBuffer[19] & 0x8)?1:0, (AccBuffer[19] & 0x10)?1:0, (AccBuffer[19] & 0x20)?1:0, (AccBuffer[19] & 0x40)?1:0, (AccBuffer[19] & 0x80)?1:0);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "40 MPBS link Rx FIFO Occ", eAccBuffer[56], eAccBuffer[57], eAccBuffer[58], eAccBuffer[59], eAccBuffer[60], eAccBuffer[61], eAccBuffer[62], eAccBuffer[63]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MPBS Byte FIFO 0 Occ", eAccBuffer[0], eAccBuffer[2], eAccBuffer[4], eAccBuffer[6], eAccBuffer[8], eAccBuffer[10], eAccBuffer[12], eAccBuffer[14]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MPBS Byte FIFO 1 Occ", eAccBuffer[1], eAccBuffer[3], eAccBuffer[5], eAccBuffer[7], eAccBuffer[9], eAccBuffer[11], eAccBuffer[13], eAccBuffer[15]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MPBS PRBS Err 0", eAccBuffer[16], eAccBuffer[18], eAccBuffer[20], eAccBuffer[22], eAccBuffer[24], eAccBuffer[26], eAccBuffer[28], eAccBuffer[30]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MPBS PRBS Err 1", eAccBuffer[17], eAccBuffer[19], eAccBuffer[21], eAccBuffer[23], eAccBuffer[25], eAccBuffer[27], eAccBuffer[29], eAccBuffer[31]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MPBS Symbol Err 0", eAccBuffer[32], eAccBuffer[34], eAccBuffer[36], eAccBuffer[38], eAccBuffer[40], eAccBuffer[42], eAccBuffer[44], eAccBuffer[46]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MPBS Symbol Err 1", eAccBuffer[33], eAccBuffer[35], eAccBuffer[37], eAccBuffer[39], eAccBuffer[41], eAccBuffer[43], eAccBuffer[45], eAccBuffer[47]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MPBS parity Err 0", eAccBuffer[64], eAccBuffer[66], eAccBuffer[68], eAccBuffer[70], eAccBuffer[72], eAccBuffer[74], eAccBuffer[76], eAccBuffer[78]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MPBS parity Err 1", eAccBuffer[65], eAccBuffer[67], eAccBuffer[69], eAccBuffer[71], eAccBuffer[73], eAccBuffer[75], eAccBuffer[77], eAccBuffer[79]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "250 MBPS FIFO Occ", eAccBuffer[48], eAccBuffer[49], eAccBuffer[50], eAccBuffer[51], eAccBuffer[52], eAccBuffer[53], eAccBuffer[54], eAccBuffer[55]);
	    printf("  %-30s %10lu %10lu %10lu %10lu %10lu %10lu %10lu %10lu\n", "Self trig count", eAccBuffer[80], eAccBuffer[81], eAccBuffer[82], eAccBuffer[83], eAccBuffer[84], eAccBuffer[85], eAccBuffer[86], eAccBuffer[87]);
	    printf("\n");
	}
    }
    else
    {
        __CFG_COUT__ << "ACC got the no info frame" << std::endl;
    }

   
    //flush ACDC slow control FIFOs 
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/0x0002, /*data*/0xff);
    OtsUDPHardware::write(writeBuffer);    
	
    //Request ACDC info frame 
    command = 0xFFD00000; 
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, command);
    OtsUDPHardware::write(writeBuffer);

    usleep(500);

    //Loop over the ACC buffer words that show the ACDC buffer size
    //32 words represent a connected ACDC
    for(int i = 0; i < MAX_NUM_BOARDS; i++)
    {
	u_int64_t readQuadWord;
    	OtsUDPFirmwareCore::readAdvanced(writeBuffer, 0x1138+i);
    	OtsUDPHardware::read(writeBuffer, readQuadWord);
        uint64_t bufLen = readQuadWord;
        if(bufLen > 5)
        {

	    OtsUDPFirmwareCore::readAdvanced(writeBuffer,
					     0x1200+i, bufLen, 0x08, true);//NO_ADDR_INC=0x08, clear_buffer=true
	    OtsUDPHardware::read(writeBuffer, readBuffer);
	    std::vector<uint64_t> buf(readBuffer.size()/8);
	    memcpy((void*)buf.data(), (void*)readBuffer.c_str(), readBuffer.size());
	    __CFG_COUT__ << "Board " << i << " has the firmware version: " << std::hex << buf.at(2) << std::dec;
	    __CFG_COUT__ << " from " << std::hex << ((buf.at(4) >> 8) & 0xff) << std::dec << "/" << std::hex << (buf.at(4) & 0xff) << std::dec << "/" << std::hex << buf.at(3) << std::dec << std::endl;

	    if(debug)
	    {
		printf("  Header/footer: %4lx %4lx %4lx %4lx (%s)\n", buf[0], buf[1], buf[30], buf[31], (buf[0] == 0x1234 && buf[1] == 0xbbbb && buf[30] == 0xbbbb && buf[31] == 0x4321)?"Correct":"Wrong");
		printf("  PLL lock status:\n    ACC PLL:    %d\n    Serial PLL: %d\n    JC PLL:     %d\n    WR PLL:     %d\n", (buf[6] & 0x4)?1:0, (buf[6] & 0x2)?1:0, (buf[6] & 0x8)?1:0, (buf[6] & 0x1)?1:0);
		printf("  FLL Locks:              %8lx\n", (buf[6] >> 4)&0x1f);
		printf("  Backpressure:           %8d\n", (buf[5] & 0x2)?1:0);
		printf("  40 MBPS parity error:   %8d\n", (buf[5] & 0x1)?1:0);
		printf("  Event count:            %8lu\n", (buf[15] << 16) | buf[16]);
		printf("  ID Frame count:         %8lu\n", (buf[17] << 16) | buf[18]);
		printf("  Trigger count all:      %8lu\n", (buf[11] << 16) | buf[12]);
		printf("  Trigger count accepted: %8lu\n", (buf[13] << 16) | buf[14]);
		printf("  PSEC0 FIFO Occ:         %8lu\n", buf[21]);
		printf("  PSEC1 FIFO Occ:         %8lu\n", buf[22]);
		printf("  PSEC2 FIFO Occ:         %8lu\n", buf[23]);
		printf("  PSEC3 FIFO Occ:         %8lu\n", buf[24]);
		printf("  PSEC4 FIFO Occ:         %8lu\n", buf[25]);
		printf("  Wr time FIFO Occ:       %8lu\n", buf[26]);
		printf("  Sys time FIFO Occ:      %8lu\n", buf[27]);
		printf("\n");

		std::vector<std::vector<uint64_t>> bufs;
		for(int j = 0; j < 5; ++j)
		{
		    command = (0x01000000 << i) | (0x00D00001 + j); 
		    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, command);
		    OtsUDPHardware::write(writeBuffer);
		    usleep(500);
		    OtsUDPFirmwareCore::readAdvanced(writeBuffer, 0x1138+i);
		    OtsUDPHardware::read(writeBuffer, readQuadWord);
		    uint64_t bufLen = readQuadWord;
		    if(bufLen >= 32)
		    {
			OtsUDPFirmwareCore::readAdvanced(writeBuffer,
							 0x1200+i, bufLen, 0x08, true);//NO_ADDR_INC=0x08, clear_buffer=true
			OtsUDPHardware::read(writeBuffer, readBuffer);
			std::vector<uint64_t> tmpbuf(readBuffer.size()/8);
			memcpy((void*)tmpbuf.data(), (void*)readBuffer.c_str(), readBuffer.size());

			bufs.push_back(tmpbuf);
		    }
		}
		printf("    PSEC4:                      %8ld  %8ld  %8ld  %8ld  %8ld\n", bufs[0][16], bufs[1][16], bufs[2][16], bufs[3][16], bufs[4][16]); 
		printf("    RO Feedback count:          %8ld  %8ld  %8ld  %8ld  %8ld\n", bufs[0][3], bufs[1][3], bufs[2][3], bufs[3][3], bufs[4][3]); 
		printf("    RO Feedback target:         %8ld  %8ld  %8ld  %8ld  %8ld\n", bufs[0][4], bufs[1][4], bufs[2][4], bufs[3][4], bufs[4][4]);
		printf("    pro Vdd:                    %8ld  %8ld  %8ld  %8ld  %8ld\n", bufs[0][7], bufs[1][7], bufs[2][7], bufs[3][7], bufs[4][7]); 
		printf("    Vbias:                      %8ld  %8ld  %8ld  %8ld  %8ld\n", bufs[0][5], bufs[1][5], bufs[2][5], bufs[3][5], bufs[4][5]);
		printf("    Self trigger threshold 0:   %8ld  %8ld  %8ld  %8ld  %8ld\n", bufs[0][6], bufs[1][6], bufs[2][6], bufs[3][6], bufs[4][6]); 
		printf("    vcdl count:                 %8ld  %8ld  %8ld  %8ld  %8ld\n", (bufs[0][14] << 16) | bufs[0][13], (bufs[1][14] << 16) | bufs[1][13], (bufs[2][14] << 16) | bufs[2][13], (bufs[3][14] << 16) | bufs[3][13], (bufs[4][14] << 16) | bufs[4][13]); 
		printf("    DLL Vdd:                    %8ld  %8ld  %8ld  %8ld  %8ld\n", bufs[0][15], bufs[1][15], bufs[2][15], bufs[3][15], bufs[4][15]); 
		printf("\n");

	    }
        }
        else
        {
            __CFG_COUT__ << "Board " << i << " is not connected" << std::endl;
        }
    }
}


/*------------------------------------------------------------------------------------*/
/*-------------------------------------Help functions---------------------------------*/

/*ID 13: Fires the software trigger*/
void FEACCInterface::softwareTrigger()
{
	std::string writeBuffer;
   	//Software trigger
   	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0010, /*data*/0xff);
   	OtsUDPHardware::write(writeBuffer);
}

/*ID 16: Used to dis/enable transfer data from the PSEC chips to the buffers*/
void FEACCInterface::enableTransfer(int onoff, int acdcMask)
{
    unsigned int command;
    command = 0x00F60000;
    std::string writeBuffer;
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0100, ((0xff&acdcMask) << 24) | command | onoff);
    OtsUDPHardware::write(writeBuffer);
}
/*ID 18: Tells ACDCs to clear their ram.*/ 
void FEACCInterface::dumpData(unsigned int boardMask)
{
    //send and read.
	std::string writeBuffer;
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0001, /*data*/boardMask);
    OtsUDPHardware::write(writeBuffer);
          
}

void FEACCInterface::resetLinks()
{
    std::string writeBuffer;
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0023, /*data*/0);
    OtsUDPHardware::write(writeBuffer);
    usleep(100);
    dumpData(params_.boardMask);
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0023, /*data*/1);
    OtsUDPHardware::write(writeBuffer);

}
/*ID 27: Resets the ACDCs*/
void FEACCInterface::resetACDC(unsigned int boardMask)
{
    unsigned int command = 0x00FF0000;
	std::string writeBuffer;
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, command | (boardMask << 24));
    OtsUDPHardware::write(writeBuffer);
          
    __CFG_COUT__ << "ACDCs were reset" << std::endl;
}

/*ID 28: Resets the ACCs*/
void FEACCInterface::resetACC()
{
	std::string writeBuffer;
    
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/0x1ffffffff, /*data*/1);
    OtsUDPHardware::write(writeBuffer);
    sleep(5);//TODO: I do not know if it is wise to stop the data processing thread like this. -Jin
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, /*address*/0x0, /*data*/1);
    OtsUDPHardware::write(writeBuffer);
          
    __CFG_COUT__ << "ACC was reset" << std::endl;
}


/*------------------------------------------------------------------------------------*/
/*-------------------------------------Help functions---------------------------------*/
/*ID 25: Scan possible high speed link clock phases and select the optimal phase setting*/ 
void FEACCInterface::scanLinkPhase(unsigned int boardMask, bool print)
{
    std::vector<std::vector<uint64_t>> errors;

    std::stringstream printout;

    if(print)
    {
        printout << "Fast link phase scan\ngPhase  ";
        for(int iChan = 0; iChan < 8; iChan += 2) printout << setw(25) << "ACDC:" << setw(2) << iChan/2 << setw(21) << " ";
        printout << "\n      ";
        for(int iChan = 0; iChan < 8; ++iChan)    printout << setw(12) << "Channel:" << setw(2) << iChan%2 << "          ";
        printout << "\n      ";
        for(int iChan = 0; iChan < 8; ++iChan)    printout << setw(10) << "Encode err" << setw(9) <<  "PRBS err";
	printout << "\n";
    }
    
    for(int iOffset = 0; iOffset < 24; ++iOffset)
    {
        // advance phase one step (there are 24 total steps in one clock cycle)
	std::string writeBuffer;
    	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0054, /*data*/0);
	OtsUDPHardware::write(writeBuffer);
        for(int iChan = 0; iChan < 8; ++iChan)
        {
	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0055, /*data*/iChan);
	    OtsUDPHardware::write(writeBuffer);
	    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0056, /*data*/0);
	    OtsUDPHardware::write(writeBuffer);          
        }

        // transmit idle pattern to make sure link is aligned 
        enableTransfer(0);

        usleep(1000);

        //transmit PRBS pattern 
        enableTransfer(1);

        usleep(100);

        //reset error counters 
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0053, 0);
	OtsUDPHardware::write(writeBuffer);
          
        usleep(1000);
	OtsUDPFirmwareCore::readAdvanced(writeBuffer, 0x1120, 8, 0, true);//flags=0, clear_buffer=true
	std::vector<uint64_t> decode_errors;
	OtsUDPHardware::read(writeBuffer, decode_errors);
        if(print)
        {

	    OtsUDPFirmwareCore::readAdvanced(writeBuffer, 0x1110, 8, 0, true);//flags=0, clear_buffer=true
	    std::vector<uint64_t> prbs_errors;
	    OtsUDPHardware::read(writeBuffer, prbs_errors);

            printout << setw(5) << iOffset << "  ";
            for(int iChan = 0; iChan < 8; ++iChan) printout << setw(10) << uint32_t(decode_errors[iChan]) << setw(9) << uint32_t(prbs_errors[iChan]) << "     ";
            printout << "\n";
        }

        errors.push_back(decode_errors);
    }

    // set transmitter back to idle mode
    enableTransfer(0);

    if(boardMask)
    {
        if(print) printout << "Set:   "; 
        for(int iChan = 0; iChan < 8; ++iChan)
        {
            // set phase for channels in boardMask
            if(boardMask & (1 << iChan))
            {
                int stop = 0;
                int length = 0;
                int length_best = 0;
                for(int i = 0; i < int(2*errors.size()); ++i)
                {
                    int imod = i % errors.size();
                    if(errors[imod][iChan] == 0)
                    {
                        ++length;
                    }
                    else
                    {
                        if(length >= length_best)
                        {
                            stop = imod;
                            length_best = length;
                        }
                        length = 0;
//                if(i > int(errors.size())) break;
                    }
                }
                int phaseSetting = (stop - length_best/2)%errors.size();
                if(print) printout << setw(15) << phaseSetting << "          ";
		std::string writeBuffer;
		OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0054, /*data*/0);
		OtsUDPHardware::write(writeBuffer);
                OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0055, /*data*/iChan);
		OtsUDPHardware::write(writeBuffer);
                for(int i = 0; i < phaseSetting; ++i)
                {
                    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0056, /*data*/0);
		    OtsUDPHardware::write(writeBuffer);
                	    
                }
            }
            else
            {
                if(print) printout << setw(25) << " ";
            }
        }
        if(print) printout << "\n"; 

	if(print) __CFG_COUT__ << printout.str() << __E__;

	// ensure at least 1 ms for links to realign (ensures at least 25 alignment markers)
	usleep(1000);

        //reset error counters
	std::string writeBuffer;
	OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x0053, 0);
	OtsUDPHardware::write(writeBuffer); 
    }

}

void FEACCInterface::sendJCPLLSPIWord(unsigned int word, unsigned int boardMask, bool verbose)
{
    unsigned int clearRequest = 0x00F10000 | (boardMask << 24);
    unsigned int lower16 = 0x00F30000 | (boardMask << 24) | (0xFFFF & word);
    unsigned int upper16 = 0x00F40000 | (boardMask << 24) | (0xFFFF & (word >> 16));
    unsigned int setPLL = 0x00F50000 | (boardMask << 24);
    std::string writeBuffer;
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, clearRequest);
    OtsUDPHardware::write(writeBuffer);
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, lower16);
    OtsUDPHardware::write(writeBuffer);
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, upper16);
    OtsUDPHardware::write(writeBuffer);
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, setPLL);
    OtsUDPHardware::write(writeBuffer);
    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, clearRequest);
    OtsUDPHardware::write(writeBuffer);

    if(verbose)
    {
	printf("send 0x%08x\n", lower16);
	printf("send 0x%08x\n", upper16);
	printf("send 0x%08x\n", setPLL);
    }
}

/*ID 26: Configure the jcPLL settings */
void FEACCInterface::configJCPLL(unsigned int boardMask)
{
    // program registers 0 and 1 with approperiate settings for 40 MHz output 
    //sendJCPLLSPIWord(0x55500060, boardMask); // 25 MHz input
    sendJCPLLSPIWord(0x5557C060, boardMask); // 125 MHz input
    usleep(2000);    
    //sendJCPLLSPIWord(0x83810001, boardMask); // 25 MHz input
    sendJCPLLSPIWord(0xFF810081, boardMask); // 125 MHz input
    usleep(2000);

    // cycle "power down" to force VCO calibration 
    sendJCPLLSPIWord(0x00001802, boardMask);
    usleep(2000);
    sendJCPLLSPIWord(0x00001002, boardMask);
    usleep(2000);
    sendJCPLLSPIWord(0x00001802, boardMask);
    usleep(2000);

    // toggle sync bit to synchronize output clocks
    sendJCPLLSPIWord(0x0001802, boardMask);
    usleep(2000);
    sendJCPLLSPIWord(0x0000802, boardMask);
    usleep(2000);
    sendJCPLLSPIWord(0x0001802, boardMask);
    usleep(2000);

    // read register
//    sendJCPLLSPIWord(0x0000000e);
//    sendJCPLLSPIWord(0x00000000);

    // write register contents to EEPROM
    //sendJCPLLSPIWord(0x0000001f);

}

std::vector<uint64_t> FEACCInterface::readSlowControl(const int iacdc, const int timeout)
{
    std::string writeBuffer;
    uint64_t readQuadWord;

    OtsUDPFirmwareCore::writeAdvanced(writeBuffer, 0x100, 0x00D00000 | (1 << (iacdc + 24)));
    OtsUDPHardware::write(writeBuffer);
	
    //wait until we have fully received all 32 expected words from the ACDC
    int iTimeout = timeout;
    OtsUDPFirmwareCore::readAdvanced(
	writeBuffer,
	0x1138+iacdc);  // This register of ACC stores the occupancy of the buffer which stores words from ACDC.
	    
    OtsUDPHardware::read(writeBuffer, readQuadWord);
    while(readQuadWord < 32 && iTimeout > 0)
    {
	usleep(10);
	--iTimeout;
	OtsUDPHardware::read(writeBuffer,readQuadWord);
    }
    if(iTimeout == 0) 
    {
	__SS__ << "ERROR: ACDC info frame retrieval timeout." << std::endl;
	__CFG_COUT_ERR__ << ss.str();
	throw std::runtime_error(ss.str());
    }
    OtsUDPFirmwareCore::readAdvanced(
	writeBuffer,
	0x1200+iacdc, 32, 0x08, true);//NO_ADDR_INC=0x08, clear_buffer=true
	    
    std::vector<uint64_t> acdcInfo;
    OtsUDPHardware::read(writeBuffer, acdcInfo);
    return acdcInfo;
}

DEFINE_OTS_INTERFACE(FEACCInterface)
