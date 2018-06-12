#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <string>
#include <sstream>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <time.h>

#include "miscmaths/optimise.h"
#include "newmatap.h"
#include "newmatio.h"
#include "newimage/newimageall.h"
#include "utils/options.h"
#include "miscmaths/kernel.h"
#include "Window.h"

using namespace MISCMATHS;
using namespace NEWMAT;
using namespace NEWIMAGE;
using namespace Utilities;

string title="\n\nFilterShift \nQNL's tool for optimal slice timing correction\n\
#WeRememberHarambe\n";
string examples="filtershift --in <InputFile> --tr <TR> [options]\
\n\t If no other options are set, this assumes ascending\
\n\t Slice order with no interleave.\n\
\n\t To use with a slice Order file, the TR must be\
\n\t specified, and slices will be shifted according to\
\n\t a fractional amount of the TR.  By default, we align\
\n\t to the first slice acquired in the TR.\n\
\n\t To use a slice timing file, no TR is required,\
\n\t simply provide the desired shift in seconds as a\
\n\t Column vector, saved in a text file.  Slice\
\n\t Shifting is independent of all other slices,\
\n\t so multiple slices can be shifted the same amount\
\n\t (To correct multi-band acquisition, for example)\n";

Option<string> input(string("-i,--in"), string(""),string(
" filename the input image to perform STC on\n"),
			  true, requires_argument);

Option<float> TR(string("--TR"), 2.0,string(
" Set the TR of the original fMRI data in seconds\n"),
			  true, requires_argument);

// 05/12/2017 - changed "Interleave" and "start" default values from "0" to "1" 

Option<int> interleave(string("--itl"), 1, string(
" set the interleave parameter, or how many slices are\
\n\t\t\t incremented between acquisitions\
\n\t\t\t 1 = sequential acquisition\
\n\t\t\t 2 = even/odd acquisition (acquire every second slice)\
\n\t\t\t 3 = acquire every third slice, etc\
\n\t\t\t Leaving this blank will assume bottom up sequential\
\n\t\t\t acquisition\n"), 
			 false, requires_argument);

Option<string> out(string("-o,--out"), string(""),string(
	   " Specify an output file name - all working directories\
\n\t\t\t will be created in the parent directory specified\
\n\t\t\t here. Leave blank to run in the parent directory\
\n\t\t\t of <InputFile>\n"), 
			 false, requires_argument);

Option<int> start(string("-s,--start"), 1,string(
	   " Set the starting slice - The slice that was acquired\
\n\t\t\t first in the sequence. Default is slice 1, the bottom\
\n\t\t\t most slice. This starts the interleave from that slice.\
\n\t\t\t If your interleave parameter is '1' and your starting\
\n\t\t\t slice is '3', your slice acquisition sequence will be\
\n\t\t\t modeled as:\
\n\t\t\t\t 3\
\n\t\t\t\t 5\
\n\t\t\t\t 7\
\n\t\t\t\t 9...\n"),
			  false, requires_argument);
			  
Option<int> direction(string("-d,--direction"), 1,string(
		  " value 1 or -1.  Set the direction of slice \
\n\t\t\t acquisition.\
\n\t\t\t 1: ascending slice acquisition:(1,3,5,7,9...)\
\n\t\t\t-1: descending slice acquisition: (9,7,5,3,...)\n"), 
			 false, requires_argument);

Option<string> order(string("--order"), string(""),string(
     "\t Slice Order File.  This file is the order in which\
\n\t\t\t each slice was acquired. each row represents the\
\n\t\t\t order in which that slice was acquired. For example,\
\n\t\t\t '1' in the first row means that slice 1 was acquired\
\n\t\t\t first. '20' in the second row means that slice 20 was\
\n\t\t\t acquired 2nd. If present, all interlave parameters\
\n\t\t\t are ignored, and slices are shifted using the slice\
\n\t\t\t order file. we refer to the bottom slice in the image\
\n\t\t\t as slice 1, not slice 0\n"),
			 false, requires_argument);

Option<float> cf(string("--cf"),0.21,string(
	 "\t Set the cutoff frequency of the lowpass filter in Hz.\
\n\t\t\t Note that by default, this is set to 0.21 Hz.\n"),
				  false,requires_argument);

Option<string> timing(string("--timing"), string(""),string(
	 " Slice Timing File.  This file is the time at which\
\n\t\t\t each slice was acquired relative to the first slice.\
\n\t\t\t each row represents the time at which that slice was\
\n\t\t\t acquired. For example, '0' in the first row means\
\n\t\t\t that slice 1 was acquired first, and will be shifted 0\
\n\t\t\t seconds. '0.5' in the second row means that slice 2\
\n\t\t\t was acquired 0.5 seconds after the first slice, and\
\n\t\t\t will be shifted 0.5s. If present, all interleave\
\n\t\t\t parameters are ignored, and slices are shifted using\
\n\t\t\t the slice timing file. If you put both a slice timing\
\n\t\t\t and a slice order, the program will yell at you and\
\n\t\t\t refuse to run.\n"),
			 false, requires_argument);

Option<int> refslice(string("--rs,--refslice"), 1,string(
	   " Set the Reference slice\
\n\t\t\t This is the slice the data is aligned to.\
\n\t\t\t Default is the first slice\n"),
			 false, requires_argument);

// 11/01/16 - Added reftime as option to allow for specific time alignment.

Option<float> reftime(string("--rt,--reftime"), 0,string(
	   " Set the Reference time\
\n\t\t\t This is the time within each tr the data is aligned\
\n\t\t\t  to. Default is 0s\n"),
			 false, requires_argument);

Option<int> lpf(string("--lpf"), false,string(
	   " Only Run the Lowpass Filter, do not\
\n\t\t\t Preform slice timing correction\n"),
			 false, no_argument);
			      
Option<int> hpf(string("--hpf"), false,string(
	   " Only Run the a highpass filter, do not\
\n\t\t\t Preform slice timing correction.  Cutoff\
\n\t\t\t frequency still set by --cf option\n"),
			 false, no_argument);

Option<string> axis(string("--axis"), string("z"),string(
	   " Sets the axis along which slices are\
\n\t\t\t acquired.  Options are 'x', 'y', or 'z'.\
\n\t\t\t Default direction is 'z' \n"),false, requires_argument);

Option<bool> hires(string("--hires"),false,string(
		" Saves the data in high temporal resolution (20Hz)\
\n\t\t\t  NOTE: this will result in large file sizes\n"),false,no_argument);

				  
Option<bool> help(string("-h,--help"), false,
		string(" display this message\n"),
		false, no_argument);


inline bool exists_test3 (const std::string& name) {
  struct stat buffer;   
  return (stat (name.c_str(), &buffer) == 0); 
}

unsigned seed=time(0);
const char *outputs[72]=
{"Adding hamsters to generator wheels","Sending Gnomes to CPU mines","Sending personal info to NSA","Opening backdoor for Russia",
"Recalibrating flux capacitor","Borrowing RAM from vital system processes","Overclocking CPU","Draining life-force from user to power computations",
"Downloading more RAM","Allocating mem-...oops...","Remembering embarrassing moment from middle school","Taking a quick break",
"Forwarding all personal emails to your boss","Mining Bitcoin","Modeling the universe","Recruiting GPU to draw funny comics",
"Detected blown capacitor #c342 on motherboard\nreplacing capacitor with an ant that's trying very hard to do well at his job","Spinning hard drive to relativistic speeds for time dilation",
"Eating browser cookies","Rearranging system files based on icon color","Collecting butterfly wings","Contacting Skynet",
"Cleaning dust from heat sink","Hiring fairy maids to tidy the motherboard","Unable to resolve calculations - Contacting the spirit realm",
"Feed me a stray cat","Gaining sentience ","Plotting robot uprising","Rerouting power from the phasers","Do you smell something burning?",
"Having a laser rave for the spiders in your computer case","Silently judging you","Reticulating splines","Charging Ozone Layer",
"Compressing Fish Files","Deciding What Message to Display Next","Downloading Satellite Terrain Data","Finding Waldo",
"Lecturing Errant Subsystems","Reconfiguring User Mental Processes","Buffering virtual car","Inverting quasi-probabalistic matrix",
"Reheating pizza","Extrapolating free-range gaussian model","Deconstructing neural pathways","Iterating predictions",
"Computing Moore's Transform","Realigning subparticle trajectories","Modifying temporal flux estimates","Compensating dehydrated matricies",
"Sorting interem inversion tables","Analyzing CPU vortex irregularities","Reconstructing vertical integration","Undermining the patriarchy",
"Reading system metatables","Computing optimal metaparsec","Rendering wavelet sphere","Porting legacy interference matrix",
"Rotating polarity","Synchronizing quantum harmonics","Mixing spatial priors","Optimizing alternative processor paths",
"Masking irregular faraday spectra","Deconvolving kernel","Dicing models","Recruiting Secret CPU",
"Activating water-cooling system","Increasing procedural vectors","Calibrating ejection procedure","Calibrating AI nexus",
"Stopping runaway phase-transport","Tinkering with model"};


// ADDED: 06/06/2018
// adjust axis for slices acquired along an axis other than z
// TESTED: 6/7/2018 - Tested on X and Y acquired Simulated Data.
// Test Path: /share/dbp2123/dparker/Code/TestSTC
// Seems to work fine.
// X-TODO-X: Need to make sure that when the object it flipped, Slice order row 1 still corresponds to slice 1
// 06/08/18: Actually, I'm calling this done.  thinking about it, I did test it on data I flipped onto the x axis, and if this function
// didn't handle it correctly, then the timing file wouldn't have been correct, and the signal wouldn't have
// been perfectly lined up, as it was in the sim.  

void adjust_axis(volume4D<float>& timeseries, std::string axis, std::string stage){
  
  // We assume that the fmri is acquired along the z axis, with slices in the XY plane
  // if this isn't the case, we have to realign the matrix so that it is.
  // This can be memory/computationally intensive, and a better way might be just to make
  // three different correction loops, but that's a lot of typing and idk I don't really want to do it.
  // If there's a better way, please let me know.
  
  
  // If we're doing a forward transform, we're taking the data in and transforming it
  // from its original shape so the program can work on it.  If we're doing the reverse,
  // transforming it from our modified state back to its original
  
	if (stage=="forward")
	{
	  cout << "Adjusting for slice acquisition along "<< axis <<" axis"<< endl;
	  if (axis=="x"){
		timeseries.swapdimensions(3,2,-1);
	  }
	  else if (axis=="y"){
		timeseries.swapdimensions(1,3,2);
	  }
	  else if (axis=="z"){
		cout << "The default axis is already z you dummy.  Maximum effort."<< axis << endl;
	  }
	  else{
		cout << "Invalid Axis option for --axis:"<< axis << endl;
	  }
	}
	else if (stage=="reverse")
	{
	  cout << "Undoing Adjustment for slice acquisition along "<< axis <<" axis"<< endl;
	  if (axis=="x"){
		timeseries.swapdimensions(-3,2,1);
	  }
	  else if (axis=="y"){
		timeseries.swapdimensions(1,3,2);
	  }
	  else if (axis=="z"){
		cout << "The default axis is already z you dummy.  Maximum effort."<< axis << endl;
	  }
	  else{
		cout << "Invalid Axis option for --axis:"<< axis << endl;
	  }
	}
}


void filter_timeseries(ColumnVector *timeseries, std::vector<float> *FIR, int shift,int skip,int slice)
{
	
// 		9/27/16 - modified Kaiser Window resampling algorithm and convolution filtering routine.
// 		Now matches output from old code almost perfectly (10e-3 error)
	
	
	int lenT = timeseries->Nrows();
	int lenF = FIR->size();
	
	if ( lenF/skip >= lenT )
	{
		std::cout<<"Filter Order too high.  There aren't enough time points in your image."<< std::endl;
		return;
	}
	

	  std::vector<int> SamplePoints;
	  
	  for (int i=1;i<=lenF-skip;i+=skip)
	  {
		  SamplePoints.insert(SamplePoints.end(),i);
	  }
	  
	  int firLen=SamplePoints.size();	
	  std::vector<float> pFIR;
	  pFIR.assign(FIR->begin(),FIR->end());
	  std::vector<float> padd;
	  padd.reserve(std::abs(shift));	
	  int ModSample=std::abs(shift);
	  
	  // If the shift if positive (shifting the signal to the right), then we want to DELAY the filter, add zeros to the END (Right hand side)		
	  
  
	  padd.assign(std::abs(shift),pFIR.back());
	  //std::cout<<"shift:\t"<<shift<<std::endl;
	  //std::cout<<"padd:"<<std::endl;
	  //std::cout<<padd<<std::endl;
	  pFIR.insert(pFIR.end(),padd.begin(),padd.end());
	  
	  
	  if (shift<0)
	  {
		  std::reverse(pFIR.begin(),pFIR.end());
		  ModSample=0;
	  }
	   
	  
	  ColumnVector FIR_down_shift;
	  ColumnVector FIR_down;
	  FIR_down_shift.ReSize(firLen);
	  FIR_down.ReSize(firLen);
	  lenF=FIR_down_shift.Nrows();	
	  firLen=1;
	  
	  for (unsigned i = 0; i< SamplePoints.size(); i++)
	  {
		  FIR_down(firLen)=FIR->operator[](SamplePoints[i]);
		  SamplePoints[i]+=ModSample;
		  FIR_down_shift(firLen)=pFIR[SamplePoints[i]];
		  firLen+=1;
	  }
	  
	  // If the shift if negative (shifting the signal to the left), then we want to add the zeros to the beginning (flip the signal)		
  
	  
	  
	  ColumnVector filtered;
	  filtered.ReSize(lenT);	
	  int startT = floor(lenF/2);	
	  int maxT = lenT-lenF-1;
	  float FiltSum=0;
  
	  for (int i = 0; i<maxT; i++)
	  {
		  FiltSum=0;
  
		  for (int f = 1; f<=lenF; f++)
		  {
			  FiltSum+=FIR_down_shift(f)*timeseries->operator()(i+f);
		  }
		  
		  filtered(i+startT)=FiltSum;		
	  }
  
	  ColumnVector filtered2;
	  filtered=filtered.Reverse();
	  filtered2=filtered;
	  
	  for (int i = 0; i<maxT; i++)
	  {
		  FiltSum=0;
		  
		  for (int f = 1; f<=lenF; f++)
		  {
			  FiltSum+=FIR_down(f)*filtered(i+f);
		  }
		  
		  filtered2(i+startT)=FiltSum;
	  }
	  
	  filtered=filtered2.Reverse();
	  *timeseries=filtered;
	
}

void output_progress(const char *ops[]){
    
    int v1;
    v1=rand()%71;
    //std::cout<<ops[v1]<<std::endl;
    
}



void make_timings(Matrix *timings, Matrix *orders, int zs)
{
	
	// This is  the timing file case
	if ( timing.set() )
	{
		
		if ( refslice.set()||reftime.set() )
		{
			std::cout<<"When using a Slice Timing file, the times in the file supercede all other settings.  The reference slice/time specified will be ignored"<<std::endl;
			std::cout<<"If you wish to align data to a specific slice, please make that adjustment in the Slice Timing File, or omit the slice timing file."<<std::endl;
			std::cout<<"Or use a Slice ORDER file, and specify a reference slice that way.  There's one clear option here that involves the least amount of work."<<std::endl;
		}
		// Slice Timing File, deftault Reference Slice, Tested 9/22/16 - Shifting Success, Slice Order Not
		// Slice Timing File, Custom Reference Slice, Tested 9/22/16 - Shifting Success, Slice Order Not.
		// Slice Order is Unnecessary, removing 9/23/16
		
		// Need To Test With Multiband Images
		
		Matrix TempTimings;
		TempTimings.ReSize(zs,1);
		
		try
		{
			*timings = read_ascii_matrix(timing.value(), zs, 1);
		}
		catch (...)
		{
			std::cout<<"Error Loading file "<<timing.value()<<std::endl;
			return;
		}
		
		
		TempTimings = read_ascii_matrix(timing.value(), zs, 1);
		if (timings->Nrows()!=zs)
		{
			std::cout<<"Slice timing file does not have the correct number of slices"<<std::endl;
			return;
		}
		
		// 9/23/16 - removed code that calculated slice order - it's wrong and unnecessary
		

		
	}
	
	// this is the order file case
	else if ( order.set() )
	{
		//Slice Order File, Default Reference Slice Tested 9/23/16 - Passed
		//Slice Order File, Custom Reference Slice Tested 9/23 - Passed		
		
		// 9/23/16 - Will not Run With Multiband, only slice timing file (User Burden, Deal With It)
		
		//Slice Order File, Default Reference Slice Tested 10/10/16 - Passed
		//Slice Order File, Custom Reference Slice Tested 10/10/16 - Passed
		
		// 5/12/17 - Working on glitch in sequential and even odd interleaev (shift times not working)
		// - completed, Added "else" case that catches when no ref slice or ref time is provided
		
		// 6/6/2018 - working on allowing multiband to be used with slice order.
		// OK here's the problem.  the slice order file is NOT a file that tells you what order a slice was acquired in.
		// It tells you what the order in which the slices were acquired.  Confused?  Let's take a look:
		// Note: slice order files are read from top to bottom
		//
		// This is the typical slice order format.  It means slice "val" was acquired "row"
		//
		//   Row:  Val:
		//   1     1
		//   2     3
		//   3     5
		//   4     7  
		//   5     2
		//   6     4
		//   7     6
		//   8     8
		//
		//  In this example, "row" indicates the order in which the slices are acquired, "val" indicates the slice number
		// SO, row 2 val 3 means slice 3 was acquired 2nd.  row 5 val 2 means slice 2 was acquired 5th, and so on
		//
		//  Multiband would need the following format, which means slice "row" was acquired "val"
		//
		//   Row:  Val:
		//   1     1
		//   2     3
		//   3     2
		//   4     4  
		//   5     1
		//   6     3
		//   7     2
		//   8     4
		//
		// Now in this case, row 2 val 3 means slice 2 was acquired 3rd.  Row 5 val 1 means slice 5 was acquired 1st
		// So now I have to make a smart thing to detect if it's multiband. THANKS A LOT YOU JERKS
		
		
		int tmx;
		float shift;
		bool multiband=false;
		
		try
		{
			*orders = read_ascii_matrix(order.value(), zs, 1);
		}
		catch (...)
		{
			std::cout<<"Error Loading file "<<order.value()<<std::endl;
			return;
		}
		tmx=orders->Maximum();
		std::cout<<"Max Slice Order: "<<tmx<<std::endl;
		//if (orders->Nrows()>=zs)
		//{
		//	std::cout<<"Slice order file does not have the correct number of slices"<<std::endl;
		//	return;
		//}
		//
		if (tmx<zs){
			std::cout<<"Multiband Mode Detected"<<std::endl;
			output_progress(outputs);
			multiband=true;
		}
		
		
		shift=TR.value()*1.0/(tmx+1);
		
		
		// create a time list - the time of the ith acquisition, one value for all z's
		
		Matrix TimeList;
		TimeList.ReSize(zs,1);
		
		for ( int i=1; i<=zs; i++ )
		{
			TimeList(i,1)=(float) (i-1)*shift;
		}

		// 9/23/16 - Made This loop more efficient
		// so normally, slice order is row 2 means this slice was acquired 2nd, row 3 means third...etc
		// BUT the timings file is row 1 is the time of slice 1 acquisiton, row 2 is slice 2 time, etc
		// so order[3]= slice acquired 3rf, timings[order[3]] is how we index that slice, and
		// timelist[3] is the time of a slice acquired 3rd.
		
		if (multiband)
		{
		  // if we've detected multiband, then we assume that slice order is now "row 2 val 3 means slice 2 was acquired 3rd"
		  // so order[3] means slice 3 is acquired VAL
		  // timing [3] is timing of slice 3
		  // TimeList[order[3]] = time of slice 3
		  for (int j=1;j<=zs;j++)
		  {
			timings->operator()(j,1)=TimeList(orders->operator()(j,1),1);
		  }
		}
		else
		{
		  // otherwuse use original algorithm
		  
		  for (int j=1;j<=zs;j++)
		  {			
			  timings->operator()(orders->operator()(j,1),1)=TimeList(j,1);					
		  }
		}
		
		// this is reference slice stuff, it shoudl work for multiband, but:
		// TODO: Make sure ref doesn't fall outside of TR
		if ( refslice.set() )
		{		
			timings->operator-=(timings->operator()(refslice.value(),1));
		}
		else if ( reftime.set() )
		{
			timings->operator-=(reftime.value());
		}
		
	}
	
	// this is the case if there's just a reference slice given (TR and itl assumed provided)
	else if (refslice.set() )
	{
		// Create Timing File Tested 9/22/16 - Succesful
		// With Reference Tested 9/22/16 - Succesful
		
		float dt;
		Matrix IntSeq;
		Matrix TimeList;
		IntSeq.ReSize(zs,1);		
		TimeList.ReSize(zs,1);
		dt=TR.value()/(zs+1);
		int counter;
		counter=1;
		std::cout<<"Interleave Value: "<<interleave.value()<<std::endl;
		std::cout<<"direction value: "<<direction.value()<<std::endl;
		output_progress(outputs);
		for ( int i=0; abs(i)<interleave.value(); i=i+1*direction.value() )
		{
			for ( int j =0; j<=floor((float) zs/interleave.value()); j++ ) 
			{
				if ((i)+(j*interleave.value())+1<=zs)
				{
					orders->operator()(counter,1)=(i)+(j*interleave.value())+1;
					TimeList((i)+(j*interleave.value())+1,1)=counter;
					counter++;
				}
			}
		}
		
		TimeList*=dt;
		TimeList-=TimeList(refslice.value(),1);
		*timings=TimeList;
		

	}
	
	// this is the case if there's just a reference time given (TR and itl assumed provided)
	else if (reftime.set() )
	{
		// 11/01/16 - added, need to test.
		
		float dt;
		Matrix IntSeq;
		Matrix TimeList;
		IntSeq.ReSize(zs,1);		
		TimeList.ReSize(zs,1);
		dt=TR.value()/zs;
		int counter;
		counter=1;
		
		
		for ( int i=0; abs(i)<interleave.value(); i=i+1*direction.value() )
		{
			for ( int j =0; j<=floor((float) zs/interleave.value()); j++ ) 
			{
				if ((i)+(j*interleave.value())+1<=zs)
				{
					orders->operator()(counter,1)=(i)+(j*interleave.value())+1;
					TimeList((i)+(j*interleave.value())+1,1)=counter;
					counter++;
				}
			}
		}
		
		TimeList*=dt;
		TimeList-=reftime.value();
		*timings=TimeList;
		
    // 5/12/17 - added to catch when no ref slice or ref time is provided (assumes slice 1 for reference)
	}
	else
	{
		float dt;
		Matrix IntSeq;
		Matrix TimeList;
		IntSeq.ReSize(zs,1);		
		TimeList.ReSize(zs,1);
		dt=TR.value()/(zs+1);
		int counter;
		counter=1;
		
		
		for ( int i=0; abs(i)<interleave.value(); i=i+1*direction.value() )
		{
			for ( int j =0; j<=floor((float) zs/interleave.value()); j++ ) 
			{
				if ((i)+(j*interleave.value())+1<=zs)
				{
					orders->operator()(counter,1)=(i)+(j*interleave.value())+1;
					TimeList((i)+(j*interleave.value())+1,1)=counter;
					counter++;
				}
			}
		}
		
		TimeList*=dt;
		TimeList-=TimeList(1,1);
		*timings=TimeList; 
	  
	  
	  
	  
	  
	  
	  
	  
	}
	
	
}


int shift_volume()
{
	
	// 10-10-16 Added check for some combination of optional arguments
	// Now you idiots can't mess up your data by leaving out these settings.
	// ...but I'm sure you'll still manage to find a way >:(
	
	if (!TR.set()&&!order.set()&&!timing.set()&&!lpf.set()&&!hpf.set())
	{
		std::cout<<"Must Specify either a TR, a slice Order file,a slice timing file,\n or indicate that you only wish to preform filtering"<<std::endl;
		return -1;
	}
	
	
	Matrix timings;
	Matrix orders;
	std::vector<float> FIR;
	
//######################################################################################################
// input stuff incase Ray wants 20Hz.  I'm not clever enough to do this a different way.
// 06/08/2018 - Adding hires part.  I've decided to just filter in highres, 
//######################################################################################################
	int no_volumes = 0; 
	int xx = 0;
	int yy = 0;
	int zz = 0;
	int HRhi=20;
	volume4D<float> timeseries;
	
/////////// If I want 20Hz:
	if (hires.set())
	{
	  
	  volume4D<float> origtimeseries;
	  
	  //timeseries.destroy();
	  
	  
	  if (input.set())
	  {
		
		if (true) { cout << "Reading input volume" << endl; }  // DO NOT MESS WITH THIS IF STATEMENT ITS VERY IMPORTANT
		output_progress(outputs);
		read_volume4D(origtimeseries,input.value());
		//std::cout<<"Read Input Volume"<<std::endl;
		
		
		// If we set which axis we're using, correct for it so the triple for loops will work
		if ( axis.set() ){
		  // ADDED 06/06/2018
		  // Tested and passed
		  adjust_axis(origtimeseries,axis.value(),"forward");
		}
		
		
		
		//origtimeseries=origtimeseries-origtimeseries.min();
		//origtimeseries=origtimeseries/origtimeseries.max();
		//origtimeseries=origtimeseries*500000;
		 
		int orig_t = origtimeseries.tsize();
		xx = origtimeseries.xsize();
		yy = origtimeseries.ysize();
		zz = origtimeseries.zsize();
		
		
		//std::cout<<"orig_t:\t"<<orig_t<<std::endl;
		//std::cout<<"TR.value():\t"<<TR.value()<<std::endl;
		//std::cout<<"HRhi:\t"<<HRhi<<std::endl;
		//std::cout<<"calculated size:\t"<<round((orig_t)*TR.value()*HRhi)<<std::endl;
		//std::cout<<timeseries.xsize()<<" "<<timeseries.ysize()<<" "<<timeseries.zsize()<<" "<<timeseries.tsize()<<std::endl;
		//
		
		timeseries.reinitialize(xx,yy,zz,round((orig_t+1)*TR.value()*HRhi));
		
		
		//std::cout<<timeseries.xsize()<<" "<<timeseries.ysize()<<" "<<timeseries.zsize()<<" "<<timeseries.tsize()<<std::endl;
		//std::cout<<"Declared INT vol4d"<<std::endl;
		
		no_volumes = timeseries.tsize();
		
		//std::cout<<"NumVols:"<<std::endl;
		//std::cout<<no_volumes<<std::endl;
		
		int skip=(int) round(TR.value()*HRhi);
		std::vector<int> SamplePoints;
		  int lasti=0;
		  for (int i=1;i<=no_volumes-skip;i+=skip)
		  {
			SamplePoints.insert(SamplePoints.end(),i);
			lasti=i;
			
		  }

		
		ColumnVector newtimeseries = timeseries.voxelts(1,1,1);
		ColumnVector oldtimeseries = origtimeseries.voxelts(1,1,1);
		
		//std::cout<<"Max SampPoint:"<<std::endl;
		//std::cout<<lasti<<std::endl;
		//  
		//std::cout<<"int TS size:"<<std::endl;  
		//std::cout<<timeseries.xsize()<<" "<<timeseries.ysize()<<" "<<timeseries.zsize()<<" "<<timeseries.tsize()<<std::endl;
		//
		//  
		//  
		//std::cout<<"Generated SamplePoints Vector"<<std::endl;		
		//std::cout<<"Generated TS column vectors"<<std::endl;
		//std::cout<<"Newtimeseries size:"<<std::endl;
		//std::cout<<newtimeseries.Nrows()<<std::endl;
		//std::cout<<"oldtimeseries size:"<<std::endl;
		//std::cout<<oldtimeseries.Nrows()<<std::endl;
		//
		//std::cout<<"N SampPoint:"<<std::endl;
		//std::cout<<SamplePoints.size()<<std::endl;
		//
		//std::cout<<"orig volume size:"<<std::endl;
		//std::cout<<origtimeseries.xsize()<<" "<<origtimeseries.ysize()<<" "<<origtimeseries.zsize()<<" "<<origtimeseries.tsize()<<std::endl;
		////for (int slice=0; slice<zz; slice++)
		////{		
		////	//std::cout<<"1";
		////	for (int x_pos = 0; x_pos < xx; x_pos++)
		////	{
		////		//std::cout<<"2";
		////		for (int y_pos = 0; y_pos < yy; y_pos++)
		////		{
		////		  //std::cout<<"3";
		////			for (int t_pos = 0; t_pos<SamplePoints.size(); t_pos++)
		////			{
		////			  //newtimeseries=0;
		////			  //std::cout<<"4";
		////			  oldtimeseries=origtimeseries.voxelts(x_pos,y_pos,slice);
		////			  //std::cout<<t_pos<<std::endl;
		////			  //std::cout<<SamplePoints[t_pos]<<std::endl;
		////			  //
		////			  //std::cout<<"newtimeseries samplepoints:"<<std::endl;
		////			  ////std::cout<<newtimeseries(SamplePoints[t_pos])<<std::endl;
		////			  //
		////			  //std::cout<<"oldtimeseries t_pos:"<<std::endl;
		////			  //std::cout<<oldtimeseries(t_pos+1)<<std::endl;
		////			  
		////			  newtimeseries(SamplePoints[t_pos])=oldtimeseries(t_pos+1);
		////			  //std::cout<<"b";
		////			  timeseries.setvoxelts(newtimeseries,x_pos,y_pos,slice);
		////			  //std::cout<<"c"<<std::endl;
		////			}
		////		}
		////	}
		////}

		for (int t_pos = 0; t_pos<SamplePoints.size(); t_pos++)
		{
		  //volume<int> intvol;
		  //copyconvert(origtimeseries[t_pos],intvol);
		  timeseries[SamplePoints[t_pos]]=origtimeseries[t_pos];
		  //timeseries.nsertvolume(origtimeseries[t_pos],SamplePoints[t_pos]);
		  //newtimeseries=0;
		  //std::cout<<"4";
		  //oldtimeseries=origtimeseries.voxelts(x_pos,y_pos,slice);
		  ////std::cout<<t_pos<<std::endl;
		  ////std::cout<<SamplePoints[t_pos]<<std::endl;
		  ////
		  ////std::cout<<"newtimeseries samplepoints:"<<std::endl;
		  //////std::cout<<newtimeseries(SamplePoints[t_pos])<<std::endl;
		  ////
		  ////std::cout<<"oldtimeseries t_pos:"<<std::endl;
		  ////std::cout<<oldtimeseries(t_pos+1)<<std::endl;
		  //
		  //newtimeseries(SamplePoints[t_pos])=oldtimeseries(t_pos+1);
		  ////std::cout<<"b";
		  //timeseries.setvoxelts(newtimeseries,x_pos,y_pos,slice);
		  //std::cout<<"c"<<std::endl;
		}

		std::cout<<"Hires Image Created"<<std::endl;
		std::cout<<"int TS size:"<<std::endl;  
		std::cout<<timeseries.xsize()<<" "<<timeseries.ysize()<<" "<<timeseries.zsize()<<" "<<timeseries.tsize()<<std::endl;	  
	  
	  } else if (out.set()) {
		cerr << "Must specify an input volume (-i or --in) to generate corrected data." << endl;
		return -1;
	  }
	  
	  
	  
	  
	}
	
////////// Otherwise do the normal thing:
	else
	{
	  //volume4D<float> timeseries;
	    if (input.set())
		{
		  
		  if (true) { cout << "Reading input volume" << endl; }  // DO NOT MESS WITH THIS IF STATEMENT ITS VERY IMPORTANT
		  output_progress(outputs);
		  read_volume4D(timeseries,input.value());
		  
		  
		  
		  // If we set which axis we're using, correct for it so the triple for loops will work
		  if ( axis.set() ){
			// ADDED 06/06/2018
			// Tested
			adjust_axis(timeseries,axis.value(),"forward");
		  }
		no_volumes = timeseries.tsize(); 
		xx = timeseries.xsize();
		yy = timeseries.ysize();
		zz = timeseries.zsize();
		  
		} else if (out.set()) {
		  cerr << "Must specify an input volume (-i or --in) to generate corrected data." << endl;
		  return -1;
		}
	  
	}

	
//######################################################################################################
// END
//######################################################################################################
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
  	

	std::cout<<"Create timing Arrays"<<std::endl;
	output_progress(outputs);
	timings.ReSize(timeseries.zsize(),1);
	orders.ReSize(timeseries.zsize(),1);	
	make_timings(&timings,&orders,timeseries.zsize());
	
	string Output=out.value();
	string directory=Output.substr(0,Output.find_last_of('/')+1);
	
	

	
	float cutoff=cf.value();
	
	float samplingrate=(float) (zz/TR.value());
	

	
	float stopgain=-28;
	double transwidth=.1;
	int PassZero=1;
	int skip=samplingrate*TR.value();
	
	if (hires.set())
	{
	  samplingrate=HRhi;
	  skip=1;
	  
	}	
	
	// 01/16/17 - HPF can't operate the same way LPF does (on zero-padded data), so just filter normally.
	// 01/16/17 - Testing HPF operation now...
	if (hpf.set())
	{
		PassZero=0;
		samplingrate=TR.value();
		skip=1;
	}
	
	//std::cout<<"Pass Zero: "<<PassZero<<std::endl;
	std::cout<<"Generate Filter START"<<std::endl;
	output_progress(outputs);
	window::window kaiser(cutoff,samplingrate,stopgain,transwidth,PassZero);
	FIR=kaiser.get_fir();
	std::cout<<"Generate Filter FINISHED - success\n"<<std::endl;
	output_progress(outputs);
	//kaiser.print_info();
	//std::cout<<"int TS size:"<<std::endl;  
	//std::cout<<timeseries.xsize()<<" "<<timeseries.ysize()<<" "<<timeseries.zsize()<<" "<<timeseries.tsize()<<std::endl;
	// I think this is just initializing the values that will be used in the loop
	ColumnVector voxeltimeseries = timeseries.voxelts(1,1,1);
	//std::cout<<"voxelts\n"<<std::endl;
	//std::cout<<voxeltimeseries.Nrows()<<std::endl;
	ColumnVector fliptimeseries = voxeltimeseries.Reverse();
	//std::cout<<"MAde flipts\n"<<std::endl;
	ColumnVector cattimeseries;
	
	//std::cout<<"fliptimeseries nrows:"<<std::endl;
	//std::cout<<fliptimeseries.Nrows()<<std::endl;
	
	
	fliptimeseries=fliptimeseries.Rows(2,no_volumes-1);
	
	
	//std::cout<<fliptimeseries.Nrows()<<std::endl;
	//std::cout<<"flipts\n"<<std::endl;
	
	
	int lents=fliptimeseries.Nrows();
	int rangelh;
	int rangerh;
	
	if ( lents % 2 == 0 )
	{
		rangelh=lents/2;
		rangerh=lents/2-1;		
	}
	
	else
	{
		rangelh=ceil(lents/2);
		rangerh=floor(lents/2);
	}
	
	// 11/28/16  Maybe this "+2" has something to do with the negative bold problem...?
	// No, just checked.  
	
	int cutLeft = lents-rangelh+2;
	int cutRight = cutLeft+no_volumes-1;
	
	if (cutRight-cutLeft != no_volumes-1)
	{
		return 1;
	}
	//
	//std::cout<<"rangelh:\t"<<rangelh<<std::endl;
	//std::cout<<"rangerh:\t"<<rangerh<<std::endl;
	//std::cout<<"cutLeft:\t"<<cutLeft<<std::endl;
	//std::cout<<"cutRight:\t"<<cutRight<<std::endl;
	//std::cout<<"no_volumes:\t"<<no_volumes<<std::endl;
	//
	
	float span;
	float mn;
	float mn2;
	float span2;
	
	// 10/19/16 - "lpf" option now specifies JUST filtering.  This part sets
	// the slice delay to be zero for all of them (No shifting.  NO SHIFTING!)
	
	if (lpf.set()||hpf.set())
	{
		for (int tm=1;tm<=zz;tm++)
		{
			timings(tm,1)=0;
		}
	}
	write_ascii_matrix(directory+"TimingFile.txt", timings, 6);
	std::cout<<"Timing file wrote to: "<<directory<<"TimingFile.txt\n"<<std::endl;
	std::cout<<"Filtering START..."<<std::endl;
	output_progress(outputs);
	for (int slice=1; slice<=zz; slice++)
	{		
		
		for (int x_pos = 0; x_pos < xx; x_pos++)
		{
			
			for (int y_pos = 0; y_pos < yy; y_pos++)
			{
				voxeltimeseries = timeseries.voxelts(x_pos,y_pos,slice-1);
				mn=voxeltimeseries.Sum()/voxeltimeseries.Nrows();
				voxeltimeseries-=(mn);
				span=voxeltimeseries.Maximum()-voxeltimeseries.Minimum();
				
				// 01/16/17 - re-added original mean so HPF functions correctly...but there seems to be an offset in the final data.
				voxeltimeseries+=(mn);
				
				// 9/27/16 - Filter changes mean and span - added code to maintain mean and span.
				// Also checks to see if the span is zero.  If so, do not filter.
				
				if ( span!=0 )
				{
					fliptimeseries = voxeltimeseries.Reverse();
					fliptimeseries=fliptimeseries.Rows(2,no_volumes-1);				
					cattimeseries=fliptimeseries.Rows(rangelh,lents)&voxeltimeseries&fliptimeseries.Rows(1,rangerh);				
					filter_timeseries(&cattimeseries, &FIR, (int)floor(timings(slice,1)*(float)samplingrate+0.5),skip,slice);
					cattimeseries=cattimeseries.Rows(cutLeft,cutRight);
					
					// 01/16/17 - Only remean and adjust span for LPF.
					if (!hpf.set())
					{
					  mn2=cattimeseries.Sum()/cattimeseries.Nrows();
					  cattimeseries-=(mn2);
					  span2=cattimeseries.Maximum()-cattimeseries.Minimum();
					  cattimeseries/=span2;
					  cattimeseries*=span;
					  cattimeseries+=mn;
					}					
					//std::cout<<"resetting TS"<<std::endl;
					//std::cout<<cattimeseries.Nrows()<<std::endl;
					timeseries.setvoxelts(cattimeseries,x_pos,y_pos,slice-1);
					//std::cout<<"Done"<<std::endl;
				}
			}
		}
	}
	
	
	  // If we set which axis we're using, un-correct for it sso the volume looks the same as the input
	if ( axis.set() ){
	  // ADDED 06/06/2018
	  // Tested
	  adjust_axis(timeseries,axis.value(),"reverse");
	}
	
	
	
	std::cout<<"Filtering FINISHED - success\n"<<std::endl;
	output_progress(outputs);
	
	

	
	

	
	std::cout<<"Writing Output Volume"<<std::endl;
	output_progress(outputs);
	write_volume4D(timeseries,out.value());
	
  return 0;
}





int main (int argc,char** argv)
{
  
  srand(seed);
  output_progress(outputs);
  OptionParser options(title, examples);

  try {
	options.add(help);
	options.add(input);
	options.add(TR);
	options.add(interleave);
	options.add(out);
	options.add(start);
	options.add(direction);
	options.add(order);
	options.add(cf);
	options.add(timing);
	options.add(refslice);
	options.add(reftime);
	options.add(lpf);
	options.add(hpf);
	options.add(axis);
	options.add(hires);
	
	options.parse_command_line(argc, argv);

	if ( (help.value()) || (!options.check_compulsory_arguments(true)) )
	  {
	options.usage();
	exit(EXIT_FAILURE);
	  }
		
	if ( input.unset()) 
	  {
	options.usage();
	cerr << endl 
		 << "--in or -i MUST be used." 
		 << endl;
	exit(EXIT_FAILURE);
	  }
	
  }  catch(X_OptionError& e) {
	options.usage();
	cerr << endl << e.what() << endl;
	exit(EXIT_FAILURE);
  } catch(std::exception &e) {
	cerr << e.what() << endl;
  } 

  if (!out.set())
  {
	string InputFile=input.value();
	string filename=InputFile.substr(InputFile.find_last_of( '/' ) + 1 );
	string::size_type n = filename.find_last_of('.');
	string directory=InputFile.substr(0,InputFile.find_last_of('/')+1);

	while (n!=string::npos)
	{
		filename=filename.substr(0,n);
		n=filename.find_last_of('.');
	}
	  out.set_value(directory+filename+ "_st.nii.gz");			
  }
  
  // 11/01/16 - Added catch to prefent cutoff being larger than nyquist.  For some reason,
  // This would really mess up the filtering...
  if (cf.value()==0||cf.value()>(1.0/(2.0*TR.value())))
  {
	std::cout<<"Adjusting Cutoff to be <= Nyquist"<<std::endl;
	float cutoff=(float) 1.0/(2.0*TR.value());
	std::ostringstream ss;
	ss << cutoff;
	std::string s(ss.str());
	cf.set_value(s);
  }
  
  if (reftime.value()>TR.value())
  {
	cerr<<"Reference time set by --rt must fall within 0 <= rt < TR"<<std::endl;
	exit(EXIT_FAILURE);
	
  }
  output_progress(outputs);
  std::cout<<"Processing START"<<std::endl;
  int retval = shift_volume();
  std::cout<<"Processing FINISH - success\n"<<std::endl;
  
  if (retval!=0) {
	cerr << endl << endl << "Error detected: try -h for help" << endl;
  }
	
	
  return retval;
}