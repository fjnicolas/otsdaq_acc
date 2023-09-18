#include "ACDC.h"
#include "otsdaq/ConfigurationInterface/ConfigurationTree.h"

#include <bitset>
#include <sstream>
#include <fstream>
#include <chrono> 
#include <iomanip>
#include <numeric>
#include <ctime>

using namespace std;

ACDC::ACDC() : boardIndex(-1), nEvents_(0) {}

ACDC::ACDC(int bi) : boardIndex(bi), nEvents_(0) {}

ACDC::~ACDC()
{
}

ACDC::ConfigParams::ConfigParams() :
    reset(false),
    pedestals(0x800, 5),
    selfTrigPolarity(0),
    triggerThresholds(0x780, 30),
    selfTrigMask(0),
    calibMode(false),
    dll_vdd(0xcff),
    acc_backpressure(true)
{
}

int ACDC::getBoardIndex() const
{
	return boardIndex;
}

void ACDC::setBoardIndex(int bi)
{
	boardIndex = bi;
}

void ACDC::parseConfig(const ots::ConfigurationTree& config)
{
    params_.reset = config.getNode("ResetACDCOnStart").getValue<bool>();
    params_.pedestals = std::vector<unsigned int>(5, config.getNode("Pedestals").getValue<unsigned int>());
    //The function which enabled sequence pedestal input is not implemented for now.


    params_.selfTrigPolarity = config.getNode("SelfTrigPolarity").getValue<int>();
    params_.triggerThresholds = std::vector<unsigned int>(30, config.getNode("SelfTrigThresholds").getValue<unsigned int>());
    // //The function which enabled sequence selfTrigThresholds input is not implemented for now.

    params_.selfTrigMask = config.getNode("SelfTrigMask").getValue<unsigned int>();
    params_.calibMode = config.getNode("CalibMode").getValue<bool>();
    params_.acc_backpressure = config.getNode("ACCBackpressure").getValue<bool>();
    params_.dll_vdd = config.getNode("DllVdd").getValue<unsigned int>();
}

//looks at the last ACDC buffer and organizes
//all of the data into a data map. The boolean
//argument toggles whether you want to subtract
//pedestals and convert ADC-counts to mV live
//or keep the data in units of raw ADC counts. 
//retval:  ... 1 and 2 are never returned ... 
//2: other error
//1: corrupt buffer 
//0: all good
int ACDC::parseDataFromBuffer(const vector<uint64_t>& buffer)
{
    //Catch empty buffers
    if(buffer.size() == 0)
    {
        std::cout << "You tried to parse ACDC data without pulling/setting an ACDC buffer" << std::endl;
        return -1;
    }

    //clear the data map prior.
    data.clear();

    //check for fixed words in header
    if(((buffer[1] >> 48) & 0xffff) != 0xac9c || (buffer[4] & 0xffff) != 0xcac9)
    {
        printf("%lx, %lx\n", (buffer[1] >> 48) & 0xffff, buffer[4] & 0xffff);
        std::cout << "Data buffer header corrupt" << std::endl;
        return -2;
    }

    //Fill data map
    int channel_count = 0;
    int cap_count = 0;
    decltype(data.emplace()) empl_retval;
    for(unsigned int i = 5; i < buffer.size(); ++i)
    {
        for(int j = 4; j >=0; --j)
        {
            if(cap_count == 0) empl_retval = data.emplace(std::piecewise_construct, std::forward_as_tuple(channel_count), std::forward_as_tuple(256));
            (*(empl_retval.first)).second[cap_count] = (buffer[i] >> (j*12)) & 0xfff;
            ++cap_count;
            if(cap_count >= 256)
            {
                cap_count = 0;
                ++channel_count;
            }
        }
    }

    if(data.size()!=NUM_CH)
    {
        cout << "error 1: Not 30 channels " << data.size() << endl;
        for(const auto& thing : data) cout << thing.second.size() << std::endl;
    }

    for(int i=0; i<NUM_CH; i++)
    {
        if(data[i].size()!=NUM_SAMP)
        {
            cout << "error 2: not 256 samples in channel " << i << endl;
        }
    }

    return 0;
}









