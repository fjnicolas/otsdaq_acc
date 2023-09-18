#ifndef _ots_FEACCInterface_h_
#define _ots_FEACCInterface_h_

#define SAFE_BUFFERSIZE 100000 //used in setup procedures to guarantee a proper readout 
#define NUM_CH 30 //maximum number of channels for one ACDC board
#define MAX_NUM_BOARDS 8 // maxiumum number of ACDC boards connectable to one ACC 
#define ACCFRAME 32
#define ACDCFRAME 32
#define PPSFRAME 16
#define PSECFRAME 7696
#include <bitset>
#include <thread>
#include <vector>
#include <map>
#include "otsdaq-acc/ACC/ACDC.h"
#include "otsdaq-acc/ACC/BlockingQueue.h"
#include "otsdaq-components/FEInterfaces/FEOtsUDPTemplateInterface.h"

namespace ots
{
class FEInterfaceTableBase;
class FEACCInterfaceConfiguration;

class FEACCInterface : public FEOtsUDPTemplateInterface
{
  public:
	FEACCInterface(const std::string&       interfaceUID,
	                   const ConfigurationTree& theXDAQContextConfigree,
	                   const std::string&       interfaceConfigurationPath);
	virtual ~FEACCInterface(void);
	//-------------------------------------------------------------------------
	//Interface implementation-------------------------------------------------
	void configure(void) override;
	void halt(void) override;
	void pause(void) override;
	void resume(void) override;
	void start(std::string runNumber) override;
	bool running(void) override;
	void stop(void) override;

	/*------------------------------------------------------------------------------------*/
	/*--------------------------------Local return functions------------------------------*/
	/*ID Nan: Returns set triggermode */
	int getTriggermode(){return trigMode;} 

	/*------------------------------------------------------------------------------------*/
	/*-------------------------Local set functions for board setup------------------------*/
	/*-------------------Sets global variables, see below for description-----------------*/
	void setValidationStart(unsigned int in){validation_start=in;} //sets the validation window start delay for required trigger modes
	void setValidationWindow(unsigned int in){validation_window=in;} //sets the validation window length for required trigger modes
	void setTriggermode(int in){trigMode = in;} //sets the overall triggermode
	void setMetaSwitch(int in){metaSwitch = in;}

	/*------------------------------------------------------------------------------------*/
	/*-------------------------Local set functions for board setup------------------------*/
	/*ID:9 Create ACDC class instances for each connected ACDC board*/
	int createAcdcs(); 
	/*ID 10: Clear all ACDC class instances*/
	void clearAcdcs(); 
	/*ID:11 Queries the ACC for information about connected ACDC boards*/
	std::vector<int> whichAcdcsConnected(); 
	/*ID 12: Set up the software trigger*/
	void setSoftwareTrigger(unsigned int boardMask); 
	/*ID 13: Fires the software trigger*/
	void softwareTrigger(); 
	/*ID 15: Main listen fuction for data readout*/
	int listenForAcdcData(); 
	/*ID 16: Used to dis/enable transfer data from the PSEC chips to the buffers*/
	void enableTransfer(int onoff = 0, int acdcMask = 0xff);
	/*ID 18: Tells ACDCs to clear their ram.*/ 	
	void dumpData(unsigned int boardMask); 
	/*ID 19: Pedestal setting procedure.*/
	bool setPedestals(unsigned int boardmask, unsigned int chipmask, unsigned int adc); 
	bool setPedestals(unsigned int boardmask, const std::vector<unsigned int>& pedestals);
	/*ID 20: Switch for the calibration input on the ACC*/
	void toggleCal(int onoff, unsigned int channelmask = 0x7FFF,  unsigned int boardMask=0xFF); 
	/*ID 21: Set up the hardware trigger*/
	void setHardwareTrigSrc(int src, unsigned int boardMask = 0xFF); 
	/*ID 22: Special function to check the ports for connections to ACDCs*/
	void connectedBoards(); 
	/*ID 24: Special function to check connected ACDCs for their firmware version*/ 
	void versionCheck(bool debug = false);
	/*ID 25: Scan possible high speed link clock phases and select the optimal phase setting*/ 
	void scanLinkPhase(unsigned int boardMask, bool print = false);
        /*ID 26: Configure the jcPLL settings */
	void configJCPLL(unsigned int boardMask = 0xff);
        /*ID 27: Turn off triggers and data transfer off */
	void endRun();
	void resetLinks();
	void resetACDC(unsigned int boardMask = 0xff); //resets the acdc boards
	void resetACC(); //resets the acdc boards 

    class ConfigParams
    {
    public:
        ConfigParams();

        bool rawMode;
        int eventNumber;
        int triggerMode;
        unsigned int boardMask;
        std::string label;
        bool reset;
        int accTrigPolarity;
        int validationStart;
        int validationWindow;

        int coincidentTrigMask;
        int coincidentTrigDelay[8];
        int coincidentTrigStretch[8];
    } params_;

  private:
	/*------------------------------------------------------------------------------------*/
	/*---------------------------------Load neccessary classes----------------------------*/
	std::vector<ACDC> acdcs; //a vector of active acdc boards. 

	//----------all neccessary global variables
	int trigMode; //var: decides the triggermode
	int metaSwitch = 0;
	unsigned int validation_start;
	unsigned int validation_window; //var: validation window for some triggermodes
	unsigned int PPSRatio;
	int PPSBeamMultiplexer;
	int nEvtsMax;

	static void got_signal(int);
	void sendJCPLLSPIWord(unsigned int word, unsigned int boardMask = 0xff, bool verbose = false);
	std::string runNumber_;
};
}  // namespace ots

#endif
