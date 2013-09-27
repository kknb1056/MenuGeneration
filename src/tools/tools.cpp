#include "l1menu/tools/tools.h"

#include <sstream>
#include <exception>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <ostream>
#include <fstream>
#include <iomanip>
#include "l1menu/ITrigger.h"
#include "l1menu/L1TriggerDPGEvent.h"
#include "l1menu/TriggerTable.h"
#include "l1menu/TriggerMenu.h"
#include "l1menu/IMenuRate.h"
#include "l1menu/ITriggerRate.h"
#include "l1menu/FullSample.h"
#include "l1menu/ReducedSample.h"


std::vector<std::string> l1menu::tools::getThresholdNames( const l1menu::ITrigger& trigger )
{
	std::vector<std::string> returnValue;

	//
	// I don't know how many thresholds there are, so I'll try and get every possible one and catch
	// the exception when I eventually hit a threshold that doesn't exist.
	//

	std::stringstream stringConverter;
	// I plan in the future to implement cross triggers with "leg1threshold1", "leg2threshold1" etcetera,
	// so I'll loop over those possible prefixes.
	for( size_t legNumber=0; true; ++legNumber ) // Loop continuously until the exception handler calls "break"
	{
		size_t thresholdNumber;
		try
		{
			// Loop over all possible numbers of thresholds
			for( thresholdNumber=1; true; ++thresholdNumber ) // Loop continuously until I hit an exception
			{
				stringConverter.str("");
				if( legNumber!=0 ) stringConverter << "leg" << legNumber; // For triggers with only one leg I don't want to prefix anything.

				stringConverter << "threshold" << thresholdNumber;

				trigger.parameter(stringConverter.str());
				// If the threshold doesn't exist the statement above will throw an exception, so
				// I've reached this far then the threshold name must exist.
				returnValue.push_back( stringConverter.str() );
			}

		}
		catch( std::exception& error )
		{
			// If this exception is from the first threshold tried then the prefix (e.g. "leg1") does not
			// exist, so I know I've finished. If it isn't from the first threshold then there could be
			// other prefixes (e.g. "leg2") that have thresholds that can be modified, in which case I
			// need to continue.
			if( thresholdNumber==1 && legNumber!=0 ) break;
		}
	}

	return returnValue;
}

std::vector<std::string> l1menu::tools::getNonThresholdParameterNames( const l1menu::ITrigger& trigger )
{
	std::vector<std::string> returnValue;

	// It'll be easier to get the threshold names and then copy
	// everything that's not in there to the return value.
	std::vector<std::string> allParameterNames=trigger.parameterNames();
	std::vector<std::string> thresholdNames=getThresholdNames(trigger);

	for( const auto& parameterName : allParameterNames )
	{
		const auto& iFindResult=std::find( thresholdNames.begin(), thresholdNames.end(), parameterName );
		// If the current parameter name isn't one of the thresholds add
		// it the vector which I'm going to return.
		if( iFindResult==thresholdNames.end() ) returnValue.push_back(parameterName);
	}

	return returnValue;
}

void l1menu::tools::setTriggerThresholdsAsTightAsPossible( const l1menu::L1TriggerDPGEvent& event, l1menu::ITrigger& trigger, float tolerance )
{
	std::vector<std::string> thresholdNames=l1menu::tools::getThresholdNames( trigger );
	std::map<std::string,float> tightestPossibleThresholds;

	//
	// If the thresholds are correlated, then I can't modify them individually to see if an event will pass
	// and I'll have to scale them all together. So if they're correlated figure out what the scalings need
	// to be.
	//
	std::vector< std::pair<float*,float> > otherParameterScalings; // "first" is a pointer to the parameter, "second" is the amount to scale it
	if( trigger.thresholdsAreCorrelated() )
	{
		// Use the first threshold as the one to vary
		std::string versusParameter=thresholdNames[0];
		float parameterValue=trigger.parameter(versusParameter); // Take a copy to save constantly looking it up

		// Then scale all of the other ones against that
		for( const auto& parameterToScale : thresholdNames )
		{
			if( parameterToScale!=versusParameter )
			{
				otherParameterScalings.push_back( std::make_pair( &trigger.parameter(parameterToScale), trigger.parameter(parameterToScale)/parameterValue ) );
			}
		}

		// Now clear the list of tresholdNames of everything except the main one.
		// Everything else will be scaled against this.
		thresholdNames.resize(1);
	}

	// First set all of the thresholds to zero
	for( const auto& thresholdName : thresholdNames ) trigger.parameter(thresholdName)=0;

	// Now run through each threshold at a time and figure out how low it can be and still
	// pass the event.
	for( const auto& thresholdName : thresholdNames )
	{
		// Note that this is a reference, so when this is changed the trigger is modified
		float& threshold=trigger.parameter(thresholdName);

		float lowThreshold=0;
		float highThreshold=500;
		// See if an indication of the range of the trigger has been set
		try // These calls will throw an exception if no suggestion has been set
		{
			lowThreshold=l1menu::TriggerTable::instance().getSuggestedLowerEdge( trigger.name(), thresholdName );
			highThreshold=l1menu::TriggerTable::instance().getSuggestedUpperEdge( trigger.name(), thresholdName );
		}
		catch( std::exception& error ) { /* No indication set. Do nothing and just use the defaults I set previously. */ }
		highThreshold*=5; // Make sure the high threshold is very high, to catch all tails


		threshold=lowThreshold;
		// Scale any other parameters required. There will only be something in this vector if the trigger thresholds are correlated.
		for( const auto& parameterScalingPair : otherParameterScalings ) *(parameterScalingPair.first)=parameterScalingPair.second*threshold;
		// Test the trigger
		bool lowTest=trigger.apply( event );

		threshold=highThreshold;
		for( const auto& parameterScalingPair : otherParameterScalings ) *(parameterScalingPair.first)=parameterScalingPair.second*threshold;
		bool highTest=trigger.apply( event );

		if( lowTest==highTest ) throw std::runtime_error( "l1menu::tools::setTriggerThresholdsAsTightAsPossible() - couldn't find a set of thresholds to pass the given event.");

		// Try and find the turn on point by bisection
		while( highThreshold-lowThreshold > tolerance )
		{
			threshold=(highThreshold+lowThreshold)/2;
			for( const auto& parameterScalingPair : otherParameterScalings ) *(parameterScalingPair.first)=parameterScalingPair.second*threshold;
			bool midTest=trigger.apply(event);

			if( lowTest==midTest && highTest!=midTest ) lowThreshold=threshold;
			else if( highTest==midTest ) highThreshold=threshold;
			else throw std::runtime_error( std::string("Something fucked up while testing ")+trigger.name() );
		}

		// Record what this value was for the parameter named
		tightestPossibleThresholds[thresholdName]=highThreshold;
		// Then set back to zero ready to test the other thresholds
		threshold=0;
	}

	//
	// Now that I've figured out what all of the thresholds need to be, run
	// through and set the trigger up with these thresholds.
	//
	for( const auto& parameterValuePair : tightestPossibleThresholds )
	{
		trigger.parameter(parameterValuePair.first)=parameterValuePair.second;
		// And also set any of the scaled parameters to reflect what they should be at this value
		for( const auto& parameterScalingPair : otherParameterScalings ) *(parameterScalingPair.first)=parameterScalingPair.second*parameterValuePair.second;
	}
}

void l1menu::tools::dumpTriggerRates( std::ostream& output, const l1menu::IMenuRate& menuRates )
{
	// I want to print the triggers in the same order as the old code does to make results
	// easier to compare between the old and new code. Otherwise the new code will print
	// the results alphabetically. I need to hard code that order with this vector.
	std::vector<std::string> triggerNames;
	triggerNames.push_back("L1_SingleEG");
	triggerNames.push_back("L1_SingleIsoEG");
	triggerNames.push_back("L1_SingleMu");
	triggerNames.push_back("L1_SingleIsoMu");
	triggerNames.push_back("L1_SingleTau");
	triggerNames.push_back("L1_SingleIsoTau");
	triggerNames.push_back("L1_DoubleEG");
	triggerNames.push_back("L1_isoEG_EG");
	triggerNames.push_back("L1_DoubleIsoEG");
	triggerNames.push_back("L1_DoubleMu");
	triggerNames.push_back("L1_isoMu_Mu");
	triggerNames.push_back("L1_DoubleIsoMu");
	triggerNames.push_back("L1_DoubleTau");
	triggerNames.push_back("L1_isoTau_Tau");
	triggerNames.push_back("L1_DoubleIsoTau");
	triggerNames.push_back("L1_EG_Mu");
	triggerNames.push_back("L1_isoEG_Mu");
	triggerNames.push_back("L1_isoEG_isoMu");
	triggerNames.push_back("L1_Mu_EG");
	triggerNames.push_back("L1_isoMu_EG");
	triggerNames.push_back("L1_isoMu_isoEG");
	triggerNames.push_back("L1_EG_Tau");
	triggerNames.push_back("L1_isoEG_Tau");
	triggerNames.push_back("L1_isoEG_isoTau");
	triggerNames.push_back("L1_Mu_Tau");
	triggerNames.push_back("L1_isoMu_Tau");
	triggerNames.push_back("L1_isoMu_isoTau");
	triggerNames.push_back("L1_SingleJet");
	triggerNames.push_back("L1_SingleJetC");
	triggerNames.push_back("L1_DoubleJet");
	triggerNames.push_back("L1_QuadJetC");
	triggerNames.push_back("L1_SixJet");
	triggerNames.push_back("L1_SingleEG_CJet");
	triggerNames.push_back("L1_SingleIsoEG_CJet");
	triggerNames.push_back("L1_SingleMu_CJet");
	triggerNames.push_back("L1_SingleIsoMu_CJet");
	triggerNames.push_back("L1_SingleTau_TwoFJet");
	triggerNames.push_back("L1_DoubleFwdJet");
	triggerNames.push_back("L1_SingleEG_ETM");
	triggerNames.push_back("L1_SingleIsoEG_ETM");
	triggerNames.push_back("L1_SingleMu_ETM");
	triggerNames.push_back("L1_SingleIsoMu_ETM");
	triggerNames.push_back("L1_SingleTau_ETM");
	triggerNames.push_back("L1_SingleIsoTau_ETM");
	triggerNames.push_back("L1_SingleEG_HTM");
	triggerNames.push_back("L1_SingleIsoEG_HTM");
	triggerNames.push_back("L1_SingleMu_HTM");
	triggerNames.push_back("L1_SingleIsoMu_HTM");
	triggerNames.push_back("L1_SingleTau_HTM");
	triggerNames.push_back("L1_SingleIsoTau_HTM");
	triggerNames.push_back("L1_HTM");
	triggerNames.push_back("L1_ETM");
	triggerNames.push_back("L1_HTT");

	// Take a copy of the results so that I can resort them.
	auto triggerRates=menuRates.triggerRates();
	//
	// Use a lambda function to sort the ITriggerRates into the same
	// order as the standard list above.
	//
	std::sort( triggerRates.begin(), triggerRates.end(), [&](const l1menu::ITriggerRate* pFirst,const l1menu::ITriggerRate* pSecond)->bool
		{
			auto iFirstPosition=std::find( triggerNames.begin(), triggerNames.end(), pFirst->trigger().name() );
			auto iSecondPosition=std::find( triggerNames.begin(), triggerNames.end(), pSecond->trigger().name() );
			// If both these trigger names aren't in the standard list, sort
			// them by their name which I guess means alphabetically.
			if( iFirstPosition==triggerNames.end() && iSecondPosition==triggerNames.end() ) return pFirst->trigger().name()<pSecond->trigger().name();
			// If only one of them is in the list then that one needs to be first.
			else if( iFirstPosition==triggerNames.end() ) return false;
			else if( iSecondPosition==triggerNames.end() ) return true;
			// If they're both in the standard list sort by their position in the list
			else return iFirstPosition<iSecondPosition;
		} );

	float totalNoOverlaps=0;
	float totalPure=0;
	for( const auto& pRate : triggerRates )
	{
		const auto& trigger=pRate->trigger();
		//output << "Trigger " << pRate->trigger().name() << " has fraction " << pRate->fraction() << " and rate " << pRate->rate() << "kHz, pure " << pRate->pureRate() << "kHz" << std::endl;
		// Print the name
		output << std::setw(23) << pRate->trigger().name();

		// Print the thresholds
		std::vector<std::string> thresholdNames=l1menu::tools::getThresholdNames( trigger );
		for( size_t thresholdNumber=0; thresholdNumber<4; ++thresholdNumber )
		{
			float threshold;
			if( thresholdNames.size()>thresholdNumber ) threshold=trigger.parameter(thresholdNames[thresholdNumber]);
			else threshold=-1;

			output << std::setw(8) << threshold;
		}

		// Print the rates
		output << std::setw(16) << pRate->rate();
		output << std::setw(12) << pRate->pureRate();

		totalNoOverlaps+=pRate->rate();
		totalPure+=pRate->pureRate();

		output << "\n";
	}

	output << "---------------------------------------------------------------------------------------------------------------" << "\n"
			<< " Total L1 Rate (with overlaps)    = " << std::setw(8) << menuRates.totalRate() << "kHz" << "\n"
			<< " Total L1 Rate (without overlaps) = " << std::setw(8) << totalNoOverlaps << "kHz" << "\n"
			<< " Total L1 Rate (pure triggers)    = " << std::setw(8) << totalPure << "kHz" << std::endl;
}

void l1menu::tools::dumpTriggerMenu( std::ostream& output, const l1menu::TriggerMenu& menu )
{
	for( size_t index=0; index< menu.numberOfTriggers(); ++index )
	{
		const l1menu::ITrigger& trigger=menu.getTrigger(index);
		output << std::setw(21) << trigger.name()
				<< " 00 " // previous code output the trigger bit number. For now I'll output an arbitrary "00"
				<< std::setw(5) << " 1 "; // haven't implemented prescales yet, so output "1"
		std::vector<std::string> thresholdNames=l1menu::tools::getThresholdNames(trigger);
		for( const auto& name : thresholdNames ) output << std::setw(5) << trigger.parameter(name) << " ";
		// If there is fewer than 4 thresholds, then I need to output arbitrary null values
		for( size_t thresholdNumber=thresholdNames.size(); thresholdNumber<4; ++thresholdNumber ) output << std::setw(5) << -1 << " ";

		// I now need to output the eta cut, which can be stored in a number of ways.
		float etaCut;
		try{ etaCut=trigger.parameter("etaCut"); }
		catch(...)
		{
			try{ etaCut=trigger.parameter("regionCut"); }
			catch(...)
			{
				try{ etaCut=trigger.parameter("leg1etaCut"); }
				catch(...)
				{
					try{ etaCut=trigger.parameter("leg1regionCut"); }
					catch(...)
					{
						// Print out a default of -1 if no eta cut can be found
						etaCut=-1;
					}
				}
			}
		}
		output << std::setw(5) << etaCut << " ";

		// Also need muon quality, if it exists
		float muonQuality;
		try{ muonQuality=trigger.parameter("muonQuality"); }
		catch(...)
		{
			try{ muonQuality=trigger.parameter("leg1muonQuality"); }
			catch(...)
			{
				try{ muonQuality=trigger.parameter("leg2muonQuality"); }
				catch(...)
				{
					// Print out a default of -1 if no quality can be found
					muonQuality=-1;
				}
			}
		}
		output << std::setw(5) << muonQuality << " ";

		output << "\n";
	}

}

std::pair<float,float> l1menu::tools::calorimeterRegionEtaBounds( size_t calorimeterRegion )
{
	if( calorimeterRegion==0 ) return std::make_pair( -5.0, -4.5 );
	else if( calorimeterRegion==1 ) return std::make_pair( -4.5, -4.0 );
	else if( calorimeterRegion==2 ) return std::make_pair( -4.0, -3.5 );
	else if( calorimeterRegion==3 ) return std::make_pair( -3.5, -3.0 );
	else if( calorimeterRegion==4 ) return std::make_pair( -3.0, -2.172 );
	else if( calorimeterRegion==5 ) return std::make_pair( -2.172, -1.74 );
	else if( calorimeterRegion==6 ) return std::make_pair( -1.74, -1.392 );
	else if( calorimeterRegion==7 ) return std::make_pair( -1.392, -1.044 );
	else if( calorimeterRegion==8 ) return std::make_pair( -1.044, -0.696 );
	else if( calorimeterRegion==9 ) return std::make_pair( -0.696, -0.348 );
	else if( calorimeterRegion==10 ) return std::make_pair( -0.348, 0 );
	else if( calorimeterRegion==11 ) return std::make_pair( 0, 0.348 );
	else if( calorimeterRegion==12 ) return std::make_pair( 0.348, 0.696 );
	else if( calorimeterRegion==13 ) return std::make_pair( 0.696, 1.044 );
	else if( calorimeterRegion==14 ) return std::make_pair( 1.044, 1.392 );
	else if( calorimeterRegion==15 ) return std::make_pair( 1.392, 1.74 );
	else if( calorimeterRegion==16 ) return std::make_pair( 1.74, 2.172 );
	else if( calorimeterRegion==17 ) return std::make_pair( 2.172, 3.0 );
	else if( calorimeterRegion==18 ) return std::make_pair( 3.0, 3.5 );
	else if( calorimeterRegion==19 ) return std::make_pair( 3.5, 4.0 );
	else if( calorimeterRegion==20 ) return std::make_pair( 4.0, 4.5 );
	else if( calorimeterRegion==21 ) return std::make_pair( 4.5, 5.0 );
	else throw std::runtime_error( "l1menu::tools::calorimeterRegionEtaBounds was given an invalid calorimeter region" );
}

float l1menu::tools::convertEtaCutToRegionCut( float etaCut )
{
	if( etaCut<calorimeterRegionEtaBounds(0).first || etaCut>calorimeterRegionEtaBounds(21).second )
			throw std::runtime_error( "l1menu::tools::convertEtaCutToRegionCut was given an eta value outside the allowed region" );

	size_t caloRegion;
	for( caloRegion=0; caloRegion<=21; ++caloRegion )
	{
		std::pair<float,float> regionBounds=calorimeterRegionEtaBounds( caloRegion );
		if( regionBounds.first<=etaCut && etaCut<regionBounds.second ) break;
	}

	// I should have an answer for which calorimeter region the etaCut corresponds to.
	// I want to format the result to make sure it's in one half of the symmetric detector.
	if( caloRegion>10 ) caloRegion=21-caloRegion;
	return caloRegion;
}

float l1menu::tools::convertRegionCutToEtaCut( float regionCut )
{
	if( regionCut<0 || regionCut>21 ) throw std::runtime_error( "l1menu::tools::convertRegionCutToEtaCut was given an invalid calorimeter region" );

	// Result will be absolute eta. Use the symmetry to make the comparison easier.
	if( regionCut>10 ) regionCut=21-regionCut;

	// The float will be truncated to an integer.
	std::pair<float,float> regionBounds=calorimeterRegionEtaBounds( regionCut );

	return std::fabs(regionBounds.second);
}

void l1menu::tools::setBinningToL1Menu2015Values()
{
	l1menu::TriggerTable& triggerTable=l1menu::TriggerTable::instance();
	triggerTable.registerSuggestedBinning( "L1_DoubleJet",        "threshold1",     101,  -2,    402    );
	triggerTable.registerSuggestedBinning( "L1_DoubleMu",         "threshold1",     141,  -0.5,  140.5  );
	triggerTable.registerSuggestedBinning( "L1_HTM",              "threshold1",     201,  -0.5,  200.5  );
	triggerTable.registerSuggestedBinning( "L1_HTT",              "threshold1",     1601, -0.25, 800.25 );
	triggerTable.registerSuggestedBinning( "L1_isoEG_EG",         "leg1threshold1", 64,   -0.5,  63.5   );
	triggerTable.registerSuggestedBinning( "L1_SingleIsoEG_HTM",  "leg1threshold1", 64,   -0.5,  63.5   );
	triggerTable.registerSuggestedBinning( "L1_SingleIsoEG_CJet", "leg1threshold1", 64,   -0.5,  63.5   );
	triggerTable.registerSuggestedBinning( "L1_isoEG_Mu",         "leg1threshold1", 64,   -0.5,  63.5   );
	triggerTable.registerSuggestedBinning( "L1_isoEG_Tau",        "leg1threshold1", 64,   -0.5,  63.5   );
	triggerTable.registerSuggestedBinning( "L1_isoMu_Mu",         "threshold1",     141,  -0.5,  140.5  );
	triggerTable.registerSuggestedBinning( "L1_isoTau_Tau",       "leg1threshold1", 201,  -0.5,  200.5  );
	triggerTable.registerSuggestedBinning( "L1_isoMu_EG",         "leg1threshold1", 141,  -0.5,  140.5  );
	triggerTable.registerSuggestedBinning( "L1_isoMu_Tau",        "leg1threshold1", 141,  -0.5,  140.5  );
	triggerTable.registerSuggestedBinning( "L1_SingleMu_HTM",     "leg1threshold1", 141,  -0.5,  140.5  );
	triggerTable.registerSuggestedBinning( "L1_SingleMu_CJet",    "leg1threshold1", 141,  -0.5,  140.5  );
	triggerTable.registerSuggestedBinning( "L1_QuadJetC",         "threshold1",     101,  -2,    402    );
	triggerTable.registerSuggestedBinning( "L1_SingleEG",         "threshold1",     64,   -0.5,  63.5   );
	triggerTable.registerSuggestedBinning( "L1_SingleIsoEG",      "threshold1",     64,   -0.5,  63.5   );
	triggerTable.registerSuggestedBinning( "L1_SingleIsoMu",      "threshold1",     141,  -0.5,  140.5  );
	triggerTable.registerSuggestedBinning( "L1_SingleIsoTau",     "threshold1",     201,  -0.5,  200.5  );
	triggerTable.registerSuggestedBinning( "L1_SingleJetC",       "threshold1",     101,  -2,    402    );
	triggerTable.registerSuggestedBinning( "L1_SingleMu",         "threshold1",     141,  -0.5,  140.5  );
	triggerTable.registerSuggestedBinning( "L1_SingleTau",        "threshold1",     201,  -0.5,  200.5  );
	triggerTable.registerSuggestedBinning( "L1_SixJet",           "threshold1",     101,  -2,    402    );
}

std::unique_ptr<l1menu::ISample> l1menu::tools::loadSample( const std::string& filename )
{
	// Open the file, read enough of the start to determine what kind of file
	// it is, then close it.
	std::ifstream inputFile( filename, std::ios_base::binary );
	if( !inputFile.is_open() ) throw std::runtime_error( "The file does not exist or could not be opened" );

	// Look at the first few characters and see if they match some of the file formats
	const size_t bufferSize=20;
	char buffer[bufferSize];
	inputFile.get( buffer, bufferSize );
	inputFile.close();

	if( std::string(buffer)=="l1menuReducedSample" ) return std::unique_ptr<l1menu::ISample>( new l1menu::ReducedSample(filename) );
	else
	{
		// If it's not a ReducedSample then the only other ISample implementation at the
		// moment is a FullSample.
		std::unique_ptr<l1menu::FullSample> pReturnValue( new l1menu::FullSample );

		if( std::string(buffer).substr(0,4)=="root" )
		{
			// File is a root file, so assume it is one of the L1 DPG ntuples and try and load it
			// into the FullSample.
			pReturnValue->loadFile( filename );
			return std::unique_ptr<l1menu::ISample>( pReturnValue.release() );
		}
		else
		{
			// Assume the file is a list of filenames of L1 DPG ntuples.
			// TODO Do some checking to see if the characters I've read so far are valid filepath characters.
			pReturnValue->loadFilesFromList( filename );
			return std::unique_ptr<l1menu::ISample>( pReturnValue.release() );
		}
	}
}

std::pair<float,float> l1menu::tools::simpleLinearFit( const std::vector< std::pair<float,float> >& dataPoints )
{
	if( dataPoints.size()<2 ) throw std::runtime_error( "l1menu::tools::simpleLinearFit(...) requires at least two points to work" );

	float xyBar=0;
	float xBar=0;
	float yBar=0;
	float xSquaredBar=0;

	for( const auto& coords : dataPoints )
	{
		xyBar+=(coords.first*coords.second);
		xBar+=coords.first;
		yBar+=coords.second;
		xSquaredBar+=(coords.first*coords.first);
	}
	xyBar/=dataPoints.size();
	xBar/=dataPoints.size();
	yBar/=dataPoints.size();
	xSquaredBar/=dataPoints.size();

	float slope=(xyBar-xBar*yBar)/(xSquaredBar-xBar*xBar);
	float intercept=yBar-slope*xBar;

	return std::make_pair( slope, intercept );
}
