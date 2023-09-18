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
    //Is this a good practice?
    ConfigurationTree optionalLink =
      theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("LinkToACCInterfaceTable");
    //acdc_board_numbers = optionalLink.getNode("AcdcBoardNumbers").getValue<vector<int>>(); TODO
    for(unsigned int i = 0;i<8;i++)
    {
      try
      {
	//TODO: Initializing parameters with dummy variables.
	acdc_board_numbers = {0, 1, 2};
	acdc_board_ids={"ACDC60", "ACDC61", "ACDC62"};
        //acdc_board_numbers.push_back(optionalLink.getNode("LinkToACDC"+std::to_string(i)+"Parameters").getNode("BoardNumber").getValue<int>());
        //acdc_board_ids.push_back(optionalLink.getNode("LinkToACDC"+std::to_string(i)+"Parameters").getNode("InterfaceID").getValue<std::string>());
      }
      catch(...)
      {
	//do nothing
      }
    }

    current_pk_ = 0;
    lastPacketID_ = -1;
}

//==============================================================================
ACCBurstDataSaverConsumer::~ACCBurstDataSaverConsumer(void) {}

//==============================================================================
void ACCBurstDataSaverConsumer::openFile(std::string runNumber)
{
	currentRunNumber_ = runNumber;
    for(unsigned int i = 0;i<acdc_board_numbers.size();i++)
    {
        std::stringstream fileName;
        fileName << filePath_ << "/" << fileRadix_<<"_"<< acdc_board_ids[i] << "_Run" << runNumber;//acdcs[i].getBoardIndex()
        // if split file is there then subrunnumber must be set!
        if(maxFileSize_ > 0)
            fileName << "_" << currentSubRunNumber_;
        fileName << "_Raw.dat";
        __CFG_COUT__ << "Saving file: " << fileName.str() << std::endl;
	outFiles_.emplace_back(fileName.str().c_str(), std::ios::out | std::ios::binary);
        if(!outFiles_[i].is_open())
        {
            __CFG_SS__ << "Can't open file " << fileName.str() << std::endl;
            __CFG_SS_THROW__;
        }
    }
    //writeHeader();  // write start of file header. It has to be written for each file.
    __CFG_COUT__ << "All" << acdc_board_numbers.size() << "files opened successfully." << std::endl;
}

//==============================================================================
void ACCBurstDataSaverConsumer::closeFile(void)
{
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
  __CFG_COUT__ << "Attempting to save data with length:" << data.length() <<std::endl;
  const uint64_t* packet_data = reinterpret_cast<const uint64_t*>(data.c_str()+2);
  
  //Check packet ID to ensure we have not dropped any packets
  unsigned int currentPacketID = (unsigned int)*(reinterpret_cast<const unsigned char*>(data.c_str()+1));
  unsigned int nextPacketID = ((lastPacketID_+1)%256);
  if(lastPacketID_ > 0 && nextPacketID != currentPacketID)
  {
      __CFG_SS__ << "Dropped packet: Jumped from packet ID " << lastPacketID_ << " to " << currentPacketID << std::endl;
      //TODO: Instead of throwing, enter recovery mode
      //__CFG_SS_THROW__;
      current_pk_=0;
      current_file_ = -1;
  }
  lastPacketID_ = currentPacketID;

  __CFG_COUT__ << "current packet: " << current_pk_ << std::endl;
  if(current_pk_==0)
  {
    __CFG_COUT__ <<"Packet_data 0 is:"<< std::hex << packet_data[0] <<std::endl;
    if((packet_data[0]&0xffffffffffffff00) == 0x123456789abcde00 && (packet_data[1]&0xffff000000000000)==0xac9c000000000000)
    {
        int data_bi = packet_data[0] & 0xff;
	current_file_ = -1;
	for(unsigned int i =0;i<acdc_board_numbers.size();i++)
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
        __CFG_SS__ << "Header error: "<< std::hex << packet_data[0] << " " << std::hex << packet_data[1] << std::endl;
	//__CFG_SS_THROW__;
	return;
	//TODO: packet realigning code goes here. Unfortunately, in otsdaq direct ethernet control is not so straightforward at this point. -Jin
    }
  }
  if(current_file_ == -1)
  {
      __CFG_SS__ << "Current file not set" << std::endl;
      //TODO: Instead of throwing, enter recovery mode
      current_pk_=0;
      __CFG_SS_THROW__;
      return;
  }
  current_pk_=(current_pk_+1)%8;
  __CFG_COUT__ << "Current file: " << current_file_ << std::endl;
  //TODO: We should buffer a full event and then write or we may end up writing partial events.  
  outFiles_[current_file_].write(data.c_str()+2, data.size()-2);  
  __CFG_COUT__ << "Wrote " << data.size() << " bytes successfully."<< std::endl;
	
}


DEFINE_OTS_PROCESSOR(ACCBurstDataSaverConsumer)
