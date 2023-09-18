# Otsdaq Template

## Installation
This code is an extension of the otsdaq project: https://github.com/art-daq/otsdaq/wiki project and requires it to function.  To setup OTS-DAQ use the instructions listed here: https://github.com/art-daq/otsdaq/wiki/Instructions-for-Developers for installing the core OTS-DAQ packages.  *This must be done first!*  Once the OTSDAQ core dependencies are set up, the CMS Tracker interface code can be included.  

<pre>
cd $MRB_SOURCE # this is the 'srcs' directory that will be set in the course of setting up OTS-DAQ
git clone https://gitlab.cern.ch/otsdaq/otsdaq_template.git otsdaq_template
mrb uc 
</pre>

Copy a setup file in $OTSDAQ_HOME
<pre>
cd $OTSDAQ_HOME
cp $MRB_SOURCE/otsdaq_template/SetupFiles/Setup.sh .
</pre>

Once the package is checked out or if you are starting a new session, source the setup file,

<pre>
cd $OTSDAQ_HOME
source Setup.sh
</pre>
