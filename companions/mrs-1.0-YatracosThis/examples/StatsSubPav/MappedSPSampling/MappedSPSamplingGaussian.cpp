/*! \file
\brief MappedSPnode example for Gaussian objects main.
*/

/*
#include "RosenFobj2D.hpp"
#include "RosenFobj10D.hpp"
#include "GaussianFobj1D.hpp"
*/
#include "GaussianFobj2D.hpp"
/*
#include "GaussianFobj9D.hpp"
#include "GaussianFobj10D.hpp"
#include "GaussianFobj100D.hpp"
//#include "GaussianFobj1000D.hpp"
*/

#include "mappedspnode.hpp"
#include "realmappedspnode.hpp"
#include "mappedspnodevisitor_expand.hpp"

#include "histall.hpp"  // headers for the histograms
#include "dataprep.hpp" // headers for getting data
#include "MCMCGRtools.hpp" // tools to help 

#include <time.h>   // clock and time classes
#include <fstream>  // input and output streams
#include <sstream>  // to be able to manipulate strings as streams
#include <cassert> // for assertions
#include <stdexcept> // throwing exceptions
#include <functional> // mutliplies<>
#include <algorithm> // transform

#include <gsl/gsl_randist.h> // to use gsl_ran_discrete_preproc
#include <valarray> 
#include "toolz.hpp" //draw unif box

#include "auto_tools.hpp"

//#define NDEBUG // uncomment this to turn off assertion checking and all extra debugging

#ifndef NDEBUG // ie only define these if we have not defined NDEBUG for no debugging
//#define MYDEBUG_OUTPUT // extra console output etc for debugging - only use for small examples!
//#define MYDEBUG_CALCS // extra console output for calculations
//#define MYDEBUG // extra files for collations, averages and diffs to av as chains develop

//#define FORCEFAILINSERTION // debugging flag to force a failure during insertion of data

//#define FORCEFAILMCMCLOOP // debugging flag to force a failure during an MCMC loop

#endif

using namespace cxsc;
using namespace subpavings;
using namespace std;

/*! templatized function object for lexicographical sorting of vectors whose elements have total ordering
*/
template <class T>
class LexicoSorting
{
  public:
    bool operator() (const T& t1, const T& t2) const {
      return std::lexicographical_compare(&t1[0], &t1[t1.size()-1], &t2[0], &t2[t2.size()-1]);
      //return lexicographical_compare(t1.begin(), t1.end(), t2.begin(), t2.end());
    }
};

//==========Functions for MappedSPnode===================================//
/*! Function: output MappedSPnode to .txt file
*/
void output(string& filename,  const SPnode& node)
{
   // To generate a file output
   ofstream os(filename.c_str());         // Filename, c-string version
   if (os.is_open()) {
      node.leavesOutputTabs(os); // the output
      std::cout << "The output of the estimated function"
               << " has been written to " << filename << std::endl << std::endl;
         os.close();
      }
   else {
      std::cerr << "Error: could not open file named "
         << filename << std::endl << std::endl;
   }
}

/*! Function: iterate through the leaves and get weights and boxes
*/
void getAllWeights(RealMappedSPnode* thisNodePtr, vector<double>& WeightsVector,
							vector<interval>& WeightsInt)
{
	if (!(thisNodePtr->isEmpty()) && thisNodePtr->isLeaf()) { // this is a non-empty leaf
	    //get the weights
		 RangeCollectionClass<real> myContainer;
		 myContainer = thisNodePtr->getRangeCollection();
		 myContainer.getWeights(WeightsVector, WeightsInt, thisNodePtr->nodeVolume());
	}

	//recurse on the children
	if (thisNodePtr->hasLCwithBox()) {
		getAllWeights(thisNodePtr->getLeftChild(), WeightsVector, WeightsInt);
	}
   if (thisNodePtr->hasRCwithBox()) {
		getAllWeights(thisNodePtr->getRightChild(), WeightsVector, WeightsInt);
   }
}

/*! Function: iterate through the leaves and get heights and boxes
*/
void getHeightAndBox(RealMappedSPnode* thisNodePtr, vector<ivector>& BoxVector,
					 vector<real>& HeightsVector)
{
	if (!(thisNodePtr->isEmpty()) && thisNodePtr->isLeaf()) { // this is a non-empty leaf
		 //push back this box into the BoxVector
		 BoxVector.push_back(thisNodePtr->getBox());
		 
		 //get the heights
		 RangeCollectionClass<real> myContainer;
		 myContainer = thisNodePtr->getRangeCollection();
		 myContainer.getHeight(HeightsVector);
	}
  //recurse on the children
  if (thisNodePtr->hasLCwithBox()) {
		getHeightAndBox(thisNodePtr->getLeftChild(), BoxVector, HeightsVector);
	}
   if (thisNodePtr->hasRCwithBox()) {
		getHeightAndBox(thisNodePtr->getRightChild(), BoxVector, HeightsVector);
   }
}

/*! Function: normalize the heights
*/
void normHeights(RealMappedSPnode* thisNodePtr, double totalArea, 
					vector< RangeCollectionClass<real> >& heightNorm)
{
	if (!(thisNodePtr->isEmpty()) ) { // this is non-empty
		 RangeCollectionClass<real> myContainer;
		 myContainer = thisNodePtr->getRangeCollection();
		 real newHeight = myContainer.normNodeHeight(totalArea);
		 
		 RangeCollectionClass<real> height(newHeight);
		heightNorm.push_back(height);
	}
  //recurse on the children
  if (thisNodePtr->hasLCwithBox()) {
		normHeights(thisNodePtr->getLeftChild(), totalArea, heightNorm);
	}
   if (thisNodePtr->hasRCwithBox()) {
		normHeights(thisNodePtr->getRightChild(), totalArea, heightNorm);
   }
}
//=======================end of functions====================================//
AdaptiveHistogramCollator doMCMCGRAuto(size_t n, int d, size_t numHist,
				size_t maxLeaves, 
				int maxLoops, int samplesNeeded, int thinout, cxsc::real tol, 
				size_t minPoints, int dataSeed, double maxLeaf, size_t nL);

/*! Main
*/
int main(int argc, char* argv[])
{
	//========user-defined parameters====================//
	size_t n=atoi(argv[1]);  // number of datapoints to generate for each histogram
	int d = atoi(argv[2]); // dimensions
	size_t numHist = atoi(argv[3]); // number of repetitions for simulation purposes
	
	//	for generating samples from MappedSPnode 
	// ensure max leaves is < 1E6 or something reasonable
	size_t maxLeaves = atoi(argv[4]);
	
	// for the MCMC run
	int maxLoops = atoi(argv[5]); // maximum changes of state from initial state to try
	int samplesNeeded = atoi(argv[6]); // how many samples do we want (ie once chains have burned in)
	int thinout = atoi(argv[7]); // sample every thinout state, ie thinout-1 states between samples
	
	real tolerance = atof(argv[8]);
	cxsc::real tol(tolerance); //tolerance for automated burn in criteria
	
	size_t minPoints = atoi(argv[9]); // for MCMC rsplittable nodes
	 
	int dataSeed = atoi(argv[10]);
	
	double maxLeaf = atoi(argv[11]); // maximum leaves in hist2
	size_t nL = atoi(argv[12]); // maximum leaves in hist 1
	
	// should really do more checks on parameters, but just check thinout here
	if (thinout < 1 ) {
		throw std::invalid_argument("Invalid thinout argument");
	}

	//=====end of user-defined parameters==========================//

	try {
		AdaptiveHistogramCollator avg = doMCMCGRAuto(n, d, numHist, maxLeaves, 
				maxLoops, samplesNeeded, thinout, tol, minPoints, dataSeed, maxLeaf, nL);

		std::string samplesCollAverageFilename = "AveragedSamplesFromMCMCGRAuto.txt";
		outputFileStart(samplesCollAverageFilename);
	
		avg.outputToTxtTabs(samplesCollAverageFilename);
		
		return 0;
	}
	catch (std::runtime_error& e) {
		cout << "\nFailed doMCMCGRAuto: original error:\n" 
			<< std::string(e.what()) << "\n" << endl;
	}
} // end of main()

/*! Function: MCMC with automated burn-in procedure
*/
AdaptiveHistogramCollator doMCMCGRAuto(size_t n, int d, size_t numHist,
				size_t maxLeaves, 
				int maxLoops, int samplesNeeded, int thinout, cxsc::real tol, 
				size_t minPoints, int dataSeed, double maxLeaf, size_t nL)
{
	// use the cxsc manipulators for changing printing of cxsc::reals to console
	int prec = 15;
	cout << cxsc::SaveOpt;
	cout << cxsc::Variable;
	cout << cxsc::SetPrecision(prec+2, prec);

	// string formatting
	ofstream oss;         // ofstream object
   oss << scientific;  // set formatting for input to oss
   oss.precision(10);

	// set up a random number generator to draw from weighted boxes
	const gsl_rng_type * T;
	gsl_rng * r;

	//create a generator chosen by the environment variable GSL_RNG_TYPE
	gsl_rng_env_setup();
	T = gsl_rng_default;
	r = gsl_rng_alloc (T);
	//===========end of setting up preliminaries=======================//

	
	//=======generate actual gaussian data=====================//
		
		cout << "Generating actual Gaussian data: " << endl;
		const gsl_rng_type * T1;
		gsl_rng * r1;
		gsl_rng_env_setup();
		T1 = gsl_rng_default;
		r1 = gsl_rng_alloc (T1);
		gsl_rng_set(r1, dataSeed);
		RVecData actualData;
		for (size_t i = 0; i < n; i++) {
			rvector thisrv(d);
			for (size_t j = 1; j <= d; j++) {
				double z = gsl_ran_gaussian(r1, 1.0); // generate a normal r.v.
				thisrv[j] = _real(z);
			}
			//cout << thisrv << endl;
			actualData.push_back(thisrv);
		}
		//end of actual data runs
		
		ivector pavingBox;
		AdaptiveHistogram* tempHist = new AdaptiveHistogram;
		tempHist->insertFromRVec(actualData);
		pavingBox = tempHist->getSubPaving()->getBox();
		delete tempHist;
		
	//====Using MRP procedures to approximate the densities============// 
	// Function object
	//GaussianFobj1D realF;
	GaussianFobj2D realF;
	//GaussianFobj9D realF;
	//	Gaussian10D realF;
	//RosenFobj2D realF;
	//	RosenFobj10D realF;

	// Make a MappedSPnode object
	RealMappedSPnode nodeEst(pavingBox); 
   // Get the MRP approximated function using a priority queue
	MappedSPnodeVisitorExpand expander(realF, 0);
	vector<real> epsVec; 
	nodeEst.priorityAccept(expander, maxLeaves, epsVec);

	/*
 	string avgL1FileName = "Eps";
	avgL1FileName += ".txt";
	oss.open(avgL1FileName.c_str());
		for (size_t i = 0; i < epsVec.size(); i++) { 
	//		cout << epsVec[i] << endl;
			oss << epsVec[i] << "\n";
		}
		oss << flush;
		oss.close();
	*/

	cout << "Estimate function has " << nodeEst.getNumLeaves() << " leaf nodes." << endl;
	
	//=======================================================================//

	//==================Generate data from the MRP approx function============//
	// Get the weights of the boxes
	 cout << "Getting boxes and weights:" << endl;
    vector<ivector> BoxVector;
	 vector<real> HeightsVector;
	 RealMappedSPnode* nodePtr;
	 nodePtr = &nodeEst;
	 vector<double>* WeightsVectorPtr;
	 WeightsVectorPtr = new vector<double>;
	 vector<interval>* WeightsIntPtr;
	 WeightsIntPtr = new vector<interval>;
	 
	 // iterate through the leaf nodes to get boxes and heights and weights
	 getHeightAndBox(nodePtr, BoxVector, HeightsVector);
	 getAllWeights(nodePtr, *WeightsVectorPtr, *WeightsIntPtr);

	 // now put elements of WeightsVector into an array of doubles
	 size_t sizeWeight =(*WeightsVectorPtr).size();
	 //check that number of boxes < 10^6
	 if (sizeWeight > pow(10,7)) { 
			cerr << "Too many boxes (" << sizeWeight << ")." << endl;
			exit(1);
	 }

	 // normalize using heights
	 interval areaInt = interval(0);
	 // normalize the heights so that the function integrates to 1
	 for (size_t i = 0; i < sizeWeight; i++) {
		areaInt = areaInt + (*WeightsIntPtr)[i];
		//cout << (*WeightsVectorPtr)[i] << "\t" << (*WeightsIntPtr)[i] << endl;
	 }
	 cout << "Total area: " << mid(areaInt) << endl; 
	 
	 double totalArea = _double(mid(areaInt));
	 
	 // very important - normalize the heights in nodeEst
	 vector< RangeCollectionClass<real> >* heightNorm = new vector< RangeCollectionClass<real> >;
	 normHeights(nodePtr, totalArea, *heightNorm);
	 nodeEst.allocateRanges(*heightNorm, 0);
	 string filename = "EstFunctionAfterNormalized.txt";
	 output(filename, nodeEst);
	 
	 //need to check that the weights equal to 1
	 double densityCheck = 0.0;
    //convert vector to array
	 double WeightsArray[sizeWeight];
	 for (size_t i = 0; i < sizeWeight; i++) {
			WeightsArray[i] = (*WeightsVectorPtr)[i]/totalArea;
			densityCheck += (*WeightsVectorPtr)[i]/totalArea;
	 }
	 
	cout << "Total area after normalizing: " << densityCheck << endl;

	/*
	if ( (densityCheck != 1.0) ) {
		cout << densityCheck << endl; 
		cerr << "Function does not integrate to 1. Need to normalize." << endl; 
		exit(0); 
	}
	*/

	 //return to the system the memory that was pointed to by WeightsVectorPtr
	//  and heightNorm
	delete WeightsVectorPtr;
	delete heightNorm;

    //now get the lookup table returned from gsl_ran_discrete_preproc
	 //i.e. the box indices with their weights
	 gsl_ran_discrete_t* gslpdfstruct;
	 gslpdfstruct = gsl_ran_discrete_preproc(sizeWeight, WeightsArray);
	 //===================end of getting box weights=======================//

	//======Start of MCMC routine beginning with data generation============//
	for (size_t h = 0; h < numHist; h++) {
		ostringstream stmH;
		stmH << h;
		cout << "=========running simulation " << h << "===============" << endl;
		AdaptiveHistogramCollator* samplesAvg = new AdaptiveHistogramCollator; // our return object, if all goes well

		cout << h << endl;
		gsl_rng_set(r, h);


		//-------------generate data--------------------------------------//
		//now sample n data points from boxes given by the proposed indices
		cout << "Sample data points using weighted boxes:" << endl;
		RVecData* theData = new RVecData;   // a container for all the points generated
	  // make a simulated data set
		// data sampled as weighted-uniform-mixtures
		for (size_t i = 0; i < n; i++) {
			rvector thisrv(d);
			size_t proposedIndex = gsl_ran_discrete(r, gslpdfstruct);
			//int proposed_index = static_cast<int>(gsl_ran_discrete(r, gslpdfstruct));
			thisrv = DrawUnifBox(r, BoxVector[proposedIndex]);
			// put points generated into container
			theData->push_back(thisrv);
		}  // data  should be in theData
		
		cout << (*theData).size() << " points generated" << endl;

		/* optional
		// Output data
		string dataFileName = "Data";
		dataFileName += stmH.str();
		dataFileName += ".txt";
		oss.open(dataFileName.c_str());
		for (size_t i = 0; i < n; i++) { 
			for (size_t j = 1; j <= d; j++) {
			    //cout << (*theData)[i][j] << endl;
				oss << (*theData)[i][j] << "\t";
			}
			oss << "\n";
		}
		oss << flush;
		oss.close();
		*/
		//---------done generating data-------------------------------------//

		//start recording cpu time here
		clock_t start, end;
      double timing; 
		cout << "Min points is " << minPoints << endl;

		//================Gelman-Rubin method=========================//
		 // start by making the histograms that are the starting points for our chains
		 // Gelman and Carlin [1996] recommend ten or more chains if the target
		 // distribution is unimodal (Martinez and Martinze, 2000, p. 453)
		 // the starting points are chosen to be widely dispersed
	
		//------Starting histograms-------------------------------//
		cout << "Starting the chains " << endl;


//==================getting posterior-optimal histogram======================//
		//histogram 1
		cout << "Histogram 1" << endl;
		AdaptiveHistogram* HistFirst = new AdaptiveHistogram(pavingBox);
		//AdaptiveHistogram* myHistFirst = new AdaptiveHistogram(pavingBox);

		// put in the data in a 'pulse' with no splitting, ie into root box
		 bool successfulInsertionFirst = HistFirst->insertFromRVec(*theData);
		//bool successfulInsertionFirst = HistFirst->insertFromRVec(actualData);

		// PQ
		bool successfulPQSplitFirst = false;
		if (successfulInsertionFirst) {
			// set up function objects for a priority split
			CompCount compCount;
			// split until number of leaves is at most nL
			CritLeaves_GTE critVal(nL);
			// or split until each leaf node has at least minVal points
			//CritLargestCount_LTE critVal(minVal);
			size_t minPQPoints = 1; // minimum of one data point in each box
			size_t maxLeafNodes = pow(n,2); 

			// do the priority split
			//vector<real> Posterior;
			//LogCatalanPrior logPrior;
			successfulPQSplitFirst = HistFirst->prioritySplit(compCount,
			critVal, NOLOG, minPQPoints, maxLeafNodes); 
			//successfulPQSplitFirst = HistFirst->prioritySplitMCMC(compCount,
			//critVal, NOLOG, minPQPoints, maxLeafNodes, Posterior, logPrior); 
			//delete HistFirst;

/*			// now normalize the posteriors to get a discrete distribution
			vector<real>::iterator realIt;
			real totPos = 0;
			vector<real> NormPost;
			for (realIt = Posterior.begin(); realIt < Posterior.end(); realIt++) {
				//cout << *realIt << endl;
				totPos += *realIt;
			}
			totPos = abs(totPos);
			//find where the posterior is the highest
			size_t maxPosition = 0;
			real maxPosterior = -1000000;
			real check = 0; // need to make sure i check that the sum = 1
			for (size_t i = 0; i < Posterior.size(); i++) {
				NormPost.push_back(Posterior[i]/(totPos));
				if (Posterior[i]/totPos > maxPosterior) {
					maxPosterior = Posterior[i]/(totPos);
					maxPosition = i;
				}
			}

			// get the state with the highest posterior
			cout << "highest posterior at: " << maxPosition << endl;
			CritLeaves_GTE critValMax(maxPosition+1);
			
			// now get the posterior-optimal histogram
			successfulInsertionFirst = myHistFirst->insertFromRVec(actualData);
			//successfulInsertionFirst = myHistFirst->insertFromRVec(theData);
			successfulPQSplitFirst = myHistFirst->prioritySplitMCMC(compCount,
			critValMax, NOLOG, minPQPoints, maxLeafNodes, Posterior, logPrior); 
*/
			SPSnodePtrs leaves;
			(HistFirst->getSubPaving())->getLeaves(leaves);
			cout << "number of leaves in myHistFirst: " << leaves.size() << endl;
			//myHistFirst->outputToTxtTabs("Hist1.txt");
		 }

		//histogram 2
		cout << "Histogram 2" << endl;
		AdaptiveHistogram* HistSecond = new AdaptiveHistogram(pavingBox);
		//AdaptiveHistogram* myHistSecond = new AdaptiveHistogram(pavingBox);

		// put in the data in a 'pulse' with no splitting, ie into root box
		bool successfulInsertionSecond = HistSecond->insertFromRVec(*theData);
		//bool successfulInsertionSecond = HistSecond->insertFromRVec(actualData);

		// PQ
		bool successfulPQSplitSecond = false;
		if (successfulInsertionSecond) {
			// set up function objects for a priority split
			CompCount compCount;
			// split until number of leaves is at most minVal
			CritLeaves_GTE critVal(maxLeaf);
			size_t minPQPoints = 1; // minimum of one data point in each box
			size_t maxLeafNodes = pow(n,2); 

			// do the priority split
			//vector<real> Posterior;
			//LogCatalanPrior logPrior;
			//successfulPQSplitFirst = HistSecond->prioritySplitMCMC(compVol,
			//critVal, NOLOG, minPQPoints, maxLeafNodes, Posterior, logPrior); 
			successfulPQSplitSecond = HistSecond->prioritySplit(compCount,
			critVal, NOLOG, minPQPoints, maxLeafNodes); 
			//delete HistSecond;

/*			// now normalize the posteriors to get a discrete distribution
			vector<real>::iterator realIt;
			real totPos = 0;
			vector<real> NormPost;
			for (realIt = Posterior.begin(); realIt < Posterior.end(); realIt++) {
				//cout << *realIt << endl;
				totPos += *realIt;
			}

			totPos = abs(totPos);
			//find where the posterior is the highest
			size_t maxPosition = 0;
			real maxPosterior = -1000000;
			real check = 0; // need to make sure i check that the sum = 1
			for (size_t i = 0; i < Posterior.size(); i++) {
				NormPost.push_back(Posterior[i]/(totPos));
				if (Posterior[i]/totPos > maxPosterior) {
					maxPosterior = Posterior[i]/(totPos);
					maxPosition = i;
				}
			}

			// get the state with the highest posterior
			cout << "highest posterior at: " << maxPosition << endl;
			CritLeaves_GTE critValMax(maxPosition+1);
			
			// now get the posterior-optimal histogram
			successfulInsertionSecond = myHistSecond->insertFromRVec(actualData);
			//successfulInsertionFirst = myHistFirst->insertFromRVec(theData);
			successfulPQSplitSecond = myHistSecond->prioritySplitMCMC(compVol,
			critValMax, NOLOG, minPQPoints, maxLeafNodes, Posterior, logPrior); 
*/
			SPSnodePtrs leaves;
			(HistSecond->getSubPaving())->getLeaves(leaves);
			cout << "number of leaves in myHistSecond: " << leaves.size() << endl;
			//myHistSecond->outputToTxtTabs("Hist2.txt");
		 }
//===================end of getting posterior-optimal histogram==============//


/*
//====================sampled weighted states===============================//
		AdaptiveHistogram* HistFirst = new AdaptiveHistogram(pavingBox);
		AdaptiveHistogram* HistSecond = new AdaptiveHistogram(pavingBox);
		AdaptiveHistogram* myHistFirst = new AdaptiveHistogram(pavingBox);
		AdaptiveHistogram* myHistSecond = new AdaptiveHistogram(pavingBox);

		// put in the data in a 'pulse' with no splitting, ie into root box
		// bool successfulInsertionFirst = HistFirst->insertFromRVec(*theData);
		bool successfulInsertionFirst = HistFirst->insertFromRVec(actualData);
		bool successfulPQSplitFirst = false;
		bool successfulInsertionSecond = HistSecond->insertFromRVec(actualData);
		// bool successfulInsertionSecond = HistSecond->insertFromRVec(*theData);
		bool successfulPQSplitSecond = false;

		cout << "first PQ" << endl;
		// First PQ to get posterior
		if (successfulInsertionFirst) {
			// set up function objects for a priority split
			CompVolInv compVol;
			// split until number of leaves is at most minVal
			CritLeaves_GTE critVal(maxLeaf);
			size_t minPQPoints = 1; // minimum of one data point in each box
			size_t maxLeafNodes = pow(n,2); 

			vector<real> Posterior;
			LogCatalanPrior logPrior;
			
			successfulPQSplitFirst = HistFirst->prioritySplitMCMC(compVol,
			critVal, NOLOG, minPQPoints, maxLeafNodes, Posterior, logPrior); 
			delete HistFirst;

			// get the sum of the posteriors
			vector<real>::iterator realIt;
			real totPos = 0;
			for (realIt = Posterior.begin(); realIt < Posterior.end(); realIt++) {
				//cout << *realIt << endl;
				totPos += *realIt;
			}
			totPos = abs(totPos);

			//normalize the posterior
			double NormPostArray[Posterior.size()];
			for (size_t i = 0; i < Posterior.size(); i++) {
				NormPostArray[i]=(exp(_double(Posterior[i])/(totPos)));
				cout << NormPostArray[i] << endl;
			}

			// now sample from the discrrete distributions
			gslpdfstruct = gsl_ran_discrete_preproc(Posterior.size(), NormPostArray);
			vector<size_t>* Sampled = new vector<size_t>;
			size_t index1, index2;
			index1 = gsl_ran_discrete(r, gslpdfstruct);
			index2 = gsl_ran_discrete(r, gslpdfstruct);
			
			cout << index1 << "\t" << index2 << endl;
			
			if ( index1 < index2 ) { 
				(*Sampled).push_back(index1);
				(*Sampled).push_back(index2);
			}
			else {
				(*Sampled).push_back(index2);
				(*Sampled).push_back(index1);
			}
			
			cout << (*Sampled)[0] << "\t" << (*Sampled)[1] << endl;

			// Second PQ
			cout << "second pq" << endl;
			// get the states that are from the sampled indices
			vector<AdaptiveHistogram>* States = new vector<AdaptiveHistogram>;
			successfulPQSplitSecond = HistSecond->prioritySplitGet(compVol,
			critVal, NOLOG, minPQPoints, maxLeafNodes, *States, *Sampled); 
			//delete HistSecond;

			*myHistFirst = (*States)[0];
			*myHistSecond = (*States)[1];

			cout << HistSecond->getRootLeaves() << endl;
			 
			myHistFirst->outputToTxtTabs("Hist1.txt");
			myHistSecond->outputToTxtTabs("Hist2.txt");
		}
//========end of getting sampled weighted states============================//
*/


		/*
		#ifdef FORCEFAILINSERTION
			// debugging - force a failure here to check what program does
			successfulInsertionThird = false;
		#endif
		*/
		
		start = clock();
		// only proceed if successfully made histograms
		if (successfulInsertionFirst && successfulPQSplitFirst && 
			successfulInsertionSecond && successfulPQSplitSecond) { 
			
			// Initializing containers etc.
			cout << "Initializing containers since successfully made histograms. " << endl;
			// containers for adaptive histograms
			// the starting points of the chains
			vector< AdaptiveHistogram* > hists;
			hists.push_back(HistFirst);
			hists.push_back(HistSecond);

			// how many chains are to be run = number starting histograms
			size_t chains = hists.size(); 
			
			if (chains < 2) {
				throw HistException("Chains < 2");
			}

			//container to keep the L1-error between each state and true density
			vector< vector <real> > stateL1(chains);
			vector< vector <real> > avgStateL1(chains);
			
			// set up proposal distribution object
			UniformProposal proposal;
			// set up prior distribution object
			LogCatalanPrior logPrior;

			LOGGING_LEVEL logging = LOGSAMPLES; // leave it like this!
			LOGGING_LEVEL loggingInChangeStates = NOLOG;
	
			gsl_rng * rgsl = NULL;
	
			// should check that each hist to be done has a paving
	
			// set up a random number generator for uniform rvs
			const gsl_rng_type * tgsl;
			// set the library variables *gsl_rng_default and
			// gsl_rng_default_seed to default environmental vars
			gsl_rng_env_setup();
			tgsl = gsl_rng_default; // make tgsl the default type
			rgsl = gsl_rng_alloc (tgsl); // set up with default seed
			
			// set a seed for the data
			int mcmcSeed = 1234;
			gsl_rng_set(rgsl, mcmcSeed); // change the seed to our seed
	
			// set up containers for the stuff we need pass to the MCMC engine
			vector<SPSnodeList> nodeLists(chains);
			Size_tVec numLeavesVec(chains);
			Size_tVec numCherriesVec(chains);
	
			vector<string> sequenceStateFilenames(chains);
			vector<string> sequenceAverageFilenames(chains);
			vector<string> sequenceCollationFilenames(chains);
			vector<string> sequenceDiffsToAverageFilenames(chains);
			
			// names for leaves related stuff
			vector<string> leavesColNames(chains);
			vector<string> leavesRunningSumColNames(chains);
			vector<string> leavesSampleVarianceColNames(chains);
			std::string  baseLeavesColName = "leaves_";
			std::string  baseLeavesRunningSumColName = "leavesSum_";
			std::string  baseLeavesSampleVarianceColName = "leavesVar_";
			std::string  overallLeavesRunningSumColName = "OverallLeavesSum";
			
			std::string baseSequenceStateFilename = "SequenceStates";
			std::string baseSequenceStateCollationFilename = "SequenceStateCollations";
			std::string baseSequenceStateAverageFilename = "SequenceStateAverages";
			std::string baseSequenceStateDiffsToAverageFilename = "SequenceStateDiffsToAverage";
			
			// files for outputing samples
			std::string samplesCollFilename = "CollatedSamplesFromMCMCGRAuto.txt";
			std::string samplesLogFilename = "LogSamplesFromMCMCGRAuto.txt";
			//outputFileStart(samplesCollFilename);
			
			// should realy check on LOGSAMPLESANDGRAPH as well here
			// but at the moment I have not done anything about graphing
			if ( (logging = LOGSAMPLES) ) {
				outputFileStart(samplesLogFilename);
			}
			
			//std::string overSequenceAvgCollFilename = "CollationsOfAveragesOverSequences.txt";
			//outputFileStart(overSequenceAvgCollFilename);
			
			// a name for the file of diagnostics  for leaves
			std::string GRLeavesFilename = "GelmanRubinLeavesScalar.txt";
		
			// a name for a file of the leaves v_ij scalars
			std::string GR_vij_as_Leaves_Filename  = "LeavesScalar.txt";
		
			// a name for the file of working calculations for the leaves scalar diagnostics 
			std::string GRLeavesWorkingCalcsFilename = "GelmanRubinLeavesScalarWorkingCalcs.txt";
			
			/* containers for summaries for the 
			* Leaves-distances-to-average scalar convergence diagnostics */
		
			/* note we don't need this for leaves */
			//std::vector < RealVec > currentLeaves(chains); 

			// one vector of leaves as a RealVec for each chain
			std::vector < RealVec >* leavesPtr = new std::vector < RealVec >(chains);  

			/* vector containing one running sum of leaves for each chain
			we can work out the average v = leaves for each chain so far from this
			start with a running sum of 0.0 for each chain */
			RealVec* runningSumLeavesPtr = new RealVec (chains, cxsc::real(0.0));
		
			/* vector containing one running sum of 
			squared leaves for each chain
			we can work out the average of the squared v's ie v^2 = leaves^2
			for each chain so far from this
			start with a running sum of 0.0 for each chain.
			(Use a dotprecision for each running sum to keep accuracy 
			when accumulating products of reals) */
			VecDotPrec runningSumLeavesSquared(chains, cxsc::dotprecision(0.0));

			/* value of running sum of leaves over all chains
			we can work out the average v = leaves over all chains so far from this */
			real runningSumLeavesAllChains = cxsc::real(0.0);
			
			#ifdef MYDEBUG
				// keep a vector of all the overall running sums as well
				RealVec* runningSumLeavesOverallPtr = new RealVec();
				// keep a vector of the runningsums for each chain as well
				std::vector < RealVec >* runningSumLeavesChainsPtr 
									= new std::vector < RealVec >(chains);
				// keep a vector of the sample variances for each chain as well
				std::vector < RealVec >* sampleVariancesLeavesPtr 
									= new std::vector < RealVec >(chains,
										RealVec(1, cxsc::real(0.0)) );
				/* keep a vector of the flag for leaves convergence
				 * (it's not a real, but easier to output it if we treat it like one) */
				RealVec* rhatLeavesFlagPtr = new RealVec(1, cxsc::real(0.0));
			#endif
	
			RealVec* Ws_leavesPtr = new RealVec(1, cxsc::real (0.0) ); // to hold the Ws_leaves
			RealVec* Bs_leavesPtr = new RealVec(1, cxsc::real (0.0) ); // to hold the Bs_leaves
			RealVec* estVarV_leavesPtr = new RealVec(1, cxsc::real (0.0) ); // to hold the estimated var(v) for leaves
			RealVec* rhat_leavesPtr = new RealVec(1, cxsc::real (0.0) ); // to hold the rhats for leaves

			#ifdef MYDEBUG
				/* keep a vector of indicators for whether a state was sampled
				* (not a real, but easier to output it if we treat it like one) */
				RealVec* sampledIndPtr = new RealVec(1, cxsc::real(0.0));
			#endif

			// container of each state at each sequence
			vector<AdaptiveHistogramCollator*> sequenceCollators;
			// container of the collator at each sequence

			bool cancontinue = true;
			
			/* need to accumulate sum over all chains of the square of 
			* the running sum of leaves 
			* for each chain for this starting state */
			cxsc::real initialSumOfSquaresOfRunningSumsLeaves(0.0);

			// this loop is just setting up containers of file names
			// and getting info from the starting histograms that is
			// needed to start the chains
			// and a container of collators, one for each chain,
			for (size_t ci = 0; ci < chains; ci++) {
				
				// do not comment these out
				std::ostringstream stm1;
				stm1 << baseSequenceStateFilename << ci << ".txt";
				sequenceStateFilenames[ci] = stm1.str();
				outputFileStart(sequenceStateFilenames[ci]);
				
				// to output v_ij
				{
					std::ostringstream stm;
					stm << baseLeavesColName << ci;
					leavesColNames[ci] = stm.str();
				}

				#ifdef MYDEBUG
					{
						std::ostringstream stm;
						stm << baseSequenceStateCollationFilename << ci << ".txt";
						sequenceCollationFilenames[ci] = stm.str();
						outputFileStart(sequenceCollationFilenames[ci]);
					}
					{
						std::ostringstream stm;
						stm << baseSequenceStateAverageFilename << ci << ".txt";
						sequenceAverageFilenames[ci] = stm.str();
						outputFileStart(sequenceAverageFilenames[ci]);
					}
					{
						std::ostringstream stm;
						stm << baseSequenceStateDiffsToAverageFilename << ci << ".txt";
						sequenceDiffsToAverageFilenames[ci] = stm.str();
						outputFileStart(sequenceDiffsToAverageFilenames[ci]);
					}
					{
						std::ostringstream stm;
						stm << baseLeavesRunningSumColName << ci;
						leavesRunningSumColNames[ci] = stm.str();
					}
					
					{
						std::ostringstream stm;
						stm << baseLeavesSampleVarianceColName << ci;
						leavesSampleVarianceColNames[ci] = stm.str();
					}
					#endif

				/* we only need to do this because we are doing a step-by-step change of the
				* histogram states 'from the outside', ie through this example:  we need to
				* collect the stuff the histogram's changeMCMCstate method needs to make one 
				* change.  */
			  
				// set up a container for the leaf children
				SPSnodePtrs leafVec;
				// set up a container for the subleaf children
				SPSnodePtrs cherryVec;
	
				size_t numLeaves = 0;
				size_t numCherries = 0;
	
				// fill the container with the leaf children
				hists[ci]->getSubPaving()->getLeaves(leafVec);
				// fill the container with the subleaf children
				hists[ci]->getSubPaving()->getSubLeaves(cherryVec);

			//check the cherries are all "legal", ie pass checkNodeCountForSplit
			if (!cherryVec.empty()) {
				SPSnodePtrsItr cit;
				for (cit = cherryVec.begin(); cit < cherryVec.end(); cit++) {
					if (!checkNodeCountForSplit((*cit), minPoints))
					{
						throw std::logic_error(
						"\nIllegal state - cherries do not satisfy minPoints for split");
					}
				}
			}
			
			numCherries = cherryVec.size();

			// check if node is still splittable
			if (!leafVec.empty()) {
			// but only put into the container the leaves which, if split,
				 // would have at least minPoints data points associated with them
				 // or could split with one child getting all the points
				 SPSnodePtrsItr lit;
				 for (lit = leafVec.begin(); lit < leafVec.end(); lit++) {
					  if (checkNodeCountForSplit((*lit), minPoints)) {
							// leaf can go into container
							nodeLists[ci].push_back(*lit);
							numLeaves++;
					  }
				 }
				cout << "splittable leaf nodes = " << numLeaves << endl;
			}
				// no need to check on cherries - they can all go in
				if (numCherries > 0)
					 nodeLists[ci].insert(nodeLists[ci].end(), cherryVec.begin(),
													 cherryVec.end());
				if (nodeLists[ci].size() == 0) {
					 cancontinue = false;
					 break; // break out of the for loop
					 std::cout << "No changeable nodes given minPoints = "
									 << minPoints << " in histogram " << ci
									 << ". Sorry, aborting MCMC." << std::endl;
				}
	
				numLeavesVec[ci] = numLeaves;
				numCherriesVec[ci] = numCherries;

				cout << numLeaves << "\t" << numCherries << endl;

				// initialise things for the collection of data on leaves
				
				// one vector of leaves for each chain
				// record leaves for this first state
				cxsc::real lastStateLeaves(1.0*hists[ci]->getRootLeaves());
				leavesPtr->at(ci).push_back( lastStateLeaves );  
				
				// update the running sum of leaves for the chain, held in runningSumLeaves
				cxsc::real newRunningSumLeaves = runningSumLeavesPtr->at(ci) + lastStateLeaves;
				runningSumLeavesPtr->at(ci) = newRunningSumLeaves;
						
				// accumulate the square of the running sum of leaves 
				initialSumOfSquaresOfRunningSumsLeaves += newRunningSumLeaves*newRunningSumLeaves;
						
				/* update the running sum of squared leaves over this chain
				 *  held in runningSumLeavesSquared as a dot precision */
				cxsc::accumulate( runningSumLeavesSquared[ci], lastStateLeaves, lastStateLeaves );
				
				// update  the overall running sum runningSumLeavesAllChains 
				runningSumLeavesAllChains += lastStateLeaves;
				
				#ifdef MYDEBUG
					//sampleVariancesLeavesPtr->at(ci) was initialised to 0.0
					runningSumLeavesChainsPtr->at(ci).push_back (newRunningSumLeaves);
				#endif

				cout << "initialising things using the current histogram state: " << endl;
				// initialise things using current histogram state
				/* set up one collator for each chain, 
				 * starting it with the histogram state right now */
				sequenceCollators.push_back(new AdaptiveHistogramCollator(*hists[ci]));
				
				// get the IAE of this first state
				//stateL1[ci].push_back(hists[ci]->getMappedIAE(nodeEst, pavingBox));

				//moved this out from MYDEBUG_OUTPUT to get the IAE
	
				//avgStateL1[ci].push_back(colltempavg.getMappedIAE(nodeEst, pavingBox));

				#ifdef MYDEBUG_OUTPUT
				{
					sequenceCollators[ci]->publicOutputLog(sequenceCollationFilenames[ci], 1);
								AdaptiveHistogramCollator colltempavg = 
												sequenceCollators[ci]->makeAverage();
					colltempavg.publicOutputLog(sequenceAverageFilenames[ci], 1);
					
					AdaptiveHistogramCollator colltempdiffs
											= sequenceCollators[ci]->makeDifferencesToAverage();
					colltempdiffs.publicOutputLog(sequenceDiffsToAverageFilenames[ci], 1);
				} // temp objects go out of scope here	
				#endif
			} // end loop through chains setting up things to be able to start
	
			/* the overall running sum runningSumLeavesAllChains 
			 * was initialised to 0.0 
			 * and #ifdef MYDEBUG, runningSumLeavesOverall was initialised to contain one 0.0 
			 * and similarly rhatLeavesFlagPtr was initialised to contain one 0.0*/
	
			/* and we started the convergence statistics for chains with just one state in
			 * with one 0.0 in each (Ws, Bs, estVarsVs, rhats)
			 * when we initialised */

			bool goodLoop = cancontinue;

			if (cancontinue) cout << "About to do MCMC" << endl;

			/* set up some variables we need to keep track of states and sampling */
			int samplesSoFar = 0;
			size_t states = 1;  /* keep track of states in the chain = 1 so far,
							since state 1 is the initial histograms */
		
			// varibles for monitoring convergence
			int rhatLeavesFlag = 0; // indicator for whether we are burntin on L1 scalar value 
			int burntin = 0; // indicator for whether we consider ourselves burntin yet
			size_t burntinReachedState = 0; // keep track of when we (last) reached burnin
			int rhatFlagCounter = 0;
			int rhatFlagCounterThreshold = 1; 	/* how many of the scalar values must have
												* diagnostic within limits for sampling to start?
												* usually this would probably be the number
												* of scalar values being used? */	
												
			// counter to keep track of loops
			int loopCounter = 0;

			/* We also need a collator for the samples*/
			AdaptiveHistogramCollator* samplesColl = new AdaptiveHistogramCollator();

			while (goodLoop && (loopCounter < maxLoops) && (samplesSoFar < samplesNeeded)) 
			{
				#ifdef MYDEBUG_CALCS
					cout << "****** Change from state number " << states << " ******" << endl;
				#endif

				loopCounter++;

				// do initial values for everything so far
				/* we want to accumulate the sample variance of the scalar summary leaves
				 * for each chain up to the point reached in this loop */
				cxsc::real sumOfSampleVariancesLeavesOverChains(0.0);
				
				/* also accumulate sum over all chains of the square of 
				 * the running sum of leaves 
				 * for each chain up to the point reached in this loop */
				cxsc::real sumOfSquaresOfRunningSumsLeaves(0.0);
			
				// for each histogram in turn, change the state
				/* 
				 * this is all a fudge - changeMCMCstate should just be a private
				 * method of the histograms but I think I made it public so that
				 * I could use it here in the example as a first step to being
				 * able to make all of this chain convergence stuff back into
				 * a method of the histograms themselves
				*/

				for (size_t ci = 0; ci < chains; ci++) {
				
					#ifdef MYDEBUG_CALCS
						cout << "--- chain index " << ci << " ---" << endl;
					#endif

					/* I refer to the current chain, indexed by ci, as 'the chain
					* in the comments inside this loop */
//cout << "Change MCMC state " << endl;
					// changeMCMCState for the chain
					// updates nodes, numLeaves, numCherries, i
						//cout << "**** chain " << ci << "****" << endl;
					goodLoop = hists[ci]->changeMCMCState(nodeLists[ci],
							 numLeavesVec[ci], numCherriesVec[ci],
							 proposal, logPrior, minPoints,
							 rgsl, loggingInChangeStates,
							 sequenceStateFilenames[ci], states);
							 
					#ifdef FORCEFAILMCMCLOOP
						// for debugging - force a loop failure and see what happens to program
						if (states == 5) goodLoop = false;
					#endif 

					if (!goodLoop) {
						throw std::runtime_error("Failed to do MCMC change in state");
						// stop if we aren't happy
					}
				
					if ((numLeavesVec[ci] == 0 && numCherriesVec[ci] == 0)) {
						throw std::runtime_error("No more leaves or cherries in MCMC");
					}
				
					// so assume all is okay if we have not just thrown an exception
				
					/* this chain should have states + 1 states in it
					* because we have not yet incremented the states variable.*/
					size_t n_for_leaves = states + 1;
								
					/* and n_Leaves should be at least 2 because we started with the 
					* starting histogram and have now added another state.*/
					assert(n_for_leaves > 1);
				
					// collect the leaves scalar and update the running sums for leaves
					{
						// update leaves for last histogram state in the chain
						cxsc::real lastStateLeaves(1.0*hists[ci]->getRootLeaves());
						leavesPtr->at(ci).push_back( lastStateLeaves );  
										
						// update the running sum of leaves for the chain, held in runningSumLeaves
						cxsc::real newRunningSumLeaves = runningSumLeavesPtr->at(ci) + lastStateLeaves;
						runningSumLeavesPtr->at(ci) = newRunningSumLeaves;
						
						// accumulate the square of the running sum of leaves 
						sumOfSquaresOfRunningSumsLeaves += newRunningSumLeaves*newRunningSumLeaves;
						
						/* update the running sum of squared leaves over this chain
						 *  held in runningSumLeavesSquared as a dot precision */
						cxsc::accumulate( runningSumLeavesSquared[ci], lastStateLeaves, lastStateLeaves );
						
						// update  the overall running sum runningSumLeavesAllChains 
						runningSumLeavesAllChains += lastStateLeaves;
						
						/* accumulate the sample variance for leaves for this chain: 
						 * sample variance for the scalar summary v = leaves
						 * calculated as (sum of squares - n * square of averages)/(n-1)
						 * which equals (sum of squares - square of sums/n)/(n-1) */
						cxsc::real thisSampleVarianceLeaves( ( 1.0/(n_for_leaves - 1) )
								*( cxsc::rnd(runningSumLeavesSquared[ci])
								-  (newRunningSumLeaves*newRunningSumLeaves/(n_for_leaves * 1.0)) ) );
						sumOfSampleVariancesLeavesOverChains += thisSampleVarianceLeaves;
						
						#ifdef MYDEBUG
							sampleVariancesLeavesPtr->at(ci).push_back( thisSampleVarianceLeaves );
							runningSumLeavesChainsPtr->at(ci).push_back (newRunningSumLeaves);
						#endif

						#ifdef MYDEBUG_CALCS
							//check thisSampleVariance is correct, doing it the long way
							// leavesPtr[ci] has the v_ij for each chain i
							
							assert( n_for_leaves == leavesPtr->at(ci).size() );
							cxsc::real acc(0.0);
							for (RealVecItr it = leavesPtr->at(ci).begin(); it < leavesPtr->at(ci).end(); ++it) {
								acc+= (*it);
							}
							
							cxsc::real av = acc/(n_for_leaves * 1.0);
							cxsc::dotprecision accDiffs(0.0);
							for (RealVecItr it = leavesPtr->at(ci).begin(); it < leavesPtr->at(ci).end(); ++it) {
								cxsc::real thisDiff = (*it) - av;
								// sum up the squares of the differences compared to overall average
								cxsc::accumulate(accDiffs, thisDiff, thisDiff);
							}
							cxsc::real altVar = rnd(accDiffs)/( n_for_leaves - 1.0 );
							
							cout << "\nthisSampleVariance leaves is\t" << thisSampleVarianceLeaves << endl;
							cout << "altSampleVar leaves is\t" << altVar << endl;
							//assert(cxsc::_double(thisSampleVarianceLeaves) == cxsc::_double(altVar) );
						
						#endif
					}	// end of collecting leaves scalar
			} // end change state for each histogram in turn
			
			// increment number of states histograms have been through

			states++;
			//cout << "============state " << states << endl;

			/* each chain now has a new state
			 * and info for leaves scalar for diagnostics has been collected
			 * and the sample variance of the leaves scalar for each chain 
			 * has been put into sampleVariancesLeaves vector,
			 * and if we are doing full checks, the current histogram states have 
			 * has been collated into collators 
			 * and info for any other scalars for diagnostics has been collected
			 * and the sample variance of these other scalar summaries for each chain 
			 * for each scalar value
			 * have been put into sampleVariances vectors for each diagnostic,
			 * so we can now work out the convergence diagnostics */

			#ifdef MYDEBUG
				// store the current runningSumLeavesAllChains as well
				runningSumLeavesOverallPtr->push_back(runningSumLeavesAllChains);
			#endif
			
			// convergence diagnostics calculations for leaves
			{
				// the Ws_leaves: average, over chains, of sample variance of scalar value
				cxsc::real thisW_leaves = sumOfSampleVariancesLeavesOverChains/(chains * 1.0); 
				Ws_leavesPtr->push_back(thisW_leaves); 
				// the Bs_leaves
				cxsc::real thisB_leaves = (1.0/( (chains - 1) * states ) 
									* ( sumOfSquaresOfRunningSumsLeaves 
									- (runningSumLeavesAllChains 
									* runningSumLeavesAllChains/(chains * 1.0)) ) );
				Bs_leavesPtr->push_back(thisB_leaves); 
				
				#ifdef MYDEBUG_CALCS
					//check thisB_leaves is correct, doing it the long way
					// runningSumLeaves has one running sum for each chain
					RealVec chainAverages;
					cxsc::real accRunningSums(0.0);
					for (RealVecItr it = runningSumLeavesPtr->begin(); it < runningSumLeavesPtr->end(); ++it) {
						cxsc::real thisChainRunningSum = (*it);
						cxsc::real thisChainAv = thisChainRunningSum/(states * 1.0);
						chainAverages.push_back(thisChainAv);
						accRunningSums+=thisChainRunningSum;
					}
					cxsc::real overallAv = accRunningSums/(states * chains * 1.0);
					cxsc::dotprecision accDiffs(0.0);
					for (RealVecItr it = chainAverages.begin(); it < chainAverages.end(); ++it) {
						cxsc::real thisDiff = (*it) - overallAv;
						// sum up the squares of the differences compared to overall average
						cxsc::accumulate(accDiffs, thisDiff, thisDiff);
					}
					cxsc::real altB = rnd(accDiffs)*( states/(chains - 1.0) );
					
					cout << "\nthisB for leaves is\t" << thisB_leaves << endl;
					cout << "altB for leaves is\t" << altB << endl;
					//assert(thisB_leaves == altB);
				
				#endif
				
				// the estimated var(v)
				cxsc::real thisVarV_leaves = states/(states-1.0) 
								* thisW_leaves + (1.0/states)*thisB_leaves;
				estVarV_leavesPtr->push_back(thisVarV_leaves); 
				// the rhats
				cxsc::real thisRhat_leaves(0.0);
				// allow division by 0 if w = 0 when var does not
				if (thisW_leaves > 0.0 || thisVarV_leaves > 0.0) {
					thisRhat_leaves = thisVarV_leaves/thisW_leaves;
				}
				rhat_leavesPtr->push_back(thisRhat_leaves); 
				//cout << "rhat:" << thisRhat_leaves << endl;
			} // end calculations for leaves
			
			
			// check on the diagnostics for Leaves
			if (rhat_leavesPtr->back() <= 1.0 + tol 
							&& rhat_leavesPtr->back() >= 1.0 - tol) {
				// if we have not been converged before on this scalar value
				if (!rhatLeavesFlag)  {
					#ifdef MYDEBUG
						cout << "\nleaves convergence test satisfied in state " 
							  << states << endl;
					#endif
					// set the flag for this scalar value
					rhatLeavesFlag = 1;
					// and increment the flag counter = we are converged on this scalar value
					rhatFlagCounter ++; 
				}
			} // end of checking diagnostic for leaves
			/*else { // not converged on this scalar value
				// if we were okay on this scalar value before
				if (rhatLeavesFlag) {
					#ifdef MYDEBUG
						cout << "\nLeaves convergence test now NOT satisfied in state " 
						  << states << endl;
				
					#endif
					rhatLeavesFlag = 0; // update the flag
					rhatFlagCounter--; // decrement the flag counter
				} 
			}*/

			#ifdef MYDEBUG
				// store the Leavesflag as well, as a real, which is a fudge...
				rhatLeavesFlagPtr->push_back(rhatLeavesFlag);
			#endif

			if ( !burntin && (rhatFlagCounter >= rhatFlagCounterThreshold) ) {
				burntin = 1; 
				burntinReachedState = states;
				
				#ifdef MYDEBUG
					// if we have not been burntin, give a message
					 cout << "Burnin convergence test satisfied at state " 
						  << burntinReachedState << endl;
				#endif
			}
			/*
			// but it may be that we were burntin and no longer are
			else if ( burntin && (rhatFlagCounter < rhatFlagCounterThreshold) ) {
				
				burntin = 0; 
				burntinReachedState = 0;
				
				delete samplesColl; // get rid of the old samples collator
				samplesColl = new AdaptiveHistogramCollator(); // and take a new one
				
				samplesSoFar = 0;
				
				// want to change all the 1's in sampledIndPtr so far to 0s
				cxsc::real newVal(0.0);
				
				#ifdef MYDEBUG
					std::replace_if (sampledIndPtr->begin(), sampledIndPtr->end(), 
						std::bind2nd(greater< cxsc::real >(),newVal), newVal);
				#endif
				
				// restart the log file if we are logging
				// note nothing done here yet about logging graphs as well	
				if ( (logging = LOGSAMPLES) ) {
					outputFileStart(samplesLogFilename);
				}		
				
				#ifdef MYDEBUG
					cout << "Burnin convergence test now NOT satisfied at state " 
						  << states << endl;
					
				#endif
			}*/
			
			/* take samples if we are burntin and this is a sampling point according to 
			 * the thinout specified 
			 * note - we will only be in the loop at all if we still need more samples*/
			if (burntin && (( states - burntinReachedState )%thinout == 0)) {
				
				#ifdef MYDEBUG
					//cout << "sampling at state " << states << endl;
					sampledIndPtr->push_back (cxsc::real(1.0)); 

				#endif

				// take one sample from each chain until we have enough samples
				// and increment samplesSoFar for each one taken
				vector<AdaptiveHistogram*>::iterator ait;

				for (ait = hists.begin(); 
						(ait < hists.end() && samplesSoFar < samplesNeeded);
						++ait) {
					
					// add the collation (this is the main collation to output the final averaged sample)
					samplesColl->addToCollation(**ait);

					samplesSoFar++;
					cout << "samples so far: " << samplesSoFar << endl;
					
					if ( (logging = LOGSAMPLES) ) {
						//(*ait)->outputLogPlain(samplesLogFilename, samplesSoFar);
					}
				}
			} // finished taking samples for this loop
			else {
				#ifdef MYDEBUG
					sampledIndPtr->push_back (cxsc::real(0.0)); 
				#endif
			}

			assert( (samplesColl->getNumberCollated() == samplesSoFar) );

			// back into loop
			#if !defined(MYDEBUG_CALCS)
				#ifdef MYDEBUG
					// output a line every now and again so that we know it's still alive
					if (loopCounter%100 == 0) {
						cout << "\n...I'm still going: completed change in state number " << states << " ...\n" << endl;
					}
				#endif
			#endif
		}    // finished while loop - either loop failed or reached maxLoops or have all our samples

		cancontinue = goodLoop;

		// stop recording time here
		end = clock();	
		timing = ((static_cast<double>(end - start)) / CLOCKS_PER_SEC);
		cout << "Computing time : " << timing << " s."<< endl;
		
		#ifdef MYDEBUG
			cout << "****** finished all loops, states counter is = " << states << " ******" << endl;
		#endif

		cout << "\nnumber of samples collected is = " << samplesColl->getNumberCollated() << endl;

		// free the random number generator
		gsl_rng_free (rgsl);

		cout << cxsc::RestoreOpt; // undo changes we made to cout printing for cxsc values

		/* is all okay with the loop
		 * and we have all our samples */
		if (cancontinue && (samplesSoFar >= samplesNeeded) ) {
			#ifdef MYDEBUG
				// output the overall collator
				samplesColl->outputToTxtTabs(samplesCollFilename);
			#endif

			// make the return object be the average of the samples
			*samplesAvg = samplesColl->makeAverage();

			// output the convergence diagnostics
			//output file for leaves
			{
				std::vector < std::string > colNames;
				colNames.push_back("W");
				colNames.push_back("B");
				colNames.push_back("estVarV");
				colNames.push_back("rhat");
				#ifdef MYDEBUG
					colNames.push_back("rhatFlag");
					colNames.push_back("sampled?");
				#endif
				std::vector < RealVec* > data;
				data.push_back(Ws_leavesPtr);
				data.push_back(Bs_leavesPtr);
				data.push_back(estVarV_leavesPtr);
				data.push_back(rhat_leavesPtr);
				#ifdef MYDEBUG
					data.push_back(rhatLeavesFlagPtr);
					data.push_back(sampledIndPtr);
				#endif
				int precData = 5;
				outputToFileVertical(data, colNames, GRLeavesFilename, precData);
			} // all the stuff created in these {} goes out of scope here
			
			
			// output the leaves  as v_ij's)
			{
				std::vector < std::string > colNames;
				colNames.insert(colNames.end(), leavesColNames.begin(), leavesColNames.end());
				
				std::vector < RealVec* > data;
				data = addDataPtrs(data, *leavesPtr);
				
				int precData = 10;
				outputToFileVertical(data, colNames, GR_vij_as_Leaves_Filename, precData);
			}

			#ifdef MYDEBUG
			{
				/* output working calcs: all leaves for each chain, 
				 * running sums for each chain, sample variances,
				 * overall running sums */
				std::vector < std::string > colNames;
				colNames.insert(colNames.end(), leavesColNames.begin(), leavesColNames.end());
				colNames.insert(colNames.end(), leavesRunningSumColNames.begin(), leavesRunningSumColNames.end());
				colNames.insert(colNames.end(), leavesSampleVarianceColNames.begin(), leavesSampleVarianceColNames.end());
				colNames.push_back(overallLeavesRunningSumColName);
				
				std::vector < RealVec* > data;
				data = addDataPtrs(data, *leavesPtr);
				data = addDataPtrs(data, *runningSumLeavesChainsPtr);
				data = addDataPtrs(data, *sampleVariancesLeavesPtr);
				data.push_back(runningSumLeavesOverallPtr);
				
				int precData = 10;
				outputToFileVertical(data, colNames, GRLeavesWorkingCalcsFilename, precData);
			
			}
			#endif

			cout << "Burnt in at " << burntinReachedState << endl;
			
			cout << "\n\nFinished MCMC successfully\n" << endl;
			cout << "Check output files\n\t" << GRLeavesFilename
							<< "\nfor diagnostics" << endl;
			cout << "and for scalar values\n\t" << GR_vij_as_Leaves_Filename << endl;
			if ( (logging = LOGSAMPLES) ) {
				cout << "and\t" << samplesLogFilename
					  << "\nfor log of samples" <<endl;
			}
			#ifdef MYDEBUG
				cout << "and " << GRLeavesWorkingCalcsFilename
					  << "\nfor working calculations for diagnostics" <<endl;
			#endif
			#ifdef MYDEBUG_OUTPUT
				cout << "and\t" << baseSequenceStateCollationFilename << "*.txt, \n\t"
					  << baseSequenceStateAverageFilename << "*.txt \n\t"
					  << baseSequenceStateDiffsToAverageFilename << "*.txt \n"
					  << "for sequence development details" <<endl;
			#endif
			cout << endl;
		}
		
		
		//put this after clean up the newed stuff
		
		/* since I throw an exception in the while loop if it is not a good loop,
		 *  really the only reason for failing here is that we did not get the right 
		 * number of samples, but might as well leave it like this - belt & braces*/ 
		if (!cancontinue || (samplesSoFar < samplesNeeded) ) {
			cout << "\nMCMC not successful" << endl;
			cout << "Output files will not be complete - delete or ignore:\n"
					<< GRLeavesFilename
					<< "\n" << GR_vij_as_Leaves_Filename << endl;
					
			// remove this afterwards!
			// output the convergence diagnostics
			//output file for leaves
			{
				std::vector < std::string > colNames;
				colNames.push_back("W");
				colNames.push_back("B");
				colNames.push_back("estVarV");
				colNames.push_back("rhat");
				#ifdef MYDEBUG
					colNames.push_back("rhatFlag");
					colNames.push_back("sampled?");
				#endif
				std::vector < RealVec* > data;
				data.push_back(Ws_leavesPtr);
				data.push_back(Bs_leavesPtr);
				data.push_back(estVarV_leavesPtr);
				data.push_back(rhat_leavesPtr);
				#ifdef MYDEBUG
					data.push_back(rhatLeavesFlagPtr);
					data.push_back(sampledIndPtr);
				#endif
				int precData = 5;
				outputToFileVertical(data, colNames, GRLeavesFilename, precData);
			} // all the stuff created in these {} goes out of scope here
			
			
			// output the leaves  as v_ij's)
			{
				std::vector < std::string > colNames;
				colNames.insert(colNames.end(), leavesColNames.begin(), leavesColNames.end());
				
				std::vector < RealVec* > data;
				data = addDataPtrs(data, *leavesPtr);
				
				int precData = 10;
				outputToFileVertical(data, colNames, GR_vij_as_Leaves_Filename, precData);
			}

			#ifdef MYDEBUG
				cout << GRLeavesWorkingCalcsFilename << endl;
			#endif
			if ( (logging = LOGSAMPLES) ) {
				cout << samplesLogFilename << endl;
			}
			#ifdef MYDEBUG_OUTPUT
				cout << baseSequenceStateCollationFilename << "*.txt,"
				<< "\n" <<  baseSequenceStateAverageFilename << "*.txt,"
				<< "\n" <<  baseSequenceStateDiffsToAverageFilename << "*.txt" << endl;
			#endif
			cout << endl;
			
			if (!cancontinue) {
				throw std::runtime_error("MCMC failed");
			}
			if (samplesSoFar < samplesNeeded) {
				// we have not been able to get the required samples - need to give up
				throw std::runtime_error("Did not get required number of samples");
			}
		}




		/* clean up the newed stuff
		 * 
		 * note that this does not get cleaned up if we throw an exception in the while loop
		 * - should probably deal with that at some point but all the newed memory will be 
		 * freed when it terminates anyway so assuming this code is just run as a one-off example,
		 * it will be okay for the moment */

		vector<AdaptiveHistogram*>::iterator ait;
		for (ait = hists.begin(); ait < hists.end(); ++ait) {
			if (NULL != *ait) delete (*ait);
		}

		vector<AdaptiveHistogramCollator*>::iterator acit;
		for (acit = sequenceCollators.begin(); acit < sequenceCollators.end(); ++acit) {
			if (NULL != *acit) delete (*acit);
		}
		
		/*
		for (acit = averageCollators.begin(); acit < averageCollators.end(); acit++) {
			if (NULL != *acit) delete (*acit);
		}
		*/
		delete samplesColl;
		
		#ifdef MYDEBUG
			delete sampledIndPtr;
		#endif
		
		// leaves stuff
		delete leavesPtr;  
		
		#ifdef MYDEBUG
			delete runningSumLeavesOverallPtr;
			delete runningSumLeavesChainsPtr;
			delete sampleVariancesLeavesPtr;
			delete rhatLeavesFlagPtr;
		#endif

		delete Ws_leavesPtr;
		delete Bs_leavesPtr;
		delete estVarV_leavesPtr;
		delete rhat_leavesPtr;


			
			/*
			// Output the L1 error of states
			vector< vector<real> >::iterator it1;
			vector<real>::iterator it2;
			string stateL1FileName = "GaussianStateL1FileName";
			stateL1FileName += stmH.str();
			stateL1FileName += ".txt";
			oss.open(stateL1FileName.c_str());
			for (it1 = stateL1.begin(); it1 < stateL1.end(); it1++) { 
				for (it2 = (*it1).begin(); it2 < (*it1).end(); it2++) {
					oss << (*it2) << "\t";
				}
				oss << "\n";
			}
			oss << flush;
			oss.close();
			
			// Output the L1 error of avg states
			string avgL1FileName = "GaussianAvgL1FileName";
			avgL1FileName += stmH.str();
			avgL1FileName += ".txt";
			oss.open(avgL1FileName.c_str());
			for (it1 = avgStateL1.begin(); it1 < avgStateL1.end(); it1++) { 
				for (it2 = (*it1).begin(); it2 < (*it1).end(); it2++) {
					oss << (*it2) << "\t";
				}
				oss << "\n";
			}
			oss << flush;
			oss.close();
			cout << "L1-errors output to " << avgL1FileName << " and " << stateL1FileName << endl;
			*/
			
			/*
			//==============get distribution parameters==========================//
			// read input from mix*.txt
			string mixfileName;
			mixfileName = "MixtureFiles/mix";
			std::ostringstream stmMix;
			stmMix << 1;
			mixfileName += stmMix.str();
			mixfileName += ".txt";
			
			ifstream infile(mixfileName.c_str());
			double Weight1, Weight2, Mean1, Mean2, Var1, Var2;
			double W, M, V;
			
			// create vectors for Weight, Mean, Variance
			vector<double> Weight;
			vector<double> Mean;
			vector<double> Sigma;
			
			cout << "Reading in parameters of mixture " << 1 << endl;
			ifstream file; // create file for input
			file.open(mixfileName.c_str());
			// check if this file exists or not
			if ( !file ) { // exit if file doesn't exists
				cerr << "Could not open " << mixfileName << ". It does not exist." 
				     << endl;
				exit(1);
			}
			while ( !infile.eof() ) {
				infile >> Weight1 >> Weight2 >> Mean1 >> Mean2 >> Var1 >> Var2;
				W=Weight1/Weight2;
				Weight.push_back(W);
				M=Mean1/Mean2;
				Mean.push_back(M);
				V=Var1/Var2;
				Sigma.push_back(sqrt(V));
			}
			Weight.pop_back();
			Mean.pop_back();
			Sigma.pop_back();
			
			//put the parameters as data members of the structure
			FinMix mixt;
			mixt.W = Weight; mixt.M = Mean; mixt.S = Sigma;
					
					double Tol = 1e-15; // tolerance for root finding and integration routines
				cout << "Tolerance is: " << Tol << endl;
				int Deg = 2; // Degree of Taylor series.
					real trueIAE = mid(samplesAvg->getFinMixIntervalIAE(mixt, Tol, Deg)); 
					cout << "True IAE is " << trueIAE << endl;
			//end of actual
			*/

			
			// now get the IAE of samplesAvg
			real thisIAE = samplesAvg->getMappedIAE(nodeEst, pavingBox); 
			//output IAE to file
			string IAEFileName;
			IAEFileName = "GaussianIAE";
			IAEFileName += stmH.str();
			IAEFileName += ".txt";
			oss.open(IAEFileName.c_str());
			oss << thisIAE <<  endl;
		//	oss << thisIAE << "\t" << trueIAE << endl;
			oss << flush;
			oss.close();
			cout << "IAE " << thisIAE << " output to " << IAEFileName << endl;
			
			
			//output time taken to file
			string timeFileName;
			timeFileName = "GaussianTime";
			timeFileName += stmH.str();
			timeFileName += ".txt";
			oss.open(timeFileName.c_str());
			oss << timing << endl ;
			oss << flush;
			oss.close();
			cout << "Timings output to " << timeFileName << endl << endl;
	
			cout << "Remember to do height normalization with: " << totalArea << endl;
	
		//return samplesAvg;
			
		} // end check on successful insertion of data into histograms
		
		else {
			throw std::runtime_error("MCMC failed: could not insert data into all starting histograms");
		}
		
		
		string histFileName;
			histFileName = "FinalHist";
			histFileName += stmH.str();
			histFileName += ".txt";
		samplesAvg->outputToTxtTabs(histFileName);
		/*
		string leafFile = "LeafLevel.txt";
		oss.open(leafFile.c_str());
			oss << samplesAvg->getLeafLevelsString() << endl ;
			oss << flush;
			oss.close();
		*/
		delete samplesAvg;
//		delete theData;
		
	} // end of numHist
	
	//free the random number generator
	gsl_rng_free(r);
	//gsl_ran_discrete_free (gslpdfstruct);
	
	
	//can i make the doMCMCGRAuto function to be a void function?
	AdaptiveHistogramCollator coll;
	return coll;

} // end of MCMC test program





