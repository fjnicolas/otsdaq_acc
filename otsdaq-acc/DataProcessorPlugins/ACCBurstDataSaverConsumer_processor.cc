#include "otsdaq-acc/DataProcessorPlugins/ACCBurstDataSaverConsumer.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq-acc/ACC/ACDC.h"

#include <algorithm>
#include <vector>

using namespace ots;

//==============================================================================
ACCBurstDataSaverConsumer::ACCBurstDataSaverConsumer(
    std::string              supervisorApplicationUID,
    std::string              bufferUID,
    std::string              processorUID,
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       configurationPath)
    : WorkLoop(processorUID)
    , RawDataSaverConsumerBase(supervisorApplicationUID,
                               bufferUID,
                               processorUID,
                               theXDAQContextConfigTree,
                               configurationPath)
{
    current_pk_ = 0;
    lastPacketID_ = -1;
}

//==============================================================================
ACCBurstDataSaverConsumer::~ACCBurstDataSaverConsumer(void) {}


//==============================================================================
void ACCBurstDataSaverConsumer::configure(void)
{
    acdc_board_numbers.clear();
    acdc_board_ids.clear();

    //Is this a good practice?
    try
    {
	ConfigurationTree accTable = theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("LinkToACCInterfaceTable");
	uint32_t acdcMask = accTable.getNode("ACDCMask").getValue<uint32_t>();
	for(unsigned int i = 0; i < 8; i++)
	{
	    if(acdcMask & (1 << i))
	    {
		acdc_board_numbers.push_back(i);
		acdc_board_ids.push_back(accTable.getNode("LinkToACDC"+std::to_string(i)+"Parameters").getNode("InterfaceID").getValue<std::string>());
	    }
	}
    }
    catch(...)
    {
	//Settings not found
	__CFG_COUT__ << "ACC table parsive failed, falling back to default ACDC labels" << std::endl;
	acdc_board_numbers.clear();
	acdc_board_ids.clear();
	acdc_board_numbers = {0, 1, 2, 3};
	acdc_board_ids = {"ACDC0", "ACDC1", "ACDC2","ACDC3"};
    }

    current_pk_ = 0;
    lastPacketID_ = -1;    
}


//==============================================================================
void ACCBurstDataSaverConsumer::openFile(std::string runNumber)
{
    outFiles_.clear();
    currentRunNumber_ = runNumber;
    for(unsigned int i = 0;i<acdc_board_numbers.size();i++)
    {
	std::stringstream fileName;
	fileName << filePath_ << "/" << fileRadix_<<"_"<< acdc_board_ids[i] << "_Run" << runNumber;//acdcs[i].getBoardIndex()

	// if split file is there then subrunnumber must be set!
	if(maxFileSize_ > 0) fileName << "_" << currentSubRunNumber_;

	fileName << "_Raw.dat";
	__CFG_COUT__ << "Saving file: " << fileName.str() << std::endl;

	outFiles_.emplace_back(fileName.str(), std::ios::out | std::ios::binary);

	if(!outFiles_[i].is_open())
        {
	    __CFG_SS__ << "Can't open file " << fileName.str() << std::endl;
	    __CFG_SS_THROW__;
        }
    }
    packetCount_ = 0;
}

//==============================================================================
void ACCBurstDataSaverConsumer::closeFile(void)
{
    __CFG_COUT__ << "Packet Count: " << packetCount_ << __E__;
    for(unsigned int i = 0;i<acdc_board_numbers.size();i++)
    {
        if(outFiles_[i].is_open())
        {
            //writeFooter();  // write end of file footer. However, it should be written at each file.
            outFiles_[i].close();
        }
    }
}

//==============================================================================
void ACCBurstDataSaverConsumer::save(const std::string& data)
{
  //__CFG_COUT__ << "Attempting to save data with length:" << data.length() <<std::endl;
  ++packetCount_;

  //Check packet ID to ensure we have not dropped any packets
  unsigned int currentPacketID = (unsigned int)*(reinterpret_cast<const unsigned char*>(data.c_str()+1));
  unsigned int nextPacketID = ((lastPacketID_+1)%256);
  if(lastPacketID_ > 0 && nextPacketID != currentPacketID)
  {
      __CFG_COUT__ << "Dropped packet: Jumped from packet ID " << lastPacketID_ << " to " << currentPacketID << "\t" << packetCount_ << "\n";
      //Packet loss, assume the current event is lost and start search for next header
      current_pk_ = 0;
      current_file_ = -1;
  }
  lastPacketID_ = currentPacketID;

  const uint64_t* packet_data = reinterpret_cast<const uint64_t*>(data.c_str()+2);
  //__CFG_COUT__ << "current packet: " << current_pk_ << std::endl;  
  if(current_pk_==0)
  {
    //__CFG_COUT__ <<"Packet_data 0 is:"<< std::hex << packet_data[0] << "\t" << packet_data[1] << std::endl;
    if((packet_data[0]&0xffffffffffffff00) == 0x123456789abcde00 && 
       (packet_data[1]&0xffff000000000000) == 0xac9c000000000000)
    {
        int data_bi = packet_data[0] & 0xff;
	current_file_ = -1;
	for(unsigned int i = 0; i < acdc_board_numbers.size(); i++)
        {
	    //base command for set data readmode and which board bi to read
	    int bi = acdc_board_numbers[i];//acdcs[i].getBoardIndex();

	    if(data_bi == bi)
            {
	      current_file_ = i;
	      break;
            }
        }
	if(current_file_ == -1)
	  {
	    __CFG_SS__ << "Board number not found in the config but got a UDP packet with it: " << data_bi << std::endl;
	    __CFG_SS_THROW__;
	  }
    }
    else
    {
        __CFG_COUT__ << "Header error: "<< std::hex << packet_data[0] << " " << std::hex << packet_data[1] << std::endl;
	return;
	//Skip to next packet in search of valid header.  
    }
  }
  if(current_file_ < 0 || current_file_ >= 8)
  {
      __CFG_COUT__ << "Current file not set" << "\n";
      current_pk_=0;
      //Something went wrong, ignore this event and searchx for next header
      return;
  }

  current_pk_ = (current_pk_ + 1)%8;
  //TODO: We should buffer a full event and then write or we may end up writing partial events.  
  outFiles_[current_file_].write(data.c_str()+2, data.size()-2);  
  //__CFG_COUT__ << "Wrote " << data.size() << " bytes successfully."<< "\n";
	
}


DEFINE_OTS_PROCESSOR(ACCBurstDataSaverConsumer)
