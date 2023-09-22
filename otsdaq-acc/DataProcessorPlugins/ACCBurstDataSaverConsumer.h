#ifndef _ots_ACCBurstDataSaverConsumer_h_
#define _ots_ACCBurstDataSaverConsumer_h_

// This is the basic example of a raw data saver plugin.
// It can be used as is and it will save the data taken from a buffer in binary format
// without adding anything extra.  If you are planning on specializing any methods you
// should inherit from ACCBurstDataSaverConsumerBase the same way this consumer is
// doing.

#include "otsdaq/DataManager/RawDataSaverConsumerBase.h"

namespace ots
{
class ACCBurstDataSaverConsumer : public RawDataSaverConsumerBase
{
  public:
	ACCBurstDataSaverConsumer(std::string              supervisorApplicationUID,
	                              std::string              bufferUID,
	                              std::string              processorUID,
	                              const ConfigurationTree& theXDAQContextConfigTree,
	                              const std::string&       configurationPath);
	virtual ~ACCBurstDataSaverConsumer(void);
	virtual void configure(void) override;
	virtual void openFile(std::string runNumber) override;
	virtual void closeFile(void) override;
	virtual void save(const std::string& data) override;
  protected:
	void saveToFile();
	int current_pk_; //0-7, denotes packet number of each event. One event consists of 8 packets, 1445 words * 64 bit. The first word of the first packet determines the storage location. 
	int current_file_; //The index in outFiles_ we are currently writing to. Set at each 0th packet and stays the same for the following 7 packets.
	std::vector<std::ofstream> outFiles_; //one output file per ACDC board.
	std::vector<int> acdc_board_numbers;
	std::vector<std::string> acdc_board_ids;

	int lastPacketID_;
	int packetCount_ ;
};
}  // namespace ots

#endif
